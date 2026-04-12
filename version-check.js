/* Grinffindor – live update checker
 * APP_VERSION placeholder is replaced at Docker build time via sed.
 * The script polls /version.json every 5 minutes and shows a banner
 * when the deployed version differs from the one loaded by this page.
 */
(function () {
    var CURRENT = '__APP_VERSION__';

    function check() {
        fetch('/version.json?t=' + Date.now(), { cache: 'no-store' })
            .then(function (r) {
                if (!r.ok) return;
                return r.json();
            })
            .then(function (d) {
                if (!d || !d.version || d.version === CURRENT) return;
                var b = document.getElementById('grinffindor-update-banner');
                if (b) return;
                b = document.createElement('div');
                b.id = 'grinffindor-update-banner';
                b.style.cssText = [
                    'position:fixed', 'top:0', 'left:0', 'right:0',
                    'padding:10px 20px', 'background:#f59e0b', 'color:#1a1a1a',
                    'text-align:center', 'z-index:9999', 'cursor:pointer',
                    'font-weight:bold', 'font-family:sans-serif', 'font-size:14px',
                    'box-shadow:0 2px 8px rgba(0,0,0,0.4)'
                ].join(';');
                b.textContent =
                    'New version ' + d.version + ' is available \u2014 click here to reload';
                b.onclick = function () { location.reload(true); };
                document.body.appendChild(b);
            })
            .catch(function () { /* silent: offline or server error */ });
    }

    /* First check 5 s after page load, then every 5 minutes */
    setTimeout(check, 5000);
    setInterval(check, 5 * 60 * 1000);
}());
