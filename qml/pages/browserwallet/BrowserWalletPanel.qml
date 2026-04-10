import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: panel

    readonly property bool veryCompactWidth: width > 0 && width < 420
    readonly property bool compactWidth: width > 0 && width < 560

    property string title: ""
    property string description: ""
    property color fillColor: "#132635"
    property color strokeColor: "#2a4f64"
    property color titleColor: "#ffffff"
    property color descriptionColor: "#d7e9f4"
    property int contentPadding: veryCompactWidth ? 8 : (compactWidth ? 10 : 12)
    property int contentSpacing: veryCompactWidth ? 5 : (compactWidth ? 6 : 8)

    default property alias contentData: body.data

    radius: veryCompactWidth ? 12 : (compactWidth ? 14 : 18)
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
            font.pixelSize: veryCompactWidth ? 13 : (compactWidth ? 14 : 16)
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            visible: panel.description.length > 0
            text: panel.description
            color: panel.descriptionColor
            font.pixelSize: veryCompactWidth ? 12 : (compactWidth ? 13 : 14)
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            spacing: panel.contentSpacing
        }
    }
}