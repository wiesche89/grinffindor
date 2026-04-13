import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: card

    readonly property bool veryCompactWidth: width > 0 && width < 420
    readonly property bool compactWidth: width > 0 && width < 560

    property string title: ""
    property string subtitle: ""
    property color fillColor: "#0d1c2a"
    property color strokeColor: "#182e40"
    property color titleColor: "#ddeeff"
    property color subtitleColor: "#5a8eaa"
    property int titleSize: veryCompactWidth ? 17 : (compactWidth ? 20 : 24)
    property int contentSpacing: veryCompactWidth ? 8 : (compactWidth ? 10 : 14)
    property int contentPadding: veryCompactWidth ? 14 : (compactWidth ? 16 : 20)

    default property alias contentData: body.data

    radius: veryCompactWidth ? 16 : (compactWidth ? 18 : 22)
    color: fillColor
    border.color: strokeColor
    border.width: 1
    implicitHeight: contentColumn.implicitHeight + contentPadding * 2

    ColumnLayout {
        id: contentColumn
        anchors.fill: parent
        anchors.margins: card.contentPadding
        spacing: card.contentSpacing

        Label {
            Layout.fillWidth: true
            visible: card.title.length > 0
            text: card.title
            color: card.titleColor
            font.pixelSize: card.titleSize
            font.weight: Font.Bold
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            visible: card.subtitle.length > 0
            text: card.subtitle
            color: card.subtitleColor
            font.pixelSize: veryCompactWidth ? 12 : (compactWidth ? 12 : 13)
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            spacing: card.contentSpacing
        }
    }
}
