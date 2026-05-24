import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Column {
    id: view
    property bool compact: false
    property var notificationData: ({ summary: {}, notifications: [] })
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

    RuntimeMonitorMetricGrid {
        width: parent.width
        compact: parent.compact
        panelSoft: parent.panelSoft
        borderColor: parent.borderColor
        valueColor: "#f2cc0c"
        metrics: [
            { label: "Active", value: parent.fmt(parent.notificationData.summary.active_count, "0") },
            { label: "Errors", value: parent.fmt(parent.notificationData.summary.error_count, "0") },
            { label: "Warnings", value: parent.fmt(parent.notificationData.summary.warning_count, "0") },
            { label: "Total", value: parent.fmt(parent.notificationData.summary.notification_count, "0") }
        ]
    }

    Rectangle {
        width: parent.width
        radius: 16
        color: panel
        border.color: view.borderColor
        implicitHeight: notificationColumn.implicitHeight + 26

        Column {
            id: notificationColumn
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            Label {
                text: "Notifications"
                color: "#ffffff"
                font.pixelSize: 20
                font.bold: true
            }

            Label {
                visible: (notificationData.notifications || []).length === 0
                text: "No runtime notifications from Prometheus."
                color: "#aeb7c8"
                font.pixelSize: 15
            }

            Repeater {
                model: notificationData.notifications || []

                Rectangle {
                    id: notificationItem
                    width: parent.width
                    radius: 14
                    color: "#d00d121b"
                    border.color: view.borderColor
                    implicitHeight: notificationItemColumn.implicitHeight + 22
                    property bool expanded: false

                    Column {
                        id: notificationItemColumn
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 6

                        RowLayout {
                            width: parent.width

                            Label {
                                text: fmt(modelData.title, fmt(modelData.category))
                                color: "#ffffff"
                                font.pixelSize: 17
                                font.bold: true
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            Label {
                                text: fmt(modelData.severity) + (modelData.active ? "" : " resolved")
                                color: modelData.severity === "error" ? "#ff6b6b" : (modelData.severity === "warning" ? "#f2cc0c" : "#7fd276")
                                font.bold: true
                            }
                        }

                        Label {
                            width: parent.width
                            text: fmt(modelData.created_at) + " | " + fmt(modelData.node_name || modelData.node_id) + " | " + fmt(modelData.sync_state) + " | " + fmt(modelData.progress_stage)
                            color: "#cfd6ff"
                            elide: Text.ElideRight
                        }

                        Label {
                            width: parent.width
                            text: fmt(modelData.message)
                            color: "#aeb7c8"
                            wrapMode: Text.WordWrap
                            maximumLineCount: notificationItem.expanded ? 999 : 2
                            elide: Text.ElideRight
                        }

                        Button {
                            visible: fmt(modelData.message).length > 180
                            text: notificationItem.expanded ? "Hide details" : "Show details"
                            onClicked: notificationItem.expanded = !notificationItem.expanded
                        }
                    }
                }
            }
        }
    }
}
