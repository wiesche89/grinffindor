# ── Build arguments ──────────────────────────────────────────────────────────
# PREBUILT_DIR  path to the Qt WASM release output inside the build context
# APP_VERSION   override the version string; if empty it is read from
#               grinffindor.pro (VERSION = x.y.z) at build time
ARG PREBUILT_DIR=build/WebAssembly_Qt_6_10_1_single_threaded-Release
ARG APP_VERSION=""

FROM nginx:alpine

ARG PREBUILT_DIR
ARG APP_VERSION

WORKDIR /usr/share/nginx/html

RUN apk add --no-cache gettext

ENV APP_INDEX=grinffindor.html

# ── nginx template & entrypoint ──────────────────────────────────────────────
COPY nginx.conf.template     /etc/nginx/conf.d/default.conf.template
COPY docker-entrypoint.sh    /usr/local/bin/docker-entrypoint.sh

# ── Qt WASM build artefacts ──────────────────────────────────────────────────
COPY ${PREBUILT_DIR}/grinffindor.html ./
COPY ${PREBUILT_DIR}/grinffindor.js   ./
COPY ${PREBUILT_DIR}/grinffindor.wasm ./
COPY ${PREBUILT_DIR}/qtloader.js      ./
COPY ${PREBUILT_DIR}/qtlogo.svg       ./

# ── Static assets ─────────────────────────────────────────────────────────────
COPY media                          ./media
COPY qml/translation                ./translation
COPY media/images/Image_1_logo.PNG  ./Image_1_logo.PNG

# ── Version source (single source of truth) ──────────────────────────────────
COPY grinffindor.pro ./grinffindor.pro

# ── Update-check browser script (placeholder replaced below) ─────────────────
COPY version-check.js ./

# ── Version injection & cache-busting ────────────────────────────────────────
# Version resolution order:
#   1. --build-arg APP_VERSION=x.y.z   (explicit override)
#   2. VERSION line in grinffindor.pro  (automatic, single source of truth)
#
# What this block does:
#   • Resolves $VER
#   • Patches grinffindor.html: logo, loading text, ?v= on script src tags
#   • Patches grinffindor.js:   ?v= on the wasm filename string literal
#   • Injects version-check.js before </body>
#   • Writes version.json  { "version": "x.y.z" }
#   • Pre-gzip-compresses all heavy static assets (served by gzip_static)
RUN set -e; \
    VER="${APP_VERSION}"; \
    if [ -z "$VER" ]; then \
        VER="$(grep -E '^VERSION\s*=' grinffindor.pro \
               | sed 's/^VERSION\s*=\s*//' \
               | tr -d ' \r\n')"; \
    fi; \
    [ -n "$VER" ] || { echo "ERROR: could not determine APP_VERSION" >&2; exit 1; }; \
    echo "Building Grinffindor v${VER}"; \
    \
    # ── HTML: custom tweaks (logo, loading text) ──────────────────────────── \
    sed -i 's/qtlogo\.svg/Image_1_logo.PNG/g' grinffindor.html; \
    sed -i '0,/<strong>.*<\/strong>/s//<strong>Loading Grinffindor...<\/strong>/' \
        grinffindor.html; \
    \
    # ── HTML: cache-bust script src references ────────────────────────────── \
    sed -i "s|src=\"qtloader\.js\"|src=\"qtloader.js?v=${VER}\"|g" \
        grinffindor.html; \
    sed -i "s|src=\"grinffindor\.js\"|src=\"grinffindor.js?v=${VER}\"|g" \
        grinffindor.html; \
    \
    # ── grinffindor.js: cache-bust internal wasm reference ────────────────── \
    # Qt 6 / Emscripten writes the filename as a string literal.            \
    # Both quoting styles are handled; adjust the pattern if your Qt       \
    # version uses a URL constructor instead of a bare string.             \
    sed -i "s|'grinffindor\.wasm'|'grinffindor.wasm?v=${VER}'|g" \
        grinffindor.js; \
    sed -i "s|\"grinffindor\.wasm\"|\"grinffindor.wasm?v=${VER}\"|g" \
        grinffindor.js; \
    \
    # ── Inject version-check script before </body> ───────────────────────── \
    sed -i "s|__APP_VERSION__|${VER}|g" version-check.js; \
    sed -i "s|</body>|<script src=\"version-check.js?v=${VER}\"></script>\n</body>|g" \
        grinffindor.html; \
    \
    # ── Write version manifest ────────────────────────────────────────────── \
    printf '{"version":"%s"}\n' "${VER}" > version.json; \
    \
    # ── Pre-compress heavy assets (nginx gzip_static will serve .gz) ─────── \
    gzip -kf -9 grinffindor.js; \
    gzip -kf -9 grinffindor.wasm; \
    gzip -kf -9 qtloader.js; \
    gzip -kf -9 version-check.js; \
    gzip -kf -9 version.json; \
    \
    # ── Cleanup ───────────────────────────────────────────────────────────── \
    rm -f grinffindor.pro; \
    chmod +x /usr/local/bin/docker-entrypoint.sh

EXPOSE 80
ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]
