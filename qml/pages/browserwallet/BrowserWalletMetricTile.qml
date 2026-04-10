import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: tile

    readonly property bool veryCompactWidth: width > 0 && width < 150
    readonly property bool compactWidth: width > 0 && width < 180

    property string title: ""
    property string value: ""

    Layout.fillWidth: true
    radius: veryCompactWidth ? 14 : (compactWidth ? 16 : 20)
    color: "#0e1b27"
    border.color: "#26465b"
    implicitHeight: veryCompactWidth ? 68 : (compactWidth ? 76 : 88)

    Column {
        anchors.fill: parent
        anchors.margins: veryCompactWidth ? 10 : (compactWidth ? 12 : 14)
        spacing: 4

        Label {
            text: tile.title
            color: "#8fb4c9"
            font.pixelSize: veryCompactWidth ? 11 : (compactWidth ? 12 : 13)
        }

        Label {
            width: parent.width
            text: tile.value
            color: "#ffffff"
            wrapMode: Text.WordWrap
            font.pixelSize: veryCompactWidth ? 16 : (compactWidth ? 18 : 22)
            font.weight: Font.Bold
        }
    }
}