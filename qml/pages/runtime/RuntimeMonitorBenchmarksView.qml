import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Column {
    id: view
    property bool compact: false
    property var benchmarkData: ({ summary: {}, runs: [] })
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

    function seconds(value) {
        var n = Number(value)
        if (!isFinite(n))
            return "-"
        if (n < 60)
            return n.toFixed(1) + "s"
        return (n / 60).toFixed(1) + "m"
    }

    RuntimeMonitorMetricGrid {
        width: parent.width
        compact: parent.compact
        panelSoft: parent.panelSoft
        borderColor: parent.borderColor
        metrics: [
            { label: "Runs", value: parent.fmt(parent.benchmarkData.summary.run_count, "0") },
            { label: "Success", value: parent.fmt(parent.benchmarkData.summary.success_count, "0") },
            { label: "Running", value: parent.fmt(parent.benchmarkData.summary.running_count, "0") },
            { label: "Failed", value: parent.fmt(parent.benchmarkData.summary.failed_count, "0") }
        ]
    }

    Rectangle {
        width: parent.width
        radius: 16
        color: panel
        border.color: view.borderColor
        implicitHeight: benchmarkColumn.implicitHeight + 26

        Column {
            id: benchmarkColumn
            anchors.fill: parent
            anchors.margins: 14
            spacing: 8

            Label {
                text: "Benchmark History"
                color: "#ffffff"
                font.pixelSize: 20
                font.bold: true
            }

            Flickable {
                id: benchmarkTable
                width: parent.width
                height: benchmarkRows.implicitHeight
                contentWidth: compact ? Math.max(width, 740) : width
                contentHeight: benchmarkRows.implicitHeight
                clip: true
                flickableDirection: Flickable.HorizontalFlick

                ScrollBar.horizontal: ScrollBar { policy: compact ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }

                Column {
                    id: benchmarkRows
                    width: benchmarkTable.contentWidth
                    spacing: 8

                    Repeater {
                        model: benchmarkData.runs || []

                        RowLayout {
                            width: benchmarkRows.width
                            spacing: 10

                            Label { text: fmt(modelData.node_name); color: "#dfe5ff"; Layout.preferredWidth: 150; elide: Text.ElideRight }
                            Label { text: fmt(modelData.node_type); color: "#aeb7c8"; Layout.preferredWidth: 92; elide: Text.ElideRight }
                            Label { text: fmt(modelData.result); color: modelData.result === "success" ? "#7fd276" : "#f2cc0c"; Layout.preferredWidth: 84; elide: Text.ElideRight }
                            Label { text: seconds(modelData.total_sync_duration); color: "#ffffff"; Layout.preferredWidth: 90; horizontalAlignment: Text.AlignRight }
                            Label { text: fmt(modelData.final_height); color: "#ffffff"; Layout.preferredWidth: 100; horizontalAlignment: Text.AlignRight }
                            Label { text: fmt(modelData.sync_completed_at || modelData.sync_started_at); color: "#aeb7c8"; Layout.fillWidth: true; elide: Text.ElideRight }
                        }
                    }
                }
            }
        }
    }
}
