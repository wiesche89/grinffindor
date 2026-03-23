ARG PREBUILT_DIR=build/WebAssembly_Qt_6_10_1_single_threaded-Release

FROM nginx:alpine

ARG PREBUILT_DIR

WORKDIR /usr/share/nginx/html

RUN apk add --no-cache gettext

ENV APP_INDEX=grinffindor.html

COPY nginx.conf.template /etc/nginx/conf.d/default.conf.template
COPY ${PREBUILT_DIR}/grinffindor.html ./
COPY ${PREBUILT_DIR}/grinffindor.js ./
COPY ${PREBUILT_DIR}/grinffindor.wasm ./
COPY ${PREBUILT_DIR}/qtloader.js ./
COPY ${PREBUILT_DIR}/qtlogo.svg ./
COPY media ./media
COPY qml/translation ./translation
COPY media/images/Image_1_logo.PNG ./Image_1_logo.PNG
COPY docker-entrypoint.sh /usr/local/bin/docker-entrypoint.sh

RUN sed -i 's/qtlogo\.svg/Image_1_logo.PNG/g' ./grinffindor.html \
    && sed -i '0,/<strong>.*<\/strong>/s//<strong>Loading Grinffindor...<\/strong>/' ./grinffindor.html \
    && gzip -kf -9 ./grinffindor.js \
    && gzip -kf -9 ./grinffindor.wasm \
    && gzip -kf -9 ./qtloader.js \
    && chmod +x /usr/local/bin/docker-entrypoint.sh

EXPOSE 80
ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]
