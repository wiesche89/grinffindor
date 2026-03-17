#!/bin/sh
set -eu

if [ -z "${APP_INDEX:-}" ]; then
    APP_INDEX="grinffindor.html"
fi

export APP_INDEX

envsubst '${APP_INDEX}' \
    < /etc/nginx/conf.d/default.conf.template \
    > /etc/nginx/conf.d/default.conf

exec nginx -g 'daemon off;'
