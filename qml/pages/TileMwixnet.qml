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
    property var statusData: null
    property var activityData: null
    property var transactionData: null
    property string selectedKernel: ""
    property string kernelFilter: ""
    property bool pollingEnabled: true
    property var blockProvider: typeof testnetBlockProvider !== "undefined" ? testnetBlockProvider : null
    property alias kernelModel: kernels
    ListModel { id: kernels }

    function updateTransactions(value) {
        transactionData = value
        var rows = value ? value.transactions.slice() : []
        rows.sort(function(a, b) { return (b.block_height || 0) - (a.block_height || 0) || a.kernel.localeCompare(b.kernel) })
        for (var i = 0; i < rows.length; ++i) {
            var tx = rows[i]
            var found = i
            while (found < kernels.count && kernels.get(found).kernel !== tx.kernel) ++found
            if (found === kernels.count) kernels.insert(i, {kernel: tx.kernel, blockHeight: -1, confirmations: -1, checkedAtHeight: -1})
            else if (found !== i) kernels.move(found, i, 1)
            kernels.set(i, {kernel: tx.kernel, blockHeight: tx.block_height === null ? -1 : tx.block_height,
                confirmations: tx.confirmations === null ? -1 : tx.confirmations,
                checkedAtHeight: tx.checked_at_height === null ? -1 : tx.checked_at_height})
        }
        if (kernels.count > rows.length) kernels.remove(rows.length, kernels.count - rows.length)
        var selected = rows.some(function(tx) { return tx.kernel === selectedKernel })
        if (!selected) selectedKernel = ""
    }
    property var statsData: null
    property var serverInfo: null
    property var routeData: null
    readonly property bool routeAvailable: routeData !== null
    property bool routeExpanded: false
    property bool usageExpanded: false
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
                "  --fee_per_hop " + Math.max(serverInfo.minimum_fee, 12500000) + " \\",
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
    readonly property string phase: fresh && statusData ? statusData.phase : "unavailable"
    readonly property bool active: visible && fresh && ["validating", "mixing", "submitting", "checking_confirmations"].indexOf(phase) >= 0

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
        activityData = null
        updateTransactions(null)
        selectedKernel = ""
        serverInfo = null
        routeData = null
        receivedAt = 0
    }

    function validStatus(value) {
        return value && value.network === "testnet"
                && value.role === "swap"
                && ["idle", "validating", "mixing", "submitting", "checking_confirmations", "failed"].indexOf(value.phase) >= 0
                && typeof value.started_at === "number" && isFinite(value.started_at)
                && typeof value.observed_at === "number" && isFinite(value.observed_at)
                && value.observed_at >= value.started_at
    }

    function counter(value) {
        return typeof value === "number" && isFinite(value) && value >= 0 && value <= 9007199254740991 && Math.floor(value) === value
    }

    function validStats(value, status) {
        if (!value || !status || value.started_at !== status.started_at
                || !counter(value.completed) || !counter(value.failed)
                || !counter(value.published_through_uptime_s) || value.window_s !== 300)
            return false
        var names = ["last_success_window_end_uptime_s", "last_failure_window_end_uptime_s"]
        for (var i = 0; i < names.length; i++) {
            var end = value[names[i]]
            if (end !== undefined && end !== null
                    && (!counter(end) || end % 300 !== 0 || end > value.published_through_uptime_s))
                return false
        }
        return true
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

    function validActivity(value, status) {
        return value && validStatus(value.status) && value.status.started_at === status.started_at
                && (value.unprocessed === null || counter(value.unprocessed)) && counter(value.round_inputs)
                && (value.awaiting_confirmation === undefined || value.awaiting_confirmation === null || counter(value.awaiting_confirmation))
                && (value.completed_phases === undefined || (Array.isArray(value.completed_phases)
                    && value.completed_phases.length <= 4 && value.completed_phases.every(function(phase) {
                        return ["validating", "mixing", "submitting", "checking_confirmations"].indexOf(phase) >= 0
                    })))
    }

    function validTransactions(value, status) {
        if (!value || value.network !== "testnet" || value.started_at !== status.started_at
                || !Array.isArray(value.transactions) || value.transactions.length > 32) return false
        var seen = {}
        for (var i = 0; i < value.transactions.length; i++) {
            var tx = value.transactions[i]
            if (!tx || !/^[0-9a-f]{66}$/.test(tx.kernel) || seen[tx.kernel]) return false
            seen[tx.kernel] = true
            var unknown = tx.block_height === null && tx.confirmations === null && tx.checked_at_height === null
            if (!unknown && (!counter(tx.block_height) || !counter(tx.checked_at_height)
                    || !counter(tx.confirmations) || tx.checked_at_height < tx.block_height
                    || tx.confirmations !== tx.checked_at_height - tx.block_height + 1)) return false
        }
        return true
    }

    function applyResponses(payload) {
        if (!Array.isArray(payload) || payload.length < 2 || payload.length > 9) {
            fail(qsTr("Invalid RPC response."))
            return
        }
        var results = {}
        var seen = {}
        for (var i = 0; i < payload.length; i++) {
            var response = payload[i]
            if (!response || response.jsonrpc !== "2.0" || !counter(response.id)
                    || response.id < 1 || response.id > 9 || seen[response.id]) {
                fail(qsTr("Invalid RPC response."))
                return
            }
            seen[response.id] = true
            results[response.id] = response.error ? null : response.result
        }
        var status = validStatus(results[1]) ? results[1] : null
        var stats = validStats(results[2], status) ? results[2] : null
        if (!status || !stats) {
            fail(qsTr("MWixnet status is temporarily unavailable."))
            return
        }
        if (statusData && statusData.started_at !== status.started_at) {
            selectedKernel = ""
        }
        activityData = validActivity(results[8], status) ? results[8] : null
        var transactions = validTransactions(results[9], status) ? results[9] : null
        updateTransactions(transactions)
        if (activityData) status = activityData.status
        statusData = status
        statsData = stats
        serverInfo = validInfo(results[3]) ? results[3] : null
        routeData = validRoute(results[4]) ? results[4] : null
        errorText = ""
        receivedAt = Date.now()
        now = receivedAt
    }

    function phaseLabel(status) {
        if (!fresh || !status) return qsTr("Unavailable")
        if (status.phase === "failed" && activityData && activityData.unprocessed > 0)
            return qsTr("Retry next round")
        var labels = {idle: qsTr("Idle"), validating: qsTr("Validating"), mixing: qsTr("Mixing"),
            submitting: qsTr("Submitting"), checking_confirmations: qsTr("Checking chain"), failed: qsTr("Attempt failed")}
        return labels[status.phase]
    }

    function uptime(status) {
        if (!fresh || !status) return "—"
        var seconds = status.observed_at - status.started_at
        return Math.floor(seconds / 3600) + "h " + Math.floor(seconds % 3600 / 60) + "m"
    }

    function lastSuccess(stats) {
        if (!fresh || !stats || stats.last_success_window_end_uptime_s === undefined
                || stats.last_success_window_end_uptime_s === null) return "—"
        var end = stats.started_at + stats.last_success_window_end_uptime_s
        return Qt.formatDateTime(new Date((end - 300) * 1000), "dd MMM hh:mm") + "–"
                + Qt.formatDateTime(new Date(end * 1000), "hh:mm")
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
            if (xhr.status !== 200) { fail(qsTr("MWixnet status is temporarily unavailable.")); return }
            try { applyResponses(JSON.parse(xhr.responseText)) }
            catch (e) { fail(qsTr("Invalid RPC response.")) }
        }
        try {
            xhr.open("GET", endpoint)
            timeout.start()
            xhr.send()
        } catch (e) {
            cancel()
            fail(qsTr("RPC request failed."))
        }
    }

    onVisibleChanged: { if (visible) refresh(); else cancel() }
    Component.onCompleted: refresh()
    Component.onDestruction: cancel()

    Timer { id: timeout; interval: 10000; onTriggered: { root.cancel(); root.fail(qsTr("RPC request timed out.")) } }
    Timer { interval: 5000; running: root.visible && root.pollingEnabled; repeat: true; onTriggered: root.refresh() }
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
                readonly property bool online: root.fresh && !!root.statusData
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
                        Label { text: serverCard.online ? qsTr("Online") : qsTr("Unavailable"); color: "#a9b4c2"; font.pixelSize: 12 }
                    }
                    Label {
                        text: root.phaseLabel(root.statusData)
                        color: "#f4f7ff"
                        font.pixelSize: 30
                        font.weight: Font.DemiBold
                    }
                    Row {
                        Layout.fillWidth: true
                        spacing: 6
                        Repeater {
                            model: [
                                {phase: "validating", label: qsTr("Validate"), hint: qsTr("Checks incoming swap requests.")},
                                {phase: "mixing", label: qsTr("Mix"), hint: qsTr("Combines inputs for the round.")},
                                {phase: "submitting", label: qsTr("Submit"), hint: qsTr("Sends the transaction to the network. Confirmation is still pending.")},
                                {phase: "checking_confirmations", label: qsTr("Check chain"), hint: qsTr("Checks for blockchain confirmations. A completed check does not mean every transaction is confirmed.")}
                            ]
                            Column {
                                id: phaseStep
                                required property var modelData
                                width: (parent.width - 3 * 6) / 4
                                spacing: 10
                                readonly property bool selected: root.active && root.statusData.phase === modelData.phase
                                readonly property bool completed: root.fresh && root.activityData && (root.activityData.completed_phases || []).indexOf(modelData.phase) >= 0
                                Rectangle {
                                    objectName: "phaseBar_" + phaseStep.modelData.phase
                                    property real pulse: 1
                                    opacity: phaseStep.selected ? pulse : 1
                                    width: parent.width; height: 5; radius: 3
                                    color: phaseStep.selected ? "#FEF102" : phaseStep.completed ? "#94c9a2" : "#344356"
                                    SequentialAnimation on pulse {
                                        running: phaseStep.selected
                                        loops: Animation.Infinite
                                        NumberAnimation { from: 0.45; to: 1; duration: 800 }
                                        NumberAnimation { to: 0.45; duration: 800 }
                                    }
                                }
                                HoverHandler { id: phaseHover }
                                TapHandler { id: phaseTap }
                                ToolTip.visible: phaseHover.hovered || phaseTap.pressed
                                ToolTip.text: phaseStep.modelData.hint + (phaseStep.selected ? "\n" + qsTr("Running now.") : phaseStep.completed ? "\n" + qsTr("✓ This step completed within the last 20 seconds.") : "")
                                Label { text: phaseStep.modelData.label + (phaseStep.completed && !phaseStep.selected ? " ✓" : ""); color: phaseStep.selected ? "#FEF102" : "#a9b4c2"; font.pixelSize: 11 }
                            }
                        }
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: root.compact ? 2 : 4
                        columnSpacing: 12
                        rowSpacing: 12
                        Repeater {
                            model: [
                                {name: qsTr("Unprocessed"), value: root.fresh && root.activityData && root.activityData.unprocessed !== null ? String(root.activityData.unprocessed) : "—", lit: root.fresh && root.activityData && root.activityData.unprocessed > 0},
                                {name: qsTr("In round"), value: root.fresh && root.activityData ? String(root.activityData.round_inputs) : "—", lit: root.fresh && root.activityData && root.activityData.round_inputs > 0},
                                {name: qsTr("Awaiting confirmation"), value: root.fresh && root.activityData && root.counter(root.activityData.awaiting_confirmation) ? String(root.activityData.awaiting_confirmation) : "—", lit: root.fresh && root.activityData && root.activityData.awaiting_confirmation > 0},
                                {name: qsTr("Uptime"), value: root.uptime(root.statusData), lit: false}
                            ]
                            Rectangle {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.preferredWidth: 1
                                implicitHeight: 90
                                radius: 12
                                color: modelData.lit ? "#18fef102" : "#0affffff"
                                border.color: modelData.lit ? "#70fef102" : "#254f5b6b"
                                ColumnLayout {
                                    anchors.fill: parent; anchors.margins: 14; spacing: 6
                                    Label { text: modelData.value; color: modelData.lit ? "#FEF102" : "#f4f7ff"; font.pixelSize: 28; font.weight: Font.DemiBold }
                                    Label { text: modelData.name; color: "#a9b4c2"; font.pixelSize: 12 }
                                }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Repeater {
                            model: [
                                {label: qsTr("Rounds submitted"), value: serverCard.online && root.statsData ? String(root.statsData.completed) : "—"},
                                {label: qsTr("Failed attempts"), value: serverCard.online && root.statsData ? String(root.statsData.failed) : "—"},
                                {label: qsTr("Published through"), value: root.fresh && root.statsData ? Qt.formatDateTime(new Date((root.statsData.started_at + root.statsData.published_through_uptime_s) * 1000), "hh:mm") : "—"}
                            ]
                            ColumnLayout {
                                id: statistic
                                required property int index
                                required property var modelData
                                readonly property int textAlignment: index === 0 ? Text.AlignLeft : index === 1 ? Text.AlignHCenter : Text.AlignRight
                                Layout.fillWidth: true
                                Layout.preferredWidth: 1
                                spacing: 8
                                Label { text: modelData.value; color: "#f4f7ff"; font.pixelSize: 22; font.weight: Font.DemiBold; Layout.fillWidth: true; horizontalAlignment: statistic.textAlignment }
                                Label { text: modelData.label; color: "#a9b4c2"; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.Wrap; horizontalAlignment: statistic.textAlignment }
                            }
                        }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: root.border }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: 10
                        Label { text: qsTr("Last success window"); Layout.fillWidth: true; color: "#a9b4c2"; font.pixelSize: 12 }
                        Label { text: root.lastSuccess(root.statsData); color: "#f4f7ff"; font.pixelSize: 12; Layout.alignment: Qt.AlignRight }
                        Label { text: qsTr("Round interval"); color: "#a9b4c2"; font.pixelSize: 12 }
                        Label { text: root.fresh && root.serverInfo && root.serverInfo.round_interval_s !== null ? root.serverInfo.round_interval_s + " s" : "—"; color: "#f4f7ff"; font.pixelSize: 12; Layout.alignment: Qt.AlignRight }
                        Label { text: qsTr("Minimum fee"); color: "#a9b4c2"; font.pixelSize: 12 }
                        Label { text: root.fresh && root.serverInfo ? (root.serverInfo.minimum_fee / 1000000000).toFixed(9).replace(/0+$/, "").replace(/\.$/, "") + " Testnet GRIN" : "—"; color: "#f4f7ff"; font.pixelSize: 12; Layout.alignment: Qt.AlignRight }
                        Label { text: qsTr("Version"); color: "#a9b4c2"; font.pixelSize: 12 }
                        Label { text: root.fresh && root.serverInfo ? root.serverInfo.version : "—"; color: "#f4f7ff"; font.pixelSize: 12; Layout.alignment: Qt.AlignRight; Layout.maximumWidth: 150; elide: Text.ElideRight; textFormat: Text.PlainText }
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
                        Label { text: qsTr("Server connections"); color: "#f4f7ff"; font.pixelSize: 18; Layout.fillWidth: true }
                        Label { visible: !root.fresh || !root.routeAvailable; text: qsTr("Unavailable"); color: "#a9b4c2"; font.pixelSize: 12 }
                        AppButton {
                            text: root.routeExpanded ? qsTr("Hide") : qsTr("Details")
                            enabled: root.fresh && !!root.routeData
                            onClicked: root.routeExpanded = !root.routeExpanded
                        }
                    }
                    Repeater {
                        model: root.routeExpanded && root.fresh && root.routeData ? [{name: qsTr("This server"), peer: root.routeData.server}, {name: qsTr("Next mixer"), peer: root.routeData.next}] : []
                        ColumnLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 8
                            Label { text: modelData.name; color: "#f4f7ff"; font.pixelSize: 13 }
                            Label { visible: !modelData.peer; text: qsTr("None configured"); color: "#a9b4c2"; font.pixelSize: 12 }
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
                        Label { text: qsTr("Usage"); color: "#f4f7ff"; font.pixelSize: 18; Layout.fillWidth: true }
                        AppButton {
                            text: root.usageExpanded ? qsTr("Hide") : qsTr("Details")
                            onClicked: root.usageExpanded = !root.usageExpanded
                        }
                    }
                    Label {
                        visible: root.usageExpanded
                        text: root.usageExample.length > 0 ? qsTr("Try a swap with grin-wallet on Testnet.") : qsTr("No CLI example available for these connections.")
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
                        text: qsTr("Commitment: choose an output. Amount: choose the smallest eligible output at or above it. max: choose the largest.\nThe CLI prints the selected commitment and its full value before sending.")
                        color: "#a9b4c2"; font.pixelSize: 12
                        Layout.fillWidth: true; wrapMode: Text.Wrap
                    }
                    Label {
                        visible: root.usageExpanded && root.usageExample.length > 0
                        text: qsTr("The second onion key is verified for this Testnet mixer and is not supplied by the connections API.")
                        color: "#94a1b2"; font.pixelSize: 11
                        Layout.fillWidth: true; wrapMode: Text.Wrap
                    }
                }
            }
            Pane {
                Layout.fillWidth: true
                padding: root.compact ? 20 : 28
                background: Rectangle { color: root.panel; radius: 20; border.color: root.border }
                contentItem: ColumnLayout {
                    spacing: 16
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("On-chain kernels"); color: "#f4f7ff"; font.pixelSize: 20; Layout.fillWidth: true }
                        Label { text: root.fresh && root.transactionData ? String(root.transactionData.transactions.length) : "—"; color: "#FEF102"; font.pixelSize: 20 }
                    }
                    TextField {
                        Layout.fillWidth: true
                        placeholderText: qsTr("Find a kernel…")
                        text: root.kernelFilter
                        onTextEdited: root.kernelFilter = text
                        color: "#f4f7ff"; placeholderTextColor: "#8794a6"
                        selectByMouse: true
                        background: Rectangle { color: "#0bffffff"; radius: 10; border.color: root.border }
                    }
                    Label {
                        visible: !root.fresh || !root.transactionData || !root.transactionData.transactions.some(function(tx) { return tx.kernel.indexOf(root.kernelFilter.trim().toLowerCase()) >= 0 })
                        text: !root.fresh || !root.transactionData ? qsTr("Kernel data unavailable") : root.kernelFilter.length > 0 ? qsTr("No matching kernel") : qsTr("No kernels observed on-chain yet")
                        color: "#a9b4c2"; font.pixelSize: 14
                        Layout.fillWidth: true; wrapMode: Text.Wrap
                    }
                    Repeater {
                        model: kernels
                        ColumnLayout {
                            id: kernelEntry
                            objectName: "kernel_" + kernel
                            required property string kernel
                            required property double blockHeight
                            required property double confirmations
                            required property double checkedAtHeight
                            visible: root.fresh && kernel.indexOf(root.kernelFilter.trim().toLowerCase()) >= 0
                            Layout.fillWidth: true
                            spacing: 10
                            Button {
                                id: kernelRow
                                Layout.fillWidth: true
                                implicitHeight: 58
                                onClicked: root.selectedKernel = root.selectedKernel === kernelEntry.kernel ? "" : kernelEntry.kernel
                                background: Rectangle {
                                    color: root.selectedKernel === kernelEntry.kernel ? "#18fef102" : kernelRow.hovered ? "#12ffffff" : "#07ffffff"
                                    radius: 10
                                    border.color: root.selectedKernel === kernelEntry.kernel ? "#80fef102" : "#254f5b6b"
                                }
                                contentItem: RowLayout {
                                    spacing: 10
                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 3
                                        Label { text: kernelEntry.kernel.slice(0, root.compact ? 12 : 22) + "…" + kernelEntry.kernel.slice(-8); color: "#e8edf5"; font.family: "monospace"; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideMiddle }
                                        Label { text: kernelEntry.blockHeight < 0 ? qsTr("Confirmation unknown") : qsTr("Block ") + kernelEntry.blockHeight; color: "#94a1b2"; font.pixelSize: 11 }
                                    }
                                    Label { text: kernelEntry.confirmations < 0 ? "—" : kernelEntry.confirmations + qsTr(" conf."); color: kernelEntry.confirmations < 0 ? "#94a1b2" : "#94c9a2"; font.pixelSize: 12 }
                                    Label { text: root.selectedKernel === kernelEntry.kernel ? "−" : "+"; color: "#FEF102"; font.pixelSize: 18 }
                                }
                            }
                            ColumnLayout {
                                visible: root.selectedKernel === kernelEntry.kernel
                                Layout.fillWidth: true; spacing: 8
                                Label { text: qsTr("Kernel excess"); color: "#94a1b2"; font.pixelSize: 12 }
                                TextEdit {
                                    Layout.fillWidth: true
                                    text: kernelEntry.kernel
                                    readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere
                                    color: "#f4f7ff"; font.family: "monospace"; font.pixelSize: 13
                                    textFormat: TextEdit.PlainText
                                }
                                Label { text: kernelEntry.checkedAtHeight >= 0 ? qsTr("Checked at block ") + kernelEntry.checkedAtHeight : qsTr("Awaiting chain verification"); color: "#94a1b2"; font.pixelSize: 12 }
                                Loader {
                                    Layout.fillWidth: true
                                    active: root.selectedKernel === kernelEntry.kernel
                                    sourceComponent: MwixnetBlockDetails {
                                        objectName: "blockDetails"
                                        kernel: kernelEntry.kernel
                                        provider: root.blockProvider
                                        expectedHeight: kernelEntry.blockHeight
                                        checkedAtHeight: kernelEntry.checkedAtHeight
                                    }
                                }
                            }
                        }
                    }
                    Label {
                        text: qsTr("Up to 32 on-chain observations. No link to individual requests.")
                        color: "#94a1b2"; font.pixelSize: 11
                        Layout.fillWidth: true; wrapMode: Text.Wrap
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: root.fresh ? qsTr("Totals since restart · 5-minute windows · times shown locally") : qsTr("Reconnecting automatically")
                    color: "#94a1b2"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}
