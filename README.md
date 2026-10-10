
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

The GitHub Actions Windows job now publishes both the deployed app directory and a `.msi` installer artifact built from that deployment tree.

## MWixnet Testnet

The MWixnet tile polls `https://mwixnet.grinffindor.org/v1` every 5 seconds.
The fixed, read-only proxy response contains swap-server status, round statistics,
server information, connections, live activity and up to 32 on-chain kernel observations.
No endpoint configuration is needed in the app.

Live counters show unprocessed inputs, inputs in the current round and transactions
awaiting confirmation. The phase bar highlights the current step and steps completed
within the last 20 seconds. Aggregate round totals cover completed five-minute windows
since server start. They are not individual swap counts.

Connections describe this server and its configured next mixer, not a complete route
or mixer health. The CLI example uses the returned server information and a verified
public onion key for the known Testnet mixer.

Opening a kernel queries `get_kernel` and `get_block` through the existing node API at
`https://testnet.grinffindor.org/v2/foreign`. Block details are rechecked when the chain
observation changes. Click a field to copy its full value, including large integers.
Block inputs and outputs are not attributed to individual swap requests.

Status requests time out after 10 seconds and expire after 45 seconds. Block lookups
have a 15-second timeout. Only Testnet status responses are accepted.

For WebAssembly, the endpoint is allowed by `nginx.conf.template`. The proxy
allows browser requests from `grinffindor.org` and `www.grinffindor.org`. The proxy permits only GET requests and returns a fixed
status batch. It must never expose the existing `swap` or `mix` methods.
Deployment configuration lives in the private infrastructure repository.

Run the tile tests with `qmltestrunner -input tests/qml` (a software backend can
be selected with `QT_QUICK_BACKEND=software`). These tests use a fake block provider
and disable polling. The node response tests use only a loopback HTTP server:

```sh
mkdir -p build/nodeblock-tests
cd build/nodeblock-tests
qmake ../../tests/nodeblocktests.pro
make
./nodeblocktests
```

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
GitHub Actions builds WebAssembly from source using pinned Qt and Emscripten versions, then packages those artifacts into the Docker image. Deployment does not require a local Qt build. The Dockerfile is the packaging stage, not the compiler.

For local development only:

1. Build the project with the Qt WebAssembly single-threaded kit.
2. Ensure the generated output exists at `build/WebAssembly_Qt_6_10_1_single_threaded-Release`.
3. Start the container stack:

```sh
docker compose up --build -d
```

4. Open `http://localhost:8084`.

On GitHub Actions, the `WebAssembly` job builds the container from `Dockerfile`. Pull requests validate the image build, and pushes to `main` publish:
- `ghcr.io/<owner>/grinffindor:latest`
- `ghcr.io/<owner>/grinffindor:sha-<commit>`
- optionally `docker.io/<DOCKERHUB_USERNAME>/grinffindor:*` when `DOCKERHUB_USERNAME` and `DOCKERHUB_TOKEN` are configured as repository secrets

The TestFlight job runs only after all platform builds, including WebAssembly, succeed. Missing required WASM artifacts fail the job.

## Docker Hub Image Build

Set `DOCKERHUB_USERNAME` and `DOCKERHUB_TOKEN` as repository secrets to publish
Docker Hub images from the same CI-built WASM artifacts as GHCR. Deploy an image
from a successful workflow by immutable digest; no local build or upload is
required. See the WebAssembly job's image tags and revision labels to identify
the source commit.

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
