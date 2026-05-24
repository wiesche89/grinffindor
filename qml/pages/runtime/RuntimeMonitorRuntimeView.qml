import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Column {
    id: view
    property bool compact: false
    property var runtimeData: ({ summary: {}, nodes: [] })
    property color panel: "#d80b1018"
    property color panelSoft: "#cc131c28"
    property color borderColor: "#406b7380"

    width: parent ? parent.width : implicitWidth
    spacing: 16

    function fmt(value, fallback) {
        if (value === undefined || value === null || value === "")
            return fallback === undefined ? "-" : fallback
        return String(value)
    }

    function pct(value) {
        var n = Number(value)
        return isFinite(n) ? n.toFixed(1) + "%" : "-"
    }

    RuntimeMonitorMetricGrid {
        width: parent.width
        compact: parent.compact
        panelSoft: parent.panelSoft
        borderColor: parent.borderColor
        metrics: [
            { label: "Nodes", value: parent.fmt(parent.runtimeData.summary.node_count, "0") },
            { label: "API Up", value: parent.fmt(parent.runtimeData.summary.api_up_count, "0") },
            { label: "Syncing", value: parent.fmt(parent.runtimeData.summary.syncing_count, "0") },
            { label: "Peers", value: parent.fmt(parent.runtimeData.summary.peer_count, "0") }
        ]
    }

    GridLayout {
        width: parent.width
        columns: compact ? 1 : 2
        columnSpacing: 12
        rowSpacing: 12

        Repeater {
            model: runtimeData.nodes || []

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: nodeColumn.implicitHeight + 28
                radius: 16
                color: "#d00d121b"
                border.color: view.borderColor

                Column {
                    id: nodeColumn
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    RowLayout {
                        width: parent.width

                        Label {
                            text: modelData.node_name
                            color: "#ffffff"
                            font.pixelSize: 20
                            font.bold: true
                        }

                        Item { Layout.fillWidth: true }

                        Label {
                            text: modelData.api_up ? "up" : "down"
                            color: modelData.api_up ? "#7fd276" : "#ff5570"
                            font.bold: true
                        }
                    }

                    Label {
                        width: parent.width
                        text: fmt(modelData.node_type) + " | " + fmt(modelData.sync_status) + " | " + fmt(modelData.version)
                        color: "#cfd6ff"
                        wrapMode: Text.WordWrap
                    }

                    Rectangle {
                        width: parent.width
                        height: 10
                        radius: 5
                        color: "#26303a"

                        Rectangle {
                            width: parent.width * Math.max(0, Math.min(100, Number(modelData.sync_percent || 0))) / 100
                            height: parent.height
                            radius: parent.radius
                            color: Number(modelData.sync_percent || 0) >= 95 ? "#73bf69" : "#f2cc0c"
                        }
                    }

                    Label {
                        text: fmt(modelData.sync_stage) + "  " + pct(modelData.sync_percent)
                        color: "#f4f7ff"
                        font.bold: true
                    }

                    GridLayout {
                        width: parent.width
                        columns: 3
                        columnSpacing: 10
                        rowSpacing: 4

                        Label { text: "Height"; color: "#9da6b8" }
                        Label { text: "Connections"; color: "#9da6b8" }
                        Label { text: "Protocol"; color: "#9da6b8" }
                        Label { text: fmt(modelData.height); color: "#ffffff"; font.bold: true }
                        Label { text: fmt(modelData.connections); color: "#ffffff"; font.bold: true }
                        Label { text: fmt(modelData.protocol); color: "#ffffff"; font.bold: true }
                    }

                    Label {
                        width: parent.width
                        text: fmt(modelData.user_agent)
                        color: "#aeb7c8"
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }

    Rectangle {
        width: parent.width
        radius: 16
        color: panel
        border.color: view.borderColor
        implicitHeight: peerColumn.implicitHeight + 26

        Column {
            id: peerColumn
            anchors.fill: parent
            anchors.margins: 14
            spacing: 8

            Label {
                text: "Connected Peers"
                color: "#ffffff"
                font.pixelSize: 20
                font.bold: true
            }

            Flickable {
                id: peerTable
                width: parent.width
                height: peerRows.implicitHeight
                contentWidth: compact ? Math.max(width, 620) : width
                contentHeight: peerRows.implicitHeight
                clip: true
                flickableDirection: Flickable.HorizontalFlick

                ScrollBar.horizontal: ScrollBar { policy: compact ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }

                Column {
                    id: peerRows
                    width: peerTable.contentWidth
                    spacing: 8

                    Repeater {
                        model: {
                            var rows = []
                            var nodes = runtimeData.nodes || []
                            for (var i = 0; i < nodes.length; i++) {
                                var peers = nodes[i].peers || []
                                for (var j = 0; j < peers.length; j++)
                                    rows.push({ node: nodes[i].node_name, peer: peers[j] })
                            }
                            return rows
                        }

                        RowLayout {
                            width: peerRows.width
                            spacing: 10

                            Label { text: modelData.node; color: "#dfe5ff"; Layout.preferredWidth: 150; elide: Text.ElideRight }
                            Label { text: modelData.peer.direction; color: "#aeb7c8"; Layout.preferredWidth: 80; elide: Text.ElideRight }
                            Label { text: modelData.peer.ip; color: "#ffffff"; Layout.fillWidth: true; elide: Text.ElideRight }
                            Label { text: modelData.peer.port; color: "#aeb7c8"; Layout.preferredWidth: 58; elide: Text.ElideRight }
                            Label { text: fmt(modelData.peer.height); color: "#ffffff"; Layout.preferredWidth: 92; horizontalAlignment: Text.AlignRight }
                            Label { text: modelData.peer.user_agent; color: "#aeb7c8"; Layout.preferredWidth: 220; elide: Text.ElideRight }
                        }
                    }
                }
            }
        }
    }
}
