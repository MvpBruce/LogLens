# LogLens

A fast **Qt desktop log viewer / analyzer** for large log files.

- Virtualized **Model/View** table (`QAbstractTableModel` + a custom proxy)
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

## Architecture

LogLens follows Qt's **Model/View** separation: the window handles UI and
interaction, the model owns the data and decides how it is presented, and the
view renders only the rows currently visible (virtualized) so huge files stay
responsive.

### Classes

```mermaid
classDiagram
    class QMainWindow
    class QAbstractTableModel
    class QAbstractProxyModel
    class QStyledItemDelegate

    class MainWindow {
        +openPath(path) void
        -onOpen() void
        -buildFilterBar() void
        -m_model : LogModel*
        -m_proxy : LogFilterProxy*
        -m_view : QTableView*
    }
    class LogModel {
        +loadFile(path) bool
        +data(index, role) QVariant
        +detectLevel(line)$ Level
        -m_entries : QVector~Entry~
    }
    class LogFilterProxy {
        +setQuery(text) void
        +setUseRegex(on) void
        +setLevelEnabled(level, on) void
        #filterAcceptsRow(row, parent) bool
    }
    class LogDelegate {
        #initStyleOption(opt, index) void
    }

    QMainWindow <|-- MainWindow
    QAbstractTableModel <|-- LogModel
    QAbstractProxyModel <|-- LogFilterProxy
    QStyledItemDelegate <|-- LogDelegate
    MainWindow o--> LogModel : owns
    MainWindow o--> LogFilterProxy : owns
    LogFilterProxy --> LogModel : source model
    QTableView ..> LogFilterProxy : queries via proxy
    LogDelegate ..> LogModel : reads LevelRole
```

### Opening a file

```mermaid
sequenceDiagram
    actor User
    participant MW as MainWindow
    participant M as LogModel
    participant V as QTableView

    User->>MW: drop file / File > Open
    MW->>M: loadFile(path)
    activate M
    M->>M: beginResetModel()
    M->>M: read lines + detectLevel()
    M->>M: endResetModel()
    M-->>V: modelReset signal
    deactivate M
    V->>M: rowCount()
    loop visible rows only
        V->>M: data(index, role)
        M-->>V: text + severity color
    end
    MW->>MW: status bar: "N lines"
```

### Data flow (current + planned)

New features slot in as a layer — the core `LogModel` barely changes.

```mermaid
flowchart LR
    F[Log file] --> M[LogModel]
    M --> P[LogFilterProxy]
    P --> V[QTableView]
    D[LogDelegate<br/>severity color] -. paints .-> V

    W[Worker thread]:::planned
    FW[QFileSystemWatcher]:::planned

    W -. "D6: parse off the UI thread" .-> M
    FW -. "D7: live tail" .-> M

    classDef planned stroke-dasharray: 5 5,stroke:#888;
```

D5 added the solid `LogFilterProxy` (level + substring/regex filtering) and
`LogDelegate` (coloring), keeping `LogModel` a pure data container. D6/D7 remain
planned (dashed).

### Design note: why a custom proxy instead of `QSortFilterProxyModel`

The obvious choice for filtering is `QSortFilterProxyModel`, and that's what the
first cut used. It froze the UI: on a 500k-line log, unchecking a severity hides
tens of thousands of *scattered* rows, and the proxy filters **incrementally** —
emitting a `rowsRemoved` range per contiguous block. The view then processes tens
of thousands of removal signals on the UI thread, which reads as a hang.

`LogFilterProxy` subclasses `QAbstractProxyModel` instead and filters by
**rebuild-and-reset**: one O(N) scan builds a compact `visible source rows`
vector, published with a single `beginResetModel`/`endResetModel`. One signal,
not thousands. A parallel `source→proxy` vector keeps selection and
`dataChanged` mapping O(1). Text filtering is debounced (150 ms) so typing never
triggers a per-keystroke rescan.
