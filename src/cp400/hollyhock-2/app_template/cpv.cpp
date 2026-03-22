#include <sdk/os/file.hpp>
#include <sdk/os/mem.hpp>
#include "cpv.hpp"

class File {
public:
	int open(const char *path, int flags) {
		if (m_opened) ::close(m_fd);
		m_fd = ::open(path, flags);
		m_opened = (m_fd >= 0);
		return m_fd;
	}

	int getAddr(int offset, const void **addr) {
		return ::getAddr(m_fd, offset, addr);
	}

    int getSize(uint32_t *size) {
        struct stat stat_buffer;
		int ret = fstat(m_fd, &stat_buffer);
		if (ret < 0) return ret;

        *size = stat_buffer.fileSize;
        return ret;
    }

	int read(void *buf, int count) {
		return ::read(m_fd, buf, count);
	}

	int seek(int offset, int whence) {
		return ::lseek(m_fd, offset, whence);
	}

private:
	bool m_opened;
	int m_fd;
};

class Find {
public:
	Find() : m_opened(false), m_findHandle(-1) {

	}

	~Find() {
		if (m_opened) {
			findClose(m_findHandle);
		}
	}

	int findFirst(const wchar_t *path, wchar_t *name, struct findInfo *findInfoBuf) {
		int ret = ::findFirst(path, &m_findHandle, name, findInfoBuf);
		m_opened = true;
		return ret;
	}

	int findNext(wchar_t *name, struct findInfo *findInfoBuf) {
		return ::findNext(m_findHandle, name, findInfoBuf);
	}

private:
	bool m_opened;
	int m_findHandle;
};

namespace CPVs {
    const char CPV_FOLDER[] = "\\fls0\\cpv\\";
    const char FILE_MASK[] = "*.cpv";

    cpvlist_t cpvList;
    int currentPos = 0;
    File file;

    void loadCPVList() {
        cpvList.cpvs = (cpv_t *)malloc(sizeof(cpv_t) * MAX_CPVS);
        cpvList.count = 0;

        Find finder;
        wchar_t wName[256];
        wchar_t wPath[256];
        struct findInfo info;

        int i = 0;
        while (CPV_FOLDER[i] != 0) {
            wPath[i] = (wchar_t)CPV_FOLDER[i];
            i++;
        }
        int j = 0;
        while (FILE_MASK[j] != 0) {
            wPath[i] = (wchar_t)FILE_MASK[j];
            i++; j++;
        }
        wPath[i] = 0;

        int ret = finder.findFirst(wPath, wName, &info);
        while (ret >= 0) {
            if (info.type == info.EntryTypeFile) {
                cpv_t *currentCPV = &cpvList.cpvs[cpvList.count];

                int k = 0;
                while (wName[k] != 0 && k < 255) {
                    currentCPV->name[k] = (char)wName[k];
                    k++;
                }
                currentCPV->name[k] = 0;

                int folderLen = 0;
                while (CPV_FOLDER[folderLen] != 0 && folderLen < 255) {
                    currentCPV->path[folderLen] = CPV_FOLDER[folderLen];
                    folderLen++;
                }

                int nameIdx = 0;
                while (currentCPV->name[nameIdx] != 0 && (folderLen + nameIdx) < 255) {
                    currentCPV->path[folderLen + nameIdx] = currentCPV->name[nameIdx];
                    nameIdx++;
                }
                currentCPV->path[folderLen + nameIdx] = 0;

                cpvList.count++;
                if (cpvList.count >= MAX_CPVS) break;
            }
            ret = finder.findNext(wName, &info);
        }
    }

    void freeCPVList() {
        if (cpvList.cpvs != nullptr) {
            free(cpvList.cpvs);
            cpvList.cpvs = nullptr;
        }
        cpvList.count = 0;
    }

    cpvlist_t getCPVList() {
        return cpvList;
    }

    int loadCPVHeader(CPV_Header * header, const cpv_t * cpv) {
        int ret = file.open(cpv->path, OPEN_READ);
        if (ret < 0) return ret;

        return file.read(header, sizeof(CPV_Header));
        
    }

    int loadCPVPalette(CPV_ColorPalette * palette) {
        return file.read(palette, sizeof(CPV_ColorPalette));
    }

    int loadCPVFrameHeader(CPV_FrameHeader * header) {
        return file.read(header, sizeof(CPV_FrameHeader));
       
    }

    int loadCPVFrame(CPV_FrameHeader * header, uint8_t * data) {
        return file.read(data, header->size);
    }
}