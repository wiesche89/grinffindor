import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: panel

    property string title: ""
    property string description: ""
    property color fillColor: "#132635"
    property color strokeColor: "#2a4f64"
    property color titleColor: "#ffffff"
    property color descriptionColor: "#d7e9f4"
    property int contentPadding: 12
    property int contentSpacing: 8

    default property alias contentData: body.data

    radius: 18
    color: fillColor
    border.color: strokeColor
    implicitHeight: panelColumn.implicitHeight + contentPadding * 2

    ColumnLayout {
        id: panelColumn
        anchors.fill: parent
        anchors.margins: panel.contentPadding
        spacing: panel.contentSpacing

        Label {
            Layout.fillWidth: true
            visible: panel.title.length > 0
            text: panel.title
            color: panel.titleColor
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            visible: panel.description.length > 0
            text: panel.description
            color: panel.descriptionColor
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            spacing: panel.contentSpacing
        }
    }
}