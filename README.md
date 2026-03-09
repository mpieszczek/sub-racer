-----------------------------------

_DISCLAIMER:_Welcome to **Sub-Racer**!

This is a subtitle editor for videos built with raylib and libmpv.

-----------------------------------

## Getting Started

### Prerequisites

- CMake 3.24+
- GCC/MinGW (for Windows build)
- libmpv (LGPL) - for video playback

### Building

```sh
cmake -S . -B build
cmake --build build
```

Run the executable from `build/sub-racer/sub-racer.exe`

## Sub-Racer

Subtitle editor for videos.

### Description

A desktop application for adding subtitles to videos with timeline editing capabilities.

### Features

- Video playback with libmpv
- Timeline-based subtitle editing
- SRT export
- Subtitle burning (via ffmpeg)

### Controls

- Space: Play/Pause
- Left/Right Arrow: Seek
- Click on timeline: Jump to position
- Drag subtitle blocks: Shift timing

### License

This game sources are licensed under an unmodified zlib/libpng license, which is an OSI-certified, BSD-like license that allows static linking with closed source software. Check [LICENSE](LICENSE) for further details.

*Copyright (c) 2026*
ails.

$(Additional Licenses)

*Copyright (c) $(Year) $(User Name) ($(User Twitter/GitHub Name))*
