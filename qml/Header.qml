import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: header
    property Window window: null
    property var i18n: null
    property real baseHeight: 82
    signal settingsRequested()
    height: window ? Math.max(baseHeight, window.height * 0.075) : baseHeight
    width: parent ? parent.width : (window ? window.width : implicitWidth)
    color: "#12121200"
    border.color: "#ffffff22"
    border.width: 0

    Label {
        text: i18n ? i18n.tf("app_title", "Grinffindor") : "Grinffindor"
        font.pixelSize: 24
        font.bold: true
        color: "#f8f8f8"
        x: Math.round((header.width - width) / 2)
        anchors.top: parent.top
        anchors.topMargin: window ? Math.round(window.height * 0.04) : 22
    }

    ToolButton {
        text: "\u2699"
        font.pixelSize: 22
        opacity: hovered ? 0.72 : 1.0
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 18
        anchors.topMargin: window ? Math.round(window.height * 0.04) - 2 : 20
        padding: 4

        background: Rectangle {
            radius: 14
            color: "transparent"
            border.width: 0
        }

        contentItem: Text {
            text: parent.text
            font: parent.font
            color: "#f8f8f8"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        onClicked: header.settingsRequested()
    }
}
