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

## Android Build
The project now includes an Android package template with network permissions and cleartext support for custom node endpoints.

1. Install Qt 6 with the Android kit that matches your compiler ABI.
2. Install Android Studio components or the standalone Android SDK, NDK, OpenJDK, and platform tools.
3. In Qt Creator, configure the Android SDK, NDK, and JDK under the Qt settings.
4. Open `grinffindor.pro` and select an Android kit such as `Android Qt 6.x Clang arm64-v8a`.
5. Build the project.
6. For a device install, enable USB debugging on the phone and use the Run target in Qt Creator.
7. For an APK or AAB, use Qt Creator `Build` -> `Deploy` -> `Build Android APK` or `Build Android App Bundle`.

Notes:
- The manifest requests `INTERNET` because the wallet and node features call remote APIs.
- `usesCleartextTraffic="true"` is enabled so existing custom `http://` node endpoints continue to work on Android. If you want to enforce HTTPS only, tighten the manifest and the URL validation together.

## iPhone Build
The project now includes an iOS `Info.plist` template and runs the main window in fullscreen on native mobile platforms.

1. Build on macOS with Xcode installed. iPhone builds are not supported from Windows.
2. Install Qt 6 for iOS and register the kit in Qt Creator.
3. Open `grinffindor.pro` and select an iOS kit for simulator or device.
4. Build and run from Qt Creator, or open the generated Xcode project if you need signing adjustments.
5. Set your Apple development team and provisioning profile for device deployment.

Notes:
- The bundled `Info.plist` allows arbitrary network loads so user-configured `http://` node endpoints do not break under App Transport Security.
- For App Store delivery, replace placeholder bundle identifiers, versioning, icons, and launch assets with production values.

## WebAssembly + Docker
The Docker setup expects a prebuilt Qt WebAssembly output directory, analogous to `grin-node-docker`.

1. Build the project with the Qt WebAssembly single-threaded kit.
2. Ensure the generated output exists at `build/WebAssembly_Qt_6_10_1_single_threaded-Release`.
3. Start the container stack:

```sh
docker compose up --build -d
```

4. Open `http://localhost:8084`.

## Docker Hub Image Build
Before building the container image, create the Qt WebAssembly output locally.

1. Build the project with the Qt WebAssembly single-threaded kit.
2. Ensure the generated output exists at `build/WebAssembly_Qt_6_10_1_single_threaded-Release`.
3. Log in to Docker Hub:

```sh
docker login
```

4. Build and push the multi-architecture image:

```sh
docker buildx build --no-cache --platform linux/amd64,linux/arm64 ^
  -t wiesche89/grinffindor:0.1.0 ^
  -f Dockerfile ^
  . ^
  --push
```

Notes:
- Adjust the tag, for example `0.1.1`, for new releases.
- The image contains the prebuilt WASM assets and is ready to run behind `nginx`.

## VM Installation on Proxmox
The recommended target is a small Ubuntu VM on Proxmox.

### 1. Prepare the VM
- Create an Ubuntu 24.04 VM.
- Give it a static IP or DHCP reservation.
- Open port `80` or your chosen published port in your firewall.

### 2. Install Docker Engine
Example for Ubuntu:

```sh
apt update && apt upgrade -y
apt install -y ca-certificates curl git
install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
chmod a+r /etc/apt/keyrings/docker.asc
echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/ubuntu $(. /etc/os-release && echo $UBUNTU_CODENAME) stable" > /etc/apt/sources.list.d/docker.list
apt update
apt install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
systemctl enable --now docker
```

### 3. Deploy the Container
Clone the repository on the VM:

```sh
git clone https://github.com/wiesche89/grinffindor.git
cd grinffindor
```

To update the checkout later:

```sh
cd grinffindor
git pull
```

If you want to run the already published Docker Hub image, use a compose file that points to the image tag and does not require a local build.

Example:

```yaml
services:
  grinffindor-web:
    image: wiesche89/grinffindor:0.1.0
    container_name: grinffindor
    restart: unless-stopped
    ports:
      - "8084:80"
    environment:
      TZ: "Europe/Berlin"
      APP_INDEX: "grinffindor.html"
    volumes:
      - ./nginx-logs:/var/log/nginx
```

Start it with:

```sh
docker compose up -d
```

### 4. Verify the Deployment
- Open `http://<vm-ip>:8084/grinffindor.html`
- Check container status:

```sh
docker compose ps
docker compose logs --tail=100
```

## Docker Notes
- `nginx` serves the static WASM files and applies the required COOP/COEP headers.
- `qml/translation` is copied into the container so JSON language files remain available.
- Logs are written to `./nginx-logs`.
- If your Qt WebAssembly output path differs, override `PREBUILT_DIR` in `docker-compose.yaml` or during build.
