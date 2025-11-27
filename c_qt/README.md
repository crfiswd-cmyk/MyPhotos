# MyPhotos (C++/Qt Quick)

Prototype of the faster C++/Qt pipeline outlined earlier:

- libvips (optional) for decode/scale; falls back to Qt image reader.
- QtConcurrent thread pool for async decode + prefetch.
- LRU memory cache + disk thumbnail cache (`~/.cache/myphotos/thumbs` on Unix).
- Qt Quick + GPU textures for rendering/zooming.
- Async image provider for thumbnails/full images (`image://thumbs/...`).

## Build (macOS example, Qt 6.5+)
```bash
cd c_qt
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/macos \
      -DENABLE_VIPS=ON
cmake --build build
open build/MyPhotosCpp.app   # or ./build/MyPhotosCpp on Linux/Windows
```

Notes:
- Requires Qt Quick Controls 2 module (included in standard Qt install). If you see `QQuickStyle` not found, ensure `QuickControls2` is installed and `CMAKE_PREFIX_PATH` points to that Qt version.
- libvips is optional; if not found, build still succeeds using Qt decode. Install via Homebrew: `brew install vips`.
- You can pass your Qt install path via `CMAKE_PREFIX_PATH` (e.g. `/Users/you/Qt/6.6.0/macos`).
- CMake auto-bundles on macOS; on Linux/Windows, run the built binary from the build dir or package with windeployqt/macdeployqt if desired.

## Run with plugin diagnostics (macOS)
If launch fails due to missing Qt plugins, run from terminal with explicit paths and verbose logging:
```bash
cd c_qt
DYLD_FRAMEWORK_PATH="$(brew --prefix qt)/lib" \
QT_PLUGIN_PATH="$(brew --prefix qt)/plugins" \
QT_MESSAGE_PATTERN="[%{type}] %{time h:mm:ss.zzz} %{category}: %{message}" \
QT_DEBUG_PLUGINS=1 \
./build/MyPhotosCpp.app/Contents/MacOS/MyPhotosCpp
```
If Gatekeeper blocks the bundle, clear quarantine: `xattr -d com.apple.quarantine build/MyPhotosCpp.app`.

## How it works
- `ImageDecoder`: chooses libvips (if available) to decode + scale; else Qt.
- `ThumbCache`: in-memory LRU (count + byte budget) + PNG disk cache keyed by path+size.
- `ThumbProvider`: `QQuickAsyncImageProvider` that loads via thread pool, uses cache, supports `full/...` for originals; `prefetchAround` warms cache for neighbor items.
- `ImageListModel`: watches a folder, exposes paths/filenames to QML.
- `Main.qml`: simple Qt Quick UI (grid thumbnails + right-side preview); uses `image://thumbs/<edge>/<path>` sources; prefetches neighbor thumbs; flickable full image.

## Next steps
- Add gestures (pinch/pan inertia), HDR/RAW decode plugins, HEIC hardware decode where available.
- Add visible-range aware priority loading (based on GridView contentY/height).
- Integrate directory chooser and persistent settings; add multi-monitor and EXIF orientation handling in the viewer shader.
