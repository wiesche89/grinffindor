import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: card

    property string title: ""
    property string subtitle: ""
    property color fillColor: "#0f1b26"
    property color strokeColor: "#26465b"
    property color titleColor: "#ffffff"
    property color subtitleColor: "#cbdbe4"
    property int titleSize: 28
    property int contentSpacing: 12
    property int contentPadding: 18

    default property alias contentData: body.data

    radius: 26
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
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            spacing: card.contentSpacing
        }
    }
}