# Grinffindor Template

A lightweight Qt Quick + QML starter that is ready for Desktop and WebAssembly targets.

## Layout
- `(src/)` contains the minimal C++ entry point that loads the QML engine.
- `(qml/)` hosts the main view `Main.qml` plus any additional QML modules you add.
- `(qml.qrc)` and `(res.qrc)` bind the UI and asset files to the Qt project.

## Setup
1. Install Qt 6 (or newer) to get `qmake` and the Qt Quick modules.
2. Run `qmake grinffindor.pro` or open the `.pro` file in Qt Creator.
3. Build with your kit (`mingw32-make`, `ninja`, `make`, etc.).
4. Tailor `qml/Main.qml` and the C++ entry point to your app requirements.

## Notes
- The `build/` directory and IDE metadata are ignored via `.gitignore`.
- WebAssembly signals are prepped through the `res.qrc` resource bundle.
- Drop icons, fonts, and media into `media/`, `fonts/`, or other folders and register them in the QRC files.
