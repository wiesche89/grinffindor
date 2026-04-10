import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: card

    readonly property bool veryCompactWidth: width > 0 && width < 420
    readonly property bool compactWidth: width > 0 && width < 560

    property string title: ""
    property string subtitle: ""
    property color fillColor: "#0f1b26"
    property color strokeColor: "#26465b"
    property color titleColor: "#ffffff"
    property color subtitleColor: "#cbdbe4"
    property int titleSize: veryCompactWidth ? 18 : (compactWidth ? 22 : 28)
    property int contentSpacing: veryCompactWidth ? 8 : (compactWidth ? 10 : 12)
    property int contentPadding: veryCompactWidth ? 12 : (compactWidth ? 14 : 18)

    default property alias contentData: body.data

    radius: veryCompactWidth ? 16 : (compactWidth ? 20 : 26)
    color: fillColor
    border.color: strokeColor
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
            font.pixelSize: veryCompactWidth ? 12 : (compactWidth ? 13 : 14)
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            spacing: card.contentSpacing
        }
    }
}