import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: panel

    readonly property bool veryCompactWidth: width > 0 && width < 420
    readonly property bool compactWidth: width > 0 && width < 560

    property string title: ""
    property string description: ""
    property color fillColor: "#0f2030"
    property color strokeColor: "#1a3448"
    property color titleColor: "#c0d8ee"
    property color descriptionColor: "#5a8eaa"
    property int contentPadding: veryCompactWidth ? 10 : (compactWidth ? 12 : 14)
    property int contentSpacing: veryCompactWidth ? 6 : (compactWidth ? 7 : 9)

    default property alias contentData: body.data

    radius: veryCompactWidth ? 11 : (compactWidth ? 12 : 14)
    color: fillColor
    border.color: strokeColor
    border.width: 1
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
            font.pixelSize: veryCompactWidth ? 13 : (compactWidth ? 13 : 14)
            font.weight: Font.DemiBold
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            visible: panel.description.length > 0
            text: panel.description
            color: panel.descriptionColor
            font.pixelSize: veryCompactWidth ? 12 : (compactWidth ? 12 : 13)
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            spacing: panel.contentSpacing
        }
    }
}
