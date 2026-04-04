import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: tile

    property string title: ""
    property string value: ""

    Layout.fillWidth: true
    radius: 20
    color: "#0e1b27"
    border.color: "#26465b"
    implicitHeight: 88

    Column {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 4

        Label {
            text: tile.title
            color: "#8fb4c9"
            font.pixelSize: 13
        }

        Label {
            width: parent.width
            text: tile.value
            color: "#ffffff"
            wrapMode: Text.WordWrap
            font.pixelSize: 22
            font.weight: Font.Bold
        }
    }
}