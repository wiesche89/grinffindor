# Grinffindor

Qt Quick / QML application for desktop and WebAssembly, including a containerized `nginx` setup for serving the WASM build via Docker Compose.

## Project Layout
- `src/` contains the C++ entry point and node footer status bridge.
- `qml/` contains the application UI, tile pages, and translations.
- `qml.qrc` and `res.qrc` register UI files and assets for Qt.
- `Dockerfile`, `docker-compose.yaml`, `docker-entrypoint.sh`, and `nginx.conf.template` provide the web container setup.

## Local Qt Build
1. Install Qt 6 with Qt Quick, Qt Quick Controls 2, and the WebAssembly kit if you want a browser build.
2. Open `grinffindor.pro` in Qt Creator or run `qmake grinffindor.pro`.
3. Build with your target kit.

## WebAssembly + Docker
The Docker setup expects a prebuilt Qt WebAssembly output directory, analogous to `grin-node-docker`.

1. Build the project with the Qt WebAssembly single-threaded kit.
2. Ensure the generated output exists at `build/WebAssembly_Qt_6_10_1_single_threaded-Release`.
3. Start the container stack:

```sh
docker compose up --build -d
```

4. Open `http://localhost:8084`.

## Docker Notes
- `nginx` serves the static WASM files and applies the required COOP/COEP headers.
- `qml/translation` is copied into the container so JSON language files remain available.
- Logs are written to `./nginx-logs`.
- If your Qt WebAssembly output path differs, override `PREBUILT_DIR` in `docker-compose.yaml` or during build.
