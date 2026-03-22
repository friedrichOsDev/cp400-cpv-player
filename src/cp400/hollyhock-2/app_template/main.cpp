#include <stdint.h>
#include <stddef.h>
#include <appdef.hpp>
#include <sdk/calc/calc.hpp>
#include <sdk/os/lcd.hpp>
#include <sdk/os/debug.hpp>
#include <sdk/os/mem.hpp>
#include <sdk/os/gui.hpp>
#include "cpv.hpp"

APP_NAME("CPV Player")
APP_DESCRIPTION("A simple video player using the custom .cpv format")
APP_AUTHOR("friedrichOsDev")
APP_VERSION("1.0.0")

extern "C"
int div(int n1, int n2) {
	if (n2 == 0) return 0;
	int quotient = 0;
	int absoluteN1 = n1 < 0 ? -n1 : n1;
	int absoluteN2 = n2 < 0 ? -n2 : n2;
	
	while (absoluteN1 >= absoluteN2) {
		absoluteN1 -= absoluteN2;
		quotient++;
	}
	
	if ((n1 < 0) ^ (n2 < 0)) {
		return -quotient;
	}
	return quotient;
}

class CPVLoader : public GUIDialog {
public:
	CPVLoader() : GUIDialog(
		GUIDialog::Height60,
		GUIDialog::AlignCenter,
		"Select CPV file",
		GUIDialog::KeyboardStateNone
	), cpvMenu(
		GetLeftX() + 10,
		GetTopY() + 10,
		GetRightX() - 10,
		GetBottomY() - 10 - 30 - 10,
		CPV_MENU_EVENT_ID
	), loadBtn(
		GetLeftX() + 10,
		GetBottomY() - 10 - 30,
		GetLeftX() + 10 + 100,
		GetBottomY() - 10,
		"Load",
		LOAD_BTN_EVENT_ID
	), cancelBtn(
		GetRightX() - 10 - 100,
		GetBottomY() - 10 - 30,
		GetRightX() - 10,
		GetBottomY() - 10,
		"Cancel",
		CANCEL_BTN_EVENT_ID
	) {
		selectedCPV = 0;

		CPVs::loadCPVList();
		CPVs::cpvlist_t cpvList = CPVs::getCPVList();

		for (int i = 0; i < cpvList.count; i++) {
			GUIDropDownMenuItem item(cpvList.cpvs[i].name, i + 1, GUIDropDownMenuItem::FlagEnabled | GUIDropDownMenuItem::FlagTextAlignLeft);
			cpvMenu.AddMenuItem(item);
		}
		
		cpvMenu.SetScrollBarVisibility(GUIDropDownMenu::ScrollBarVisibleWhenRequired);

		AddElement(cpvMenu);
		AddElement(loadBtn);
		AddElement(cancelBtn);
	}

	virtual ~CPVLoader() {}

	virtual int OnEvent(GUIDialog_Wrapped *dialog, GUIDialog_OnEvent_Data *event) {
        if (event->GetEventID() == CPV_MENU_EVENT_ID && (event->type & 0xF) == 0xD) {
            selectedCPV = event->data - 1;
        }

        return GUIDialog::OnEvent(dialog, event);
    }

	const CPVs::cpv_t * GetSelectedCPV() {
		CPVs::cpvlist_t cpvList = CPVs::getCPVList();
		return &cpvList.cpvs[selectedCPV];
	}

private:
	int selectedCPV;
	const uint16_t CPV_MENU_EVENT_ID = 1;
	GUIDropDownMenu cpvMenu;
	const uint16_t LOAD_BTN_EVENT_ID = GUIDialog::DialogResultOK;
	GUIButton loadBtn;
	const uint16_t CANCEL_BTN_EVENT_ID = GUIDialog::DialogResultCancel;
	GUIButton cancelBtn;
};

class CPVPlayer {
public:
	CPVPlayer(const CPVs::cpv_t * cpv) {
		LCD_GetSize(&LCD_WIDTH, &LCD_HEIGHT);
		header = (CPVs::CPV_Header *)malloc(sizeof(CPVs::CPV_Header));
		CPVs::loadCPVHeader(header, cpv);
		palette = (CPVs::CPV_ColorPalette *)malloc(sizeof(CPVs::CPV_ColorPalette));
		CPVs::loadCPVPalette(palette);
		frameHeader = (CPVs::CPV_FrameHeader *)malloc(sizeof(CPVs::CPV_FrameHeader));
		frameData = (uint8_t *)malloc((sizeof(uint16_t) + sizeof(uint8_t)) * header->width * header->height);
		calcVideoLayout();
	}

