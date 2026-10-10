import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    readonly property bool compact: width < 760
    readonly property int pageGutter: compact ? 16 : 28
    readonly property color panel: "#d80b1018"
    readonly property color border: "#406b7380"
    function assetPath(path) {
        return (typeof assetBaseUrl === "string" ? assetBaseUrl : "qrc:/res/") + path
    }
    property var i18n: null
    property var settingsStore: null
    signal backRequested()
    anchors.fill: parent

    property string endpoint: settingsStore ? settingsStore.mwixnetTestnetRpcUrl : ""
    property var statusData: null
    property var statsData: null
    property string errorText: ""
    property double receivedAt: 0
    property double now: Date.now()
    property var pending: null
    property int requestId: 0
    property bool loading: false
    readonly property bool fresh: receivedAt > 0 && now - receivedAt < 45000 && errorText === ""
    readonly property string phase: fresh && statusData ? statusData.phase : "unavailable"
    readonly property bool active: visible && fresh && ["validating", "mixing", "submitting", "checking_confirmations"].indexOf(phase) >= 0
    readonly property int stage: ["validating", "mixing", "submitting", "checking_confirmations"].indexOf(phase)

    function cancel() {
        requestId++
        timeout.stop()
        var xhr = pending
        pending = null
        loading = false
        if (xhr) xhr.abort()
    }

    function fail(message) {
        errorText = message
        statusData = null
        statsData = null
        receivedAt = 0
    }

    function validStatus(value) {
        return value && value.network === "testnet"
                && ["swap", "mixer"].indexOf(value.role) >= 0
                && ["idle", "validating", "mixing", "submitting", "checking_confirmations", "failed"].indexOf(value.phase) >= 0
                && typeof value.started_at === "number" && isFinite(value.started_at)
                && typeof value.observed_at === "number" && isFinite(value.observed_at)
                && value.observed_at >= value.started_at
    }

    function counter(value) {
        return typeof value === "number" && isFinite(value) && value >= 0 && Math.floor(value) === value
    }

    function applyResponses(payload) {
        if (!Array.isArray(payload) || payload.length !== 2) {
            fail(qsTr("Invalid RPC response."))
            return
        }
        var status = null
        var stats = null
        for (var i = 0; i < payload.length; i++) {
            var response = payload[i]
            if (!response || response.jsonrpc !== "2.0" || response.error) {
                fail(qsTr("MWixnet status API unavailable."))
                return
            }
            if (response.id === 1 && !status) status = response.result
            else if (response.id === 2 && !stats) stats = response.result
            else { fail(qsTr("Invalid RPC response.")); return }
        }
        if (!validStatus(status)) {
            fail(qsTr("Expected a MWixnet Testnet status response."))
            return
        }
        if (!stats || stats.started_at !== status.started_at || !counter(stats.completed)
                || !counter(stats.failed) || !counter(stats.published_through_uptime_s)
                || stats.window_s !== 300) {
            fail(qsTr("Invalid round statistics."))
            return
        }
        statusData = status
        statsData = stats
        errorText = ""
        receivedAt = Date.now()
        now = receivedAt
    }

    function validEndpoint(url) {
        return /^https:\/\/[^\s\/@?#]+(?:\/[^\s?#]*)?$/.test(url)
                || /^http:\/\/(?:127\.0\.0\.1|localhost)(?::[0-9]+)?\/[^\s?#]*$/.test(url)
    }

    function refresh() {
        if (!visible || loading || endpoint.trim() === "") return
        // Keep credentials out of the persisted endpoint and public RPC requests.
        if (!validEndpoint(endpoint)) {
            fail(qsTr("Use an HTTPS RPC URL, or HTTP on localhost for development."))
            return
        }
        loading = true
        var generation = ++requestId
        var xhr = new XMLHttpRequest()
        pending = xhr
        xhr.onreadystatechange = function() {
            if (generation !== requestId || xhr.readyState !== XMLHttpRequest.DONE) return
            timeout.stop()
            pending = null
            loading = false
            if (xhr.status !== 200) { fail(qsTr("RPC unreachable. Check the endpoint and browser access policy.")); return }
            try { applyResponses(JSON.parse(xhr.responseText)) }
            catch (e) { fail(qsTr("Invalid RPC response.")) }
        }
        try {
            xhr.open("POST", endpoint)
            xhr.setRequestHeader("Content-Type", "application/json")
            xhr.setRequestHeader("Cache-Control", "no-cache")
            timeout.start()
            xhr.send(JSON.stringify([
                {jsonrpc: "2.0", method: "get_status", params: [], id: 1},
                {jsonrpc: "2.0", method: "get_round_stats", params: [], id: 2}
            ]))
        } catch (e) {
            cancel()
            fail(qsTr("RPC request failed."))
        }
    }

    onEndpointChanged: {
        cancel()
        statusData = null
        statsData = null
        receivedAt = 0
        errorText = ""
        refresh()
    }
    onVisibleChanged: { if (visible) refresh(); else cancel() }
    Component.onCompleted: refresh()
    Component.onDestruction: cancel()

    Timer { id: timeout; interval: 10000; onTriggered: { root.cancel(); root.fail(qsTr("RPC request timed out.")) } }
    Timer { interval: 15000; running: root.visible && root.endpoint !== ""; repeat: true; onTriggered: root.refresh() }
    Timer { interval: 1000; running: root.visible; repeat: true; onTriggered: root.now = Date.now() }

    Rectangle { anchors.fill: parent; color: "#05070b" }
    Image {
        anchors.fill: parent
        source: root.assetPath("media/images/image_wallpaper_tile.png")
        fillMode: Image.PreserveAspectCrop
        smooth: true
        asynchronous: true
    }
    Rectangle { anchors.fill: parent; color: "#77080d14" }

    Rectangle {
        id: topBar
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: root.compact ? 64 : 76
        color: root.panel
        border.color: root.border
        z: 10
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: root.pageGutter
            anchors.rightMargin: root.pageGutter
            spacing: 12
            Button {
                id: backButton
                text: root.i18n ? root.i18n.tf("back", "Back") : qsTr("Back")
                implicitWidth: root.compact ? 84 : 104
                implicitHeight: root.compact ? 38 : 42
                background: Rectangle {
                    radius: 14
                    color: backButton.hovered ? "#18ffffff" : "#0dffffff"
                    border.color: root.border
                }
                contentItem: Label {
                    text: backButton.text
                    color: "#f4f7ff"
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: root.backRequested()
            }
            Label {
                text: "MWixnet · Testnet"
                color: "#ffffff"
                font.pixelSize: root.compact ? 20 : 26
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
    }
    ScrollView {
        id: scroll
        anchors { top: topBar.bottom; left: parent.left; right: parent.right; bottom: parent.bottom }
        anchors.margins: root.pageGutter
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            width: scroll.availableWidth
            spacing: 22
            Label {
                Layout.fillWidth: true
                text: qsTr("Live server activity on Grin Testnet")
                color: "#a9b4c2"; font.pixelSize: 16; wrapMode: Text.Wrap
            }
            Pane {
                Layout.fillWidth: true
                padding: 20
                background: Rectangle { color: root.panel; radius: 14; border.color: root.border }
                contentItem: ColumnLayout {
                    spacing: 24
                    Label {
                        Layout.fillWidth: true
                        text: !root.fresh ? qsTr("No current data")
                              : root.phase === "failed" ? qsTr("Last operation failed")
                              : root.phase === "idle" ? qsTr("Idle · waiting for work")
                              : root.phase === "validating" ? qsTr("Checking eligible inputs")
                              : root.phase === "mixing" ? qsTr("Mixing")
                              : root.phase === "submitting" ? qsTr("Submitting transaction")
                              : qsTr("Checking pending transactions")
                        color: root.phase === "failed" ? "#f4aa80" : "#f4f6f8"
                        font.pixelSize: 21; wrapMode: Text.Wrap
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Repeater {
                            model: [qsTr("Validate"), qsTr("Mix"), qsTr("Submit"), qsTr("Check chain")]
                            ColumnLayout {
                                id: step
                                required property int index
                                required property string modelData
                                Layout.fillWidth: true
                                Layout.preferredWidth: 1
                                readonly property bool selected: root.active && root.stage === index
                                Rectangle {
                                    id: dot
                                    Layout.alignment: Qt.AlignHCenter
                                    width: 26; height: 26; radius: 13
                                    color: step.selected ? "#FEF102" : "#344356"
                                    SequentialAnimation on opacity {
                                        running: step.selected; loops: Animation.Infinite
                                        NumberAnimation { to: 0.35; duration: 650 }
                                        NumberAnimation { to: 1; duration: 650 }
                                        onRunningChanged: if (!running) dot.opacity = 1
                                    }
                                }
                                Label { text: step.modelData; color: step.selected ? "#FEF102" : "#a9b4c2"; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; font.pixelSize: 13 }
                            }
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Animation shows the sampled server phase, not individual swaps or confirmation progress.")
                        color: "#a9b4c2"; wrapMode: Text.Wrap
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                text: root.fresh && root.statusData && root.statsData
                      ? (root.statusData.role === "swap" ? qsTr("Rounds submitted: %1").arg(root.statsData.completed) : qsTr("Mix requests completed: %1").arg(root.statsData.completed))
                        + "  ·  " + qsTr("Failed: %1").arg(root.statsData.failed)
                      : qsTr("Statistics unavailable")
                color: "#f4f6f8"; font.pixelSize: 17; wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Totals since server start, published in completed five-minute windows. Submission does not mean blockchain confirmation.")
                      + (root.fresh && root.statusData ? "\n" + qsTr("Server started: %1").arg(new Date(root.statusData.started_at * 1000).toLocaleString())
                         + "\n" + qsTr("Received %1 s ago").arg(Math.max(0, Math.floor((root.now - root.receivedAt) / 1000))) : "")
                color: "#a9b4c2"; wrapMode: Text.Wrap
            }
            Label { Layout.fillWidth: true; visible: root.errorText !== ""; text: root.errorText; color: "#f4aa80"; wrapMode: Text.Wrap }
            Label { text: qsTr("Testnet RPC endpoint (/v1)"); color: "#f4f6f8" }
            TextField {
                id: endpointField
                Layout.fillWidth: true
                text: root.endpoint
                placeholderText: "https://…/v1"
                selectByMouse: true
                color: "#f4f7ff"
                placeholderTextColor: "#8794a6"
                background: Rectangle { radius: 10; color: "#cc131c28"; border.color: root.border }
                onAccepted: connectButton.clicked()
            }
            RowLayout {
                Button {
                    id: connectButton
                    palette.button: "#131c28"
                    palette.buttonText: "#f4f7ff"
                    text: qsTr("Connect")
                    onClicked: {
                        var url = endpointField.text.trim()
                        if (url !== "" && !root.validEndpoint(url)) {
                            root.cancel()
                            root.fail(qsTr("Use an HTTPS RPC URL without credentials, or localhost for development."))
                            return
                        }
                        if (root.settingsStore) root.settingsStore.mwixnetTestnetRpcUrl = url
                        else root.endpoint = url
                        root.refresh()
                    }
                }
                Button { palette.button: "#131c28"; palette.buttonText: "#f4f7ff"; text: root.loading ? qsTr("Loading…") : qsTr("Refresh"); enabled: !root.loading && root.endpoint !== ""; onClicked: root.refresh() }
            }
            Label { Layout.fillWidth: true; visible: root.endpoint === ""; text: qsTr("Configure a public read-only Testnet RPC endpoint to see live activity."); color: "#a9b4c2"; wrapMode: Text.Wrap }
        }
    }
}
