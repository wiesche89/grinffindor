import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "runtime" as Runtime

Item {
    id: root
    property var i18n: null
    property var settingsStore: null
    signal backRequested()

    anchors.fill: parent
    readonly property bool compact: width < 760
    readonly property int pageGutter: compact ? 16 : 28
    readonly property color panel: "#d80b1018"
    readonly property color panelSoft: "#cc131c28"
    readonly property color border: "#406b7380"
    property string activeView: "runtime"
    property var runtimeData: ({ summary: {}, nodes: [] })
    property var benchmarkData: ({ summary: {}, runs: [] })
    property var notificationData: ({ summary: {}, notifications: [] })
    readonly property int maxNotifications: 100
    property var setupData: ({ summary: {}, nodes: [], source: "prometheus" })
    property string errorText: ""
    property bool loading: false

    function assetPath(path) {
        return (typeof assetBaseUrl === "string" ? assetBaseUrl : "qrc:/res/") + path
    }

    function baseUrl() {
        var raw = settingsStore && settingsStore.runtimePrometheusUrl
                ? settingsStore.runtimePrometheusUrl
                : "https://prometheus.grinffindor.org"
        while (raw.length > 1 && raw.charAt(raw.length - 1) === "/")
            raw = raw.slice(0, -1)
        return raw
    }

    function fmt(value, fallback) {
        if (value === undefined || value === null || value === "")
            return fallback === undefined ? "-" : fallback
        return String(value)
    }

    function pct(value) {
        var n = Number(value)
        return isFinite(n) ? n.toFixed(1) + "%" : "-"
    }

    function seconds(value) {
        var n = Number(value)
        if (!isFinite(n))
            return "-"
        if (n < 60)
            return n.toFixed(1) + "s"
        return (n / 60).toFixed(1) + "m"
    }

    function sampleValue(sample) {
        try {
            return Number(sample.value[1])
        } catch (e) {
            return 0
        }
    }

    function metricLabels(sample) {
        return sample && sample.metric ? sample.metric : {}
    }

    function requestQuery(query, callback) {
        if (baseUrl().length === 0) {
            root.errorText = "Prometheus data source is not configured."
            root.loading = false
            return
        }
        var xhr = new XMLHttpRequest()
        xhr.open("GET", baseUrl() + "/api/v1/query?query=" + encodeURIComponent(query))
        xhr.onreadystatechange = function() {
            if (xhr.readyState !== XMLHttpRequest.DONE)
                return
            if (xhr.status >= 200 && xhr.status < 300) {
                var payload = JSON.parse(xhr.responseText)
                if (payload.status === "success") {
                    callback(payload.data.result || [])
                    return
                }
                root.errorText = payload.error || "Prometheus query failed"
                return
            }
            root.errorText = "Prometheus API " + xhr.status + ": " + xhr.responseText
        }
        xhr.onerror = function() {
            root.errorText = "Prometheus is not reachable."
        }
        xhr.send()
    }

    function nodeListFromMap(nodes) {
        var result = []
        for (var key in nodes)
            result.push(nodes[key])
        result.sort(function(a, b) {
            return String(a.node_name).localeCompare(String(b.node_name))
        })
        return result
    }

    function buildRuntime() {
        var nodes = {}
        requestQuery("grin_node_info", function(infoRows) {
            for (var i = 0; i < infoRows.length; i++) {
                var labels = metricLabels(infoRows[i])
                var node = labels.node || ""
                nodes[node] = {
                    node_id: labels.node_id || node,
                    node_name: node,
                    node_type: labels.node_type || "",
                    profile: labels.profile || "",
                    status: sampleValue(infoRows[i]) > 0 ? "running" : "unavailable",
                    api_up: false,
                    chain: labels.chain || "",
                    height: 0,
                    header_height: 0,
                    connections: 0,
                    sync_status: labels.sync_status || "unknown",
                    sync_stage: "Unavailable",
                    sync_percent: 0,
                    version: labels.version || "",
                    protocol: labels.protocol_version || "",
                    user_agent: labels.user_agent || "",
                    failure_state: labels.failure_state || "unknown",
                    peers: [],
                    error: ""
                }
            }
            requestQuery("grin_node_height", function(rows) {
                for (var i = 0; i < rows.length; i++) {
                    var labels = metricLabels(rows[i])
                    if (nodes[labels.node])
                        nodes[labels.node].height = Math.round(sampleValue(rows[i]))
                }
                requestQuery("grin_node_header_height", function(rows) {
                    for (var i = 0; i < rows.length; i++) {
                        var labels = metricLabels(rows[i])
                        if (nodes[labels.node])
                            nodes[labels.node].header_height = Math.round(sampleValue(rows[i]))
                    }
                    requestQuery("grin_node_connections", function(rows) {
                        for (var i = 0; i < rows.length; i++) {
                            var labels = metricLabels(rows[i])
                            if (nodes[labels.node])
                                nodes[labels.node].connections = Math.round(sampleValue(rows[i]))
                        }
                        requestQuery("grin_node_up", function(rows) {
                            for (var i = 0; i < rows.length; i++) {
                                var labels = metricLabels(rows[i])
                                if (nodes[labels.node]) {
                                    nodes[labels.node].api_up = sampleValue(rows[i]) > 0
                                    nodes[labels.node].error = labels.error || ""
                                }
                            }
                            requestQuery("grin_node_sync_progress_percent", function(rows) {
                                for (var i = 0; i < rows.length; i++) {
                                    var labels = metricLabels(rows[i])
                                    if (nodes[labels.node]) {
                                        nodes[labels.node].sync_stage = labels.sync_stage || ""
                                        nodes[labels.node].sync_status = labels.sync_status || nodes[labels.node].sync_status
                                        nodes[labels.node].sync_percent = sampleValue(rows[i])
                                    }
                                }
                                requestQuery("grin_peer_info", function(rows) {
                                    for (var i = 0; i < rows.length; i++) {
                                        var labels = metricLabels(rows[i])
                                        if (!nodes[labels.node])
                                            continue
                                        nodes[labels.node].peers.push({
                                            addr: labels.peer || "",
                                            ip: labels.peer_ip || "",
                                            port: labels.peer_port || "",
                                            direction: labels.direction || "",
                                            height: Number(labels.height || 0),
                                            total_difficulty: Number(labels.total_difficulty || 0),
                                            protocol: labels.version || "",
                                            client_version: labels.client_version || "",
                                            user_agent: labels.user_agent || ""
                                        })
                                    }
                                    var list = nodeListFromMap(nodes)
                                    var apiUp = 0
                                    var syncing = 0
                                    var peers = 0
                                    for (var i = 0; i < list.length; i++) {
                                        if (list[i].api_up)
                                            apiUp++
                                        if (list[i].sync_status !== "no_sync" && list[i].sync_status !== "txhashset_done" && list[i].sync_status !== "TxHashsetDone")
                                            syncing++
                                        peers += Number(list[i].connections || 0)
                                    }
                                    runtimeData = {
                                        updated_at: Date.now() / 1000,
                                        summary: {
                                            node_count: list.length,
                                            api_up_count: apiUp,
                                            syncing_count: syncing,
                                            peer_count: peers
                                        },
                                        nodes: list
                                    }
                                    loading = false
                                })
                            })
                        })
                    })
                })
            })
        })
    }

    function buildBenchmarks() {
        var runs = {}
        requestQuery("grin_benchmark_run_info", function(rows) {
            for (var i = 0; i < rows.length; i++) {
                var labels = metricLabels(rows[i])
                var id = labels.run_id || ""
                runs[id] = {
                    id: Number(id || 0),
                    node_id: labels.node_id || "",
                    node_name: labels.node_name || "",
                    node_type: labels.node_type || "",
                    profile: labels.profile || "",
                    experiment_id: labels.experiment_id || "",
                    sync_run_id: labels.sync_run_id || "",
                    sync_started_at: labels.sync_started_at || "",
                    sync_completed_at: labels.sync_completed_at || "",
                    result: labels.result || "",
                    error_message: labels.error_message || "",
                    total_sync_duration: null,
                    header_sync_duration: null,
                    PIHD_duration: null,
                    PIBD_duration: null,
                    rangeproof_validation_duration: null,
                    kernel_validation_duration: null,
                    final_height: null,
                    average_peer_count: null,
                    max_cpu_usage: null,
                    max_ram_usage: null,
                    max_disk_io: null
                }
            }
            requestQuery("grin_benchmark_duration_seconds", function(rows) {
                var phaseMap = {
                    "total": "total_sync_duration",
                    "header": "header_sync_duration",
                    "PIHD": "PIHD_duration",
                    "PIBD": "PIBD_duration",
                    "rangeproof_validation": "rangeproof_validation_duration",
                    "kernel_validation": "kernel_validation_duration"
                }
                for (var i = 0; i < rows.length; i++) {
                    var labels = metricLabels(rows[i])
                    if (runs[labels.run_id] && phaseMap[labels.phase])
                        runs[labels.run_id][phaseMap[labels.phase]] = sampleValue(rows[i])
                }
                requestQuery("grin_benchmark_final_height", function(rows) {
                    for (var i = 0; i < rows.length; i++) {
                        var labels = metricLabels(rows[i])
                        if (runs[labels.run_id])
                            runs[labels.run_id].final_height = Math.round(sampleValue(rows[i]))
                    }
                    var list = []
                    for (var key in runs)
                        list.push(runs[key])
                    list.sort(function(a, b) { return Number(b.id || 0) - Number(a.id || 0) })
                    if (list.length > 25)
                        list = list.slice(0, 25)
                    var success = 0
                    var running = 0
                    var failed = 0
                    for (var i = 0; i < list.length; i++) {
                        if (list[i].result === "success")
                            success++
                        else if (list[i].result === "running")
                            running++
                        else if (list[i].result)
                            failed++
                    }
                    benchmarkData = {
                        updated_at: Date.now() / 1000,
                        summary: {
                            run_count: list.length,
                            success_count: success,
                            running_count: running,
                            failed_count: failed
                        },
                        runs: list
                    }
                })
            })
        })
    }

    function buildNotifications() {
        requestQuery("grin_runtime_notification_info", function(rows) {
            var list = []
            for (var i = 0; i < rows.length; i++) {
                var labels = metricLabels(rows[i])
                var isActive = labels.active === "1" || labels.active === "true"
                list.push({
                    id: labels.notification_id || "",
                    active: isActive,
                    severity: labels.severity || "",
                    category: labels.category || "",
                    node_id: labels.node_id || "",
                    node_name: labels.node_name || "",
                    sync_run_id: labels.sync_run_id || "",
                    sync_state: labels.sync_state || "",
                    progress_stage: labels.progress_stage || "",
                    title: labels.title || "",
                    message: labels.message || "",
                    created_at: labels.created_at || "",
                    updated_at: labels.updated_at || "",
                    resolved_at: labels.resolved_at || ""
                })
            }
            list.sort(function(a, b) {
                var timeA = Date.parse(a.created_at || a.updated_at || "")
                var timeB = Date.parse(b.created_at || b.updated_at || "")
                if (!isNaN(timeA) && !isNaN(timeB) && timeA !== timeB)
                    return timeB - timeA
                if (a.created_at !== b.created_at)
                    return a.created_at > b.created_at ? -1 : 1
                var nodeA = a.node_name || a.node_id || ""
                var nodeB = b.node_name || b.node_id || ""
                if (nodeA !== nodeB)
                    return nodeA > nodeB ? 1 : -1
                return 0
            })
            if (list.length > maxNotifications)
                list = list.slice(0, maxNotifications)

            var active = 0
            var warnings = 0
            var errors = 0
            for (var j = 0; j < list.length; j++) {
                if (list[j].active)
                    active++
                if (list[j].severity === "warning")
                    warnings++
                if (list[j].severity === "error")
                    errors++
            }

            notificationData = {
                updated_at: Date.now() / 1000,
                summary: {
                    notification_count: list.length,
                    active_count: active,
                    warning_count: warnings,
                    error_count: errors
                },
                notifications: list
            }
        })
    }

    function buildSetup() {
        var nodes = {}
        requestQuery("grin_runtime_autosync_enabled", function(rows) {
            for (var i = 0; i < rows.length; i++) {
                var labels = metricLabels(rows[i])
                var nodeId = labels.node_id || labels.node_name || ""
                nodes[nodeId] = {
                    node_id: nodeId,
                    node_name: labels.node_name || nodeId,
                    node_type: labels.node_type || "",
                    profile: labels.profile || "",
                    status: labels.status || "",
                    autosync_enabled: sampleValue(rows[i]) > 0,
                    frequent_restart_enabled: false,
                    frequent_restart_last_bucket: "",
                    failure_state: labels.failure_state || "",
                    docker_image: labels.docker_image || "",
                    image_tag: labels.image_tag || "",
                    sync_run_id: "",
                    last_action: ""
                }
            }
            requestQuery("grin_runtime_frequent_restart_enabled", function(restartRows) {
                for (var i = 0; i < restartRows.length; i++) {
                    var labels = metricLabels(restartRows[i])
                    var nodeId = labels.node_id || labels.node_name || ""
                    if (!nodes[nodeId])
                        continue
                    nodes[nodeId].frequent_restart_enabled = sampleValue(restartRows[i]) > 0
                    nodes[nodeId].frequent_restart_last_bucket = labels.last_bucket || ""
                }
                var list = nodeListFromMap(nodes)
                setupData = {
                    updated_at: Date.now() / 1000,
                    source: "prometheus",
                    summary: {
                        node_count: list.length
                    },
                    nodes: list
                }
            })
        })
    }

    function refresh() {
        loading = true
        errorText = ""
        buildRuntime()
        buildBenchmarks()
        buildNotifications()
        buildSetup()
    }

    Component.onCompleted: initialRefreshTimer.start()

    Timer {
        id: initialRefreshTimer
        interval: Qt.platform.os === "wasm" ? 350 : 0
        repeat: false
        onTriggered: root.refresh()
    }

    Timer {
        interval: 10000
        repeat: true
        running: true
        onTriggered: root.refresh()
    }

    Rectangle {
        anchors.fill: parent
        color: "#05070b"
    }

    Image {
        anchors.fill: parent
        source: root.assetPath("media/images/image_wallpaper_tile.png")
        fillMode: Image.PreserveAspectCrop
        smooth: true
        asynchronous: true
    }

    Rectangle {
        anchors.fill: parent
        color: "#77080d14"
    }

    Rectangle {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: compact ? 64 : 76
        color: root.panel
        border.color: root.border
        z: 10

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: root.pageGutter
            anchors.rightMargin: root.pageGutter
            spacing: 12

            Rectangle {
                implicitWidth: compact ? 84 : 104
                implicitHeight: compact ? 38 : 42
                radius: 14
                color: backMouse.containsMouse ? "#18ffffff" : "#0dffffff"
                border.color: root.border

                Text {
                    anchors.centerIn: parent
                    text: i18n ? i18n.tf("back", "Back") : "Back"
                    color: "#f4f7ff"
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }

                MouseArea {
                    id: backMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.backRequested()
                }
            }

            Label {
                text: i18n ? i18n.tf("runtime_monitor_title", "Runtime Monitor") : "Runtime Monitor"
                color: "#ffffff"
                font.pixelSize: compact ? 20 : 26
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Button {
                text: compact ? "↻" : (i18n ? i18n.tf("runtime_monitor_refresh", "Refresh") : "Refresh")
                Layout.preferredWidth: compact ? 44 : implicitWidth
                Layout.preferredHeight: compact ? 38 : implicitHeight
                onClicked: root.refresh()
            }
        }
    }

    Flickable {
        id: scrollView
        anchors.fill: parent
        anchors.topMargin: topBar.height
        contentWidth: width
        contentHeight: contentColumn.implicitHeight + 50
        clip: true

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Column {
            id: contentColumn
            width: Math.min(scrollView.width - (compact ? 20 : 48), 1180)
            x: Math.round((scrollView.width - width) / 2)
            y: compact ? 16 : 24
            spacing: 16

            Rectangle {
                width: parent.width
                radius: 22
                color: root.panel
                border.color: root.border
                implicitHeight: introColumn.implicitHeight + 28

                Column {
                    id: introColumn
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Label {
                        text: i18n ? i18n.tf("runtime_monitor_eyebrow", "Read-only operations view") : "Read-only operations view"
                        color: "#aeb7c8"
                        font.pixelSize: 13
                        font.bold: true
                    }

                    Runtime.RuntimeMonitorTabs {
                        width: parent.width
                        activeView: root.activeView
                        onViewRequested: function(view) {
                            root.activeView = view
                        }
                    }

                    Label {
                        visible: root.errorText.length > 0
                        width: parent.width
                        text: root.errorText
                        color: "#ff9aa8"
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Runtime.RuntimeMonitorRuntimeView {
                visible: root.activeView === "runtime"
                width: parent.width
                compact: root.compact
                runtimeData: root.runtimeData
                panel: root.panel
                panelSoft: root.panelSoft
                borderColor: root.border
            }

            Runtime.RuntimeMonitorBenchmarksView {
                visible: root.activeView === "benchmarks"
                width: parent.width
                compact: root.compact
                benchmarkData: root.benchmarkData
                panel: root.panel
                panelSoft: root.panelSoft
                borderColor: root.border
            }

            Runtime.RuntimeMonitorNotificationsView {
                visible: root.activeView === "notifications"
                width: parent.width
                compact: root.compact
                notificationData: root.notificationData
                panel: root.panel
                panelSoft: root.panelSoft
                borderColor: root.border
            }

            Runtime.RuntimeMonitorSetupView {
                visible: root.activeView === "setup"
                width: parent.width
                compact: root.compact
                setupData: root.setupData
                panel: root.panel
                borderColor: root.border
            }
        }
    }
}