	~CPVPlayer() {
		free(header);
		free(palette);
		free(frameHeader);
		free(frameData);
	}

	void ShowNextFrame() {
		if (CPVs::loadCPVFrameHeader(frameHeader) < 0) return;
		if (CPVs::loadCPVFrame(frameHeader, frameData) < 0) return;

		int vidW = header->width;
		int vidH = header->height;
		int x = 0;
		int y = 0;

		for (int i = 0; i < frameHeader->size; ) {
			uint8_t byte1 = frameData[i++];
			uint16_t runLength = 0;
			bool is_skip = byte1 & 0x80;
			
			if (byte1 & 0x40) { // need_2nd_byte
				runLength = ((uint16_t)(byte1 & 0x3F) << 8) | frameData[i++];
			} else {
				runLength = byte1 & 0x3F;
			}

			if (is_skip) {
				int remaining = runLength;
				while (remaining > 0) {
					int space = vidW - x;
					if (remaining < space) {
						x += remaining;
						remaining = 0;
					} else {
						remaining -= space;
						x = 0;
						y++;
					}
				}
			} else {
				uint8_t color_idx = frameData[i++];
				uint16_t color = palette->colors[color_idx];

				for (int j = 0; j < runLength; j++) {
					int drawX, drawY;
					if (rotation) {
						drawX = offsetX + (vidH - 1 - y) * scale;
						drawY = offsetY + x * scale;
					} else {
						drawX = offsetX + x * scale;
						drawY = offsetY + y * scale;
					}

					if (scale == 1) {
						LCD_SetPixel(drawX, drawY, color);
					} else {
						for (int sy = 0; sy < scale; sy++) {
							for (int sx = 0; sx < scale; sx++) {
								LCD_SetPixel(drawX + sx, drawY + sy, color);
							}
						}
					}
					x++;
					if (x >= vidW) {
						x = 0;
						y++;
					}
				}
			}
		}
		LCD_Refresh();
	}

private:
	CPVs::CPV_Header * header = NULL;
	CPVs::CPV_ColorPalette * palette = NULL;
	CPVs::CPV_FrameHeader * frameHeader = NULL;
	uint8_t * frameData = NULL;
	int scale;
	bool rotation;
	int offsetY;
	int offsetX;
	int LCD_WIDTH;
	int LCD_HEIGHT;

	void calcVideoLayout() {
		bool v_is_portrait = (header->height > header->width);
		bool l_is_portrait = (LCD_HEIGHT > LCD_WIDTH);

		rotation = (v_is_portrait != l_is_portrait);

		int v_w = rotation ? header->height : header->width;
		int v_h = rotation ? header->width : header->height;

		int scale_w = div(LCD_WIDTH, v_w);
		int scale_h = div(LCD_HEIGHT, v_h);

		scale = (scale_w < scale_h) ? scale_w : scale_h;
		scale -= 1;
		if (scale < 1) scale = 1;

		offsetX = div(LCD_WIDTH - (v_w * scale), 2);
		offsetY = div(LCD_HEIGHT - (v_h * scale), 2);
	}
};

extern "C"
const CPVs::cpv_t * getCPVDialog() {
	CPVLoader * loader = new CPVLoader();
	GUIDialog::DialogResult result = loader->ShowDialog();

	const CPVs::cpv_t * selection = nullptr;
	if (result != GUIDialog::DialogResultOK) {
		selection = nullptr;
	} else {
		selection = loader->GetSelectedCPV();
	}

	delete loader;
	return selection;
}

extern "C"
void main() {
	calcInit();

	const CPVs::cpv_t * selectedCPV = getCPVDialog();
	if (selectedCPV == nullptr) {
		CPVs::freeCPVList();
		calcEnd();
		return;
	}

	fillScreen(0x0000);
	
	CPVPlayer * player = new CPVPlayer(selectedCPV);

	uint32_t key1, key2;
	bool quit = false;
	while (true) {
		player->ShowNextFrame();
		for (int i = 0; i < 100000; i++) {
			getKey(&key1, &key2);
			if (testKey(key1, key2, KEY_CLEAR)) {
				quit = true;
			} else if (testKey(key1, key2, KEY_SHIFT)) {
				break;
			}
		}
		if (quit) break;
	}

	CPVs::freeCPVList();
	delete player;
	calcEnd();
}
