import QtQuick
import QtQuick.Controls

Item {
    id: navItem

    property string text: ""
    property bool active: false
    property int fontSize: 14

    signal clicked()

    implicitHeight: 40
    implicitWidth: 180

    Rectangle {
        anchors.fill: parent
        radius: 9
        color: active ? "#0c2035" : (ma.containsMouse ? "#08161f" : "transparent")
        border.color: active ? "#1e4a68" : "transparent"
        border.width: 1

        // Active state: left accent bar
        Rectangle {
            width: 3
            height: Math.round(parent.height * 0.55)
            radius: 2
            anchors.left: parent.left
            anchors.leftMargin: 7
            anchors.verticalCenter: parent.verticalCenter
            color: "#FEF102"
            visible: navItem.active
        }

        Label {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 18
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            text: navItem.text
            font.pixelSize: navItem.fontSize
            font.weight: navItem.active ? Font.DemiBold : Font.Normal
            color: {
                if (navItem.active)        return "#FEF102"
                if (ma.containsMouse)      return "#90c8e0"
                return "#4a7e98"
            }
            elide: Text.ElideRight
        }
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: navItem.clicked()
    }
}
