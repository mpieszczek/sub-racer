# Welcome to **Sub-Racer**!

This is a subtitle editor for videos built with raylib and libmpv.
Build avilable on itch.io:
https://lewita.itch.io/subracer

## Getting Started

### Prerequisites

- CMake 3.24+
- GCC/MinGW (for Windows build)

### libmpv Setup

Before building, you need to obtain libmpv Windows build files:

1. Download libmpv release (mpv-dev or release build)
2. Extract and copy files to `external/libmpv/`:
   - `libmpv-2.dll` → `external/libmpv/`
   - `libmpv.dll.a` → `external/libmpv/`
   - `include/` folder → `external/libmpv/include/`
   - `LICENSE.LGPL` → `external/libmpv/LICENSE.LGPL`

You can download libmpv from: https://github.com/mpv-player/mpv/releases

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
- (IN-PROGRESS) Initialization of subtitles with automatic transcription
- (TO-BE-DONE) Subtitle burning (via ffmpeg)


### Controls

- Space: Play/Pause
- Left/Right Arrow: Seek
- Click on timeline: Jump to position
- "Plus" button bellow timestamps - set time to current frame
