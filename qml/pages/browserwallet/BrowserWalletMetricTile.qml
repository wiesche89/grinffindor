import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: tile

    readonly property bool veryCompactWidth: width > 0 && width < 150
    readonly property bool compactWidth: width > 0 && width < 200

    property string title: ""
    property string value: ""

    Layout.fillWidth: true
    radius: veryCompactWidth ? 12 : (compactWidth ? 13 : 14)
    color: "#091825"
    border.color: "#152c3e"
    border.width: 1
    implicitHeight: veryCompactWidth ? 62 : (compactWidth ? 68 : 76)

    Column {
        anchors.fill: parent
        anchors.leftMargin: veryCompactWidth ? 10 : (compactWidth ? 12 : 14)
        anchors.rightMargin: veryCompactWidth ? 10 : (compactWidth ? 12 : 14)
        anchors.topMargin: veryCompactWidth ? 10 : 12
        spacing: 4

        Label {
            text: tile.title
            color: "#3e7090"
            font.pixelSize: veryCompactWidth ? 10 : (compactWidth ? 11 : 11)
            font.letterSpacing: 0.6
        }

        Label {
            width: parent.width
            text: tile.value
            color: "#b8d8f0"
            wrapMode: Text.WordWrap
            font.pixelSize: veryCompactWidth ? 14 : (compactWidth ? 15 : 16)
            font.weight: Font.DemiBold
        }
    }
}
