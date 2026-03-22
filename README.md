# CPV Player

This is a simple video player for the CASIO FX-CP400 using the custom .cpv format.

## Installation

1. Install hollyhock-2 from [SnailMath](https://github.com/SnailMath/hollyhock-2)
2. Copy the .bin to `/fls0/cpv_player.bin`
3. Place CPV files in `/fls0/cpv/`. Ensure filenames are simple (e.g., `video.cpv`) and avoid multiple dots to maintain filesystem compatibility.

## Usage

- Convert videos using `./cpv_convert <file>` (requires `ffmpeg`). Preview created cpv files using `./cpv_player video.cpv`.
- Launch `cpv_player.bin` via hollyhock-2.
- Select a video from the dropdown and press **Load**.
- **[SHIFT]**: Fast forward.
- **[CLEAR]**: Exit playback.
