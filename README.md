# MyPhotos

Two tracks live in this repo:

- `python_qt/`: the existing PySide6 viewer. Quick start: `cd python_qt && python3 -m pip install -r requirements.txt` then run `python3 main.py` (run from your image folder if you want it to auto-load that folder, or launch then click “选择文件夹”).
- `c_qt/`: C++/Qt Quick prototype with async image provider, libvips (optional), LRU + disk cache, and GPU-rendered UI. Build with CMake + Qt 6.5+ (see `c_qt/README.md`).
- Test images: run `python3 dowload_test_photos.py --count 100 --width 3840 --height 2160` to download random HD photos from picsum.photos into `test_image/` (ignored by git).
