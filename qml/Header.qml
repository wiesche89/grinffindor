import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: header
    property Window window: null
    property real baseHeight: 100
    property string activeItem: ""
    height: window ? Math.max(baseHeight, window.height * 0.08) : baseHeight
    width: window ? window.width : parent ? parent.width : implicitWidth
    color: "#12121200"
    border.color: "#ffffff22"
    border.width: 0

    RowLayout {
        anchors.fill: parent
        anchors.margins: 22
        spacing: 24

        Item {
            Layout.preferredWidth: 64
        }

        Label {
            text: qsTr("Grinffindor")
            font.pixelSize: 24
            font.bold: true
            color: "#f8f8f8"
        }

        Item {
            Layout.fillWidth: true
        }

        Repeater {
            model: [
                { label: qsTr("home") },
                { label: qsTr("docs") },
                { label: qsTr("about") }
            ]
            Item {
                Layout.preferredWidth: 64
                Layout.preferredHeight: parent ? parent.height : 0

                Text {
                    id: navText
                    text: modelData.label
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    anchors.centerIn: parent
                    color: header.activeItem === modelData.label ? "#9fa3a8" : "#d1d1d1"
                    opacity: header.activeItem === modelData.label ? 1 : 0.85
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        header.activeItem = modelData.label
                        navText.opacity = 1
                        console.log(modelData.label + " clicked")
                    }
                    onEntered: navText.opacity = 1
                    onExited: navText.opacity = header.activeItem === modelData.label ? 1 : 0.85
                }
            }
        }

        Item {
            Layout.preferredWidth: 70
            Layout.preferredHeight: parent ? parent.height : 0

            Text {
                id: loginText
                text: qsTr("login")
                font.pixelSize: 14
                color: "#f7f7f7"
                anchors.centerIn: parent
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onEntered: loginText.color = "#b2b2b2"
                onExited: loginText.color = "#f7f7f7"
                onClicked: console.log("login clicked")
            }
        }
    }
}
