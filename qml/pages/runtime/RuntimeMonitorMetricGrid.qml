import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

GridLayout {
    id: grid
    property bool compact: false
    property var metrics: []
    property color panelSoft: "#cc131c28"
    property color borderColor: "#406b7380"
    property color valueColor: "#7fd276"

    width: parent ? parent.width : implicitWidth
    columns: compact ? 2 : Math.max(1, metrics.length)
    columnSpacing: 10
    rowSpacing: 10

    Repeater {
        model: metrics

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 92
            radius: 14
            color: grid.panelSoft
            border.color: grid.borderColor

            Column {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 8

                Label {
                    text: modelData.label
                    color: "#aeb7c8"
                    font.pixelSize: 13
                    font.bold: true
                }

                Label {
                    text: modelData.value
                    color: valueColor
                    font.pixelSize: 34
                    font.bold: true
                }
            }
        }
    }
}
