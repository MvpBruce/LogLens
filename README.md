# LogLens

A fast **Qt desktop log viewer / analyzer** for large log files.

- Virtualized **Model/View** table (`QAbstractTableModel` + `QSortFilterProxyModel`)
- Substring + **regex** filtering, per-severity row coloring
- **Background parsing** on a worker thread — the UI never freezes
- Live **tail -f** via `QFileSystemWatcher`
- Window/session state persisted with `QSettings`

> Tech: C++17 · Qt 6 Widgets · CMake

<!-- Record with ScreenToGif: drag a large .log in (instant open) -> type a
     filter -> toggle regex -> show live tail. 5-12s, <5MB, ~800px wide.
     Uncomment once docs/demo.gif exists:
![LogLens demo](docs/demo.gif)
-->

## Prerequisites

A Qt 6 SDK (MSVC build). Fastest install without a Qt account, using
[aqtinstall](https://github.com/miurahr/aqtinstall):

```sh
pip install aqtinstall
# Installs Qt 6.8.1 for MSVC 2022 x64 into ./Qt
aqt install-qt windows desktop 6.8.1 win64_msvc2022_64 --outputdir C:/Qt
```

(Or use the official Qt Online Installer.)

## Build (Windows / MSVC)

```sh
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
      -DCMAKE_PREFIX_PATH=C:/Qt/6.8.1/msvc2022_64
cmake --build build --config Debug
./build/Debug/LogLens.exe            # or: LogLens.exe some.log
```

If Qt's DLLs aren't found at runtime, either add
`C:/Qt/6.8.1/msvc2022_64/bin` to `PATH` or run `windeployqt` on the exe.

## Roadmap

| Milestone | Scope |
|-----------|-------|
| **D1-D4** ✅ | CMake + Qt skeleton, open file, `QAbstractTableModel` table, severity coloring, drag-drop, QSettings last dir |
| **D5**      | Filter bar: substring + regex + level checkboxes via `QSortFilterProxyModel`; coloring delegate |
| **D6**      | Background parsing on a worker thread + progress bar |
| **D7**      | Live tail via `QFileSystemWatcher` |
| **D8**      | Search highlight, export filtered view, README GIF, polish |
