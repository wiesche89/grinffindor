import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: view
    property bool compact: false
    property var setupData: ({ summary: {}, nodes: [] })
    property color panel: "#d80b1018"
    property color borderColor: "#406b7380"

    width: parent ? parent.width : implicitWidth
    radius: 16
    color: panel
    border.color: view.borderColor
    implicitHeight: setupColumn.implicitHeight + 26

    function fmt(value, fallback) {
        if (value === undefined || value === null || value === "")
            return fallback === undefined ? "-" : fallback
        return String(value)
    }

    Column {
        id: setupColumn
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        Label {
            text: "Controller Setup"
            color: "#ffffff"
            font.pixelSize: 20
            font.bold: true
        }

        Label {
            width: parent.width
            text: "Prometheus-only view. Fields appear when the Runtime Controller exports them as labels or metrics."
            color: "#aeb7c8"
            wrapMode: Text.WordWrap
        }

        Flickable {
            id: setupTable
            width: parent.width
            height: Math.min(setupRows.implicitHeight, 520)
            contentWidth: compact ? Math.max(width, 980) : width
            contentHeight: setupRows.implicitHeight
            clip: true
            flickableDirection: Flickable.HorizontalFlick | Flickable.VerticalFlick

            ScrollBar.horizontal: ScrollBar { policy: compact ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            Column {
                id: setupRows
                width: setupTable.contentWidth
                spacing: 8

                RowLayout {
                    width: setupRows.width
                    spacing: 10

                    Label { text: "Node"; color: "#aeb7c8"; Layout.preferredWidth: 150; font.bold: true }
                    Label { text: "Type"; color: "#aeb7c8"; Layout.preferredWidth: 90; font.bold: true }
                    Label { text: "Profile"; color: "#aeb7c8"; Layout.preferredWidth: 90; font.bold: true }
                    Label { text: "Status"; color: "#aeb7c8"; Layout.preferredWidth: 90; font.bold: true }
                    Label { text: "Autosync"; color: "#aeb7c8"; Layout.preferredWidth: 90; font.bold: true }
                    Label { text: "Frequent Restart"; color: "#aeb7c8"; Layout.preferredWidth: 140; font.bold: true }
                    Label { text: "Failure"; color: "#aeb7c8"; Layout.preferredWidth: 120; font.bold: true }
                    Label { text: "Image"; color: "#aeb7c8"; Layout.fillWidth: true; font.bold: true }
                }

                Repeater {
                    model: setupData.nodes || []

                    RowLayout {
                        width: setupRows.width
                        spacing: 10

                        Label { text: fmt(modelData.node_name || modelData.node_id); color: "#dfe5ff"; Layout.preferredWidth: 150; elide: Text.ElideRight }
                        Label { text: fmt(modelData.node_type); color: "#ffffff"; Layout.preferredWidth: 90; elide: Text.ElideRight }
                        Label { text: fmt(modelData.profile); color: "#ffffff"; Layout.preferredWidth: 90; elide: Text.ElideRight }
                        Label { text: fmt(modelData.status); color: "#ffffff"; Layout.preferredWidth: 90; elide: Text.ElideRight }
                        Label { text: modelData.autosync_enabled ? "enabled" : "disabled"; color: modelData.autosync_enabled ? "#7fd276" : "#aeb7c8"; Layout.preferredWidth: 90; elide: Text.ElideRight }
                        Label { text: modelData.frequent_restart_enabled ? "enabled " + fmt(modelData.frequent_restart_last_bucket) : "disabled"; color: modelData.frequent_restart_enabled ? "#f2cc0c" : "#aeb7c8"; Layout.preferredWidth: 140; elide: Text.ElideRight }
                        Label { text: fmt(modelData.failure_state); color: modelData.failure_state === "ok" ? "#7fd276" : "#ff9aa8"; Layout.preferredWidth: 120; elide: Text.ElideRight }
                        Label { text: fmt((modelData.docker_image || "") + (modelData.image_tag ? ":" + modelData.image_tag : "")); color: "#ffffff"; Layout.fillWidth: true; elide: Text.ElideRight }
                    }
                }
            }
        }
    }
}
