import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components"

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
    signal backRequested()
    anchors.fill: parent

    readonly property string endpoint: "https://mwixnet.grinffindor.org/v1"
    property bool pollingEnabled: true
    property var serverInfo: null
    property var routeData: null
    readonly property bool routeAvailable: routeData !== null
    property bool routeExpanded: false
    property bool usageExpanded: false
    property int demoStep: 0
    Timer { interval: 1500; running: root.visible; repeat: true; onTriggered: root.demoStep = (root.demoStep + 1) % 3 }
    readonly property string usageExample: {
        if (!fresh || !routeData || !serverInfo) return ""
        var next = routeData.next
        // Public onion key supplied for this Testnet mixer; get_route exposes only its identity key.
        if (!next || next.onion_address !== "56bkem4ly7oebmtw7y5d25veoi4t47txqvxlptsy7kctsfid6ekgsuqd.onion"
                || next.identity_key !== "ef82a2338bc7dc40b276fe3a3d76a472393e7e77856eb7ce58fa85391503f114") return ""
        var wallet = "./target/release/grin-wallet --testnet"
        return ["# Select an unspent output", wallet + " outputs", "",
                "# Swap the selected output", wallet + " mwixnet \\",
                "  <OUTPUT_COMMITMENT|MINIMUM_AMOUNT|max> \\",
                "  --server " + routeData.server.onion_address + " \\",
                "  --fee_per_hop " + (Math.max(serverInfo.minimum_fee, 12500000) / 1000000000).toFixed(9) + " \\",
                "  --key " + routeData.server.onion_key + " \\",
                "  --key a6920c1ef8bd8bb14fe4940521eca294f797856baaa307eb51894dda157b1477",
                "", "# Help", wallet + " mwixnet --help"].join("\n")
    }

    property string errorText: ""
    property double receivedAt: 0
    property double now: Date.now()
    property var pending: null
    property int requestId: 0
    property bool loading: false
    readonly property bool fresh: receivedAt > 0 && now - receivedAt < 45000 && errorText === ""

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
        serverInfo = null
        routeData = null
        receivedAt = 0
    }

    // IDs 3 and 4 are the proxy's server-info and route responses.
    // Ignore legacy monitoring responses until the proxy is updated.
    function applyResponses(payload) {
        if (!Array.isArray(payload) || payload.length > 9) {
            fail(qsTr("Invalid response"))
            return
        }
        var results = {}
        var seen = {}
        for (var i = 0; i < payload.length; i++) {
            var response = payload[i]
            if (!response || response.jsonrpc !== "2.0" || !counter(response.id) || seen[response.id]) {
                fail(qsTr("Invalid response"))
                return
            }
            seen[response.id] = true
            if (response.id === 3 || response.id === 4)
                results[response.id] = response.error ? null : response.result
        }
        if (!validInfo(results[3])) {
            fail(qsTr("Server unavailable"))
            return
        }
        serverInfo = results[3]
        routeData = validRoute(results[4]) ? results[4] : null
        errorText = ""
        receivedAt = Date.now()
        now = receivedAt
    }

    function counter(value) {
        return typeof value === "number" && isFinite(value) && value >= 0 && value <= 9007199254740991 && Math.floor(value) === value
    }

    function formatUptime(seconds) {
        if (!counter(seconds)) return qsTr("Unknown")
        var minutes = Math.floor(seconds / 60)
        return Math.floor(minutes / 1440) + " d " + Math.floor(minutes / 60) % 24 + " h " + minutes % 60 + " min"
    }

    function validInfo(value) {
        return value && value.network === "testnet" && value.role === "swap"
                && typeof value.version === "string" && value.version.length > 0 && value.version.length <= 80
                && counter(value.minimum_fee)
                && (counter(value.round_interval_s) && value.round_interval_s > 0)
    }

    function validPeer(value) {
        return value && /^[a-z2-7]{56}\.onion$/.test(value.onion_address)
                && /^[0-9a-f]{64}$/.test(value.identity_key)
    }

    function validRoute(value) {
        return value && value.network === "testnet" && validPeer(value.server)
                && value.server.role === "swap"
                && /^[0-9a-f]{64}$/.test(value.server.onion_key)
                && (value.next === null || validPeer(value.next))
    }

    function refresh() {
        if (!pollingEnabled || !visible || loading) return
        loading = true
        var generation = ++requestId
        var xhr = new XMLHttpRequest()
        pending = xhr
        xhr.onreadystatechange = function() {
            if (generation !== requestId || xhr.readyState !== XMLHttpRequest.DONE) return
            timeout.stop()
            pending = null
            loading = false
            if (xhr.status !== 200) { fail(qsTr("Server unavailable")); return }
            try { applyResponses(JSON.parse(xhr.responseText)) }
            catch (e) { fail(qsTr("Invalid response")) }
        }
        try {
            xhr.open("GET", endpoint)
            timeout.start()
            xhr.send()
        } catch (e) {
            cancel()
            fail(qsTr("Request failed"))
        }
    }

    onVisibleChanged: { if (visible) refresh(); else cancel() }
    Component.onCompleted: refresh()
    Component.onDestruction: cancel()

    Timer { id: timeout; interval: 10000; onTriggered: { root.cancel(); root.fail(qsTr("Request timed out")) } }
    Timer { interval: 30000; running: root.visible && root.pollingEnabled; repeat: true; onTriggered: root.refresh() }
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
                text: "MWixnet Testnet"
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
            width: Math.min(scroll.availableWidth, 1040)
            x: (scroll.availableWidth - width) / 2
            spacing: 18

            Pane {
                id: serverCard
                readonly property bool online: root.fresh && !!root.serverInfo
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                padding: root.compact ? 20 : 28
                background: Rectangle { color: root.panel; radius: 20; border.color: root.border }
                contentItem: ColumnLayout {
                    spacing: 22
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Swap server"); color: "#a9b4c2"; font.pixelSize: 14; Layout.fillWidth: true }
                        Rectangle {
                            width: 7; height: 7; radius: 4
                            color: serverCard.online ? "#94c9a2" : "#8794a6"
                        }
                        Label { text: serverCard.online ? qsTr("API reachable") : qsTr("Unavailable"); color: "#a9b4c2"; font.pixelSize: 12 }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: 10
                        Label { text: qsTr("Network"); color: "#a9b4c2"; font.pixelSize: 12 }
                        Label { text: root.fresh && root.serverInfo ? qsTr("Testnet") : qsTr("Unknown"); color: "#f4f7ff"; font.pixelSize: 12; Layout.alignment: Qt.AlignRight }
                        Label { text: qsTr("Uptime"); color: "#a9b4c2"; font.pixelSize: 12 }
                        Label { text: root.fresh && root.serverInfo ? root.formatUptime(root.serverInfo.uptime_s) : qsTr("Unknown"); color: "#f4f7ff"; font.pixelSize: 12; Layout.alignment: Qt.AlignRight }
                        Label { text: qsTr("Round interval"); color: "#a9b4c2"; font.pixelSize: 12 }
                        Label { text: root.fresh && root.serverInfo && root.serverInfo.round_interval_s !== null ? root.serverInfo.round_interval_s + " s" : qsTr("Unknown"); color: "#f4f7ff"; font.pixelSize: 12; Layout.alignment: Qt.AlignRight }
                        Label { text: qsTr("Minimum fee"); color: "#a9b4c2"; font.pixelSize: 12 }
                        Label { text: root.fresh && root.serverInfo ? (root.serverInfo.minimum_fee / 1000000000).toFixed(9).replace(/0+$/, "").replace(/\.$/, "") + " Testnet GRIN" : qsTr("Unknown"); color: "#f4f7ff"; font.pixelSize: 12; Layout.alignment: Qt.AlignRight }
                        Label { text: qsTr("Version"); color: "#a9b4c2"; font.pixelSize: 12 }
                        Label { text: root.fresh && root.serverInfo ? root.serverInfo.version : qsTr("Unknown"); color: "#f4f7ff"; font.pixelSize: 12; Layout.alignment: Qt.AlignRight; Layout.maximumWidth: 150; elide: Text.ElideRight; textFormat: Text.PlainText }
                    }
                }
            }
            Pane {
                Layout.fillWidth: true
                padding: root.compact ? 20 : 28
                background: Rectangle { color: root.panel; radius: 20; border.color: root.border }
                contentItem: ColumnLayout {
                    spacing: 18
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Servers"); color: "#f4f7ff"; font.pixelSize: 18; Layout.fillWidth: true }
                        Label { visible: !root.fresh || !root.routeAvailable; text: qsTr("Unavailable"); color: "#a9b4c2"; font.pixelSize: 12 }
                        AppButton {
                            text: root.routeExpanded ? qsTr("Hide") : qsTr("Details")
                            enabled: root.fresh && !!root.routeData
                            onClicked: root.routeExpanded = !root.routeExpanded
                        }
                    }
                    Repeater {
                        model: root.routeExpanded && root.fresh && root.routeData ? [{name: qsTr("Swap server"), peer: root.routeData.server}, {name: qsTr("Next mixer"), peer: root.routeData.next}] : []
                        ColumnLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 8
                            Label { text: modelData.name; color: "#f4f7ff"; font.pixelSize: 13 }
                            Label { visible: !modelData.peer; text: qsTr("None"); color: "#a9b4c2"; font.pixelSize: 12 }
                            Repeater {
                                model: !modelData.peer ? [] : [
                                    {label: qsTr("Onion address"), value: modelData.peer.onion_address},
                                    {label: qsTr("Identity key"), value: modelData.peer.identity_key}
                                ].concat(modelData.peer.onion_key ? [{label: qsTr("Onion key"), value: modelData.peer.onion_key}] : [])
                                ColumnLayout {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    spacing: 3
                                    Label { text: modelData.label; color: "#94a1b2"; font.pixelSize: 11 }
                                    TextEdit {
                                        text: modelData.value
                                        Layout.fillWidth: true
                                        readOnly: true
                                        selectByMouse: true
                                        wrapMode: TextEdit.WrapAnywhere
                                        color: "#c8d2e0"
                                        selectedTextColor: "#ffffff"
                                        font.family: "monospace"
                                        font.pixelSize: 12
                                        textFormat: TextEdit.PlainText
                                    }
                                }
                            }
                        }
                    }
                }
            }
            Pane {
                Layout.fillWidth: true
                padding: root.compact ? 20 : 28
                background: Rectangle { color: root.panel; radius: 20; border.color: root.border }
                contentItem: ColumnLayout {
                    spacing: 14
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Try a swap"); color: "#f4f7ff"; font.pixelSize: 18; Layout.fillWidth: true }
                        AppButton {
                            text: root.usageExpanded ? qsTr("Hide") : qsTr("Details")
                            onClicked: root.usageExpanded = !root.usageExpanded
                        }
                    }
                    Label {
                        visible: root.usageExpanded && root.usageExample.length === 0
                        text: qsTr("Example unavailable")
                        color: "#a9b4c2"; font.pixelSize: 13
                        Layout.fillWidth: true; wrapMode: Text.Wrap
                    }
                    Rectangle {
                        visible: root.usageExpanded && root.usageExample.length > 0
                        Layout.fillWidth: true
                        implicitHeight: usageCode.implicitHeight + 28
                        color: "#0bffffff"; radius: 10; border.color: root.border
                        TextEdit {
                            id: usageCode
                            width: parent.width - 28
                            x: 14; y: 14
                            text: root.usageExample
                            readOnly: true; selectByMouse: true
                            wrapMode: TextEdit.WrapAnywhere
                            textFormat: TextEdit.PlainText
                            color: "#c8d2e0"; font.family: "monospace"; font.pixelSize: 12
                        }
                    }
                    Label {
                        visible: root.usageExpanded && root.usageExample.length > 0
                        text: qsTr("Choose an output or enter a minimum amount\nUse max for the largest output\nThe whole output is swapped")
                        color: "#a9b4c2"; font.pixelSize: 12
                        Layout.fillWidth: true; wrapMode: Text.Wrap
                    }

                }
            }
            Pane {
                Layout.fillWidth: true
                padding: root.compact ? 20 : 28
                background: Rectangle { color: root.panel; radius: 20; border.color: root.border }
                contentItem: ColumnLayout {
                    spacing: 12
                    Label { text: qsTr("Swap example"); color: "#f4f7ff"; font.pixelSize: 18 }
                    RowLayout {
                        Layout.fillWidth: true
                        Repeater {
                            model: [qsTr("Submit"), qsTr("Mix"), qsTr("Confirm")]
                            ColumnLayout {
                                required property int index
                                required property string modelData
                                Layout.fillWidth: true
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 4; radius: 2; color: "#FEF102"
                                    opacity: root.demoStep === index ? 1 : 0.2
                                    Behavior on opacity { NumberAnimation { duration: 400 } }
                                }
                                Label { text: modelData; color: "#c8d2e0"; font.pixelSize: 12 }
                            }
                        }
                    }

                }
            }
        }
    }
}
