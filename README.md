# ISE Project - Point of Sale (PoS V2) & Barcode Reader/Scanner

A C++17 desktop Point of Sale application (Qt 6, OpenCV, ZXing-C++, zint) that generates and scans 6-digit Code 39 barcodes, manages product categories and inventory, records and edits orders, applies seasonal and special discounts, prints PDF bills, and generates six manager reports.

---

## Step 0: Build Setup (Windows, do this once)

All libraries (Qt, OpenCV, ZXing-C++, zint) are installed automatically by vcpkg, which is included in this repo. You do NOT download or install any libraries yourself.

### Requirements
- Windows 64-bit
- Visual Studio 2022 (fully updated) with the workload **"Desktop development with C++"**
- Git and CMake 3.20 or newer, both on `PATH`
- About 10 GB of free disk space
- Clone into a SHORT path such as `D:\dev` (long paths break the Qt build)

### Check your compiler
Open **"x64 Native Tools Command Prompt for VS 2022"** (use this prompt for every command below) and run:

    cl

It must say `Version 19.44` and `for x64`. If it does not, update Visual Studio 2022 through the Visual Studio Installer.

### Clean Build & Run Steps
1. Download the prebuilt library cache (if provided by the team) and unzip into `D:\vcpkg-cache`, then set:

       setx VCPKG_DEFAULT_BINARY_CACHE D:\vcpkg-cache

   Close and reopen the command prompt after setting the variable.

2. Clone with submodules (`--recursive` is required):

       cd /d D:\dev
       git clone --recursive https://github.com/HaibaNaeem/ISE-Project-Barcode-Reader-Scanner.git
       cd ISE-Project-Barcode-Reader-Scanner

3. Configure and build in Release mode:

       cmake --preset default
       cmake --build --preset release

4. Run all automated tests (everything except `test_camera`, which requires a physical webcam):

       ctest --test-dir build -C Release --output-on-failure

5. Run the manual camera test and desktop application:

       build\Release\test_camera.exe
       build\Release\BarcodeDesktopApp.exe

---

## Important: Upgrading from Increment 2

> **Delete the Increment 2 `data/` folder before the first run of Increment 3!**
>
> `Product.txt` (added `categoryId`) and `Bill.txt` (changed to one row per bill + `BillDetail.txt`) have new formats in Increment 3. Close the app and delete `build\Release\data` (and `build\Debug\data` if present) before launching `BarcodeDesktopApp.exe`.

On first run, `DataPaths::ensureDataFolder()` automatically creates `build\Release\data\` and copies missing files from `sample_data/`:
- `Product.txt`, `PCategory.txt`, `Order.txt`, `OrderDetail.txt`, `Bill.txt`, `BillDetail.txt`, `Seasons.txt`, `Settings.txt`
- Generated PDF bills are written to `build\Release\data\bills\Bill_<id>.pdf`
- Generated product barcode images are saved to `build\Release\data\barcodes\<barcode>.bmp`

Never commit `build/`, `output/`, `data/`, or generated bill PDFs.

---

## Architecture & Project Rules
- `src/services/`: Barcode generation, image decoding, camera scanning, validation, `CommonTypes.h`.
- `src/core/`: Business logic and file persistence (pure C++17, **no Qt**). All money is `int64_t` paisa; percentages are hundredths of a percent (`10.00% = 1000`).
- `src/gui/`: Qt 6 Widgets desktop UI (only `src/gui` uses Qt; `BillPdfWriter` lives here because it uses `QPdfWriter`).
- `tests/`: Standalone CTest executables (`test_*.cpp`). New `.cpp` files in `src/core/`, `src/gui/`, or `tests/` are picked up automatically after re-running `cmake --preset default`.
