// Footer.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

Rectangle {
    id: footer

    property Window window: null
    property var i18n: null
    property var nodeStatus: null
    readonly property bool compact: width < 760
    property real baseHeight: compact ? 120 : 72

    height: window ? Math.max(baseHeight, window.height * (compact ? 0.15 : 0.085)) : baseHeight
    width: window ? window.width : (parent ? parent.width : implicitWidth)

    color: "#05050500"
    border.width: 0
    border.color: "#ffffff11"

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: compact ? 20 : 130
        anchors.topMargin: compact ? 10 : 14
        anchors.bottomMargin: compact ? 10 : 14
        spacing: compact ? 10 : 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 24

            ColumnLayout {
                spacing: 2

                Label {
                    text: i18n ? i18n.tf("footer_mainnet", "Mainnet") : "Mainnet"
                    font.pixelSize: 12
                    font.bold: true
                    color: nodeStatus && nodeStatus.mainnetAvailable ? "#62d67a" : "#ff6b6b"
                }

                Label {
                    text: (i18n ? i18n.tf("footer_tip", "Tip") : "Tip") + ": " + (nodeStatus ? nodeStatus.mainnetTip : "...")
                    font.pixelSize: 12
                    color: "#cfd6ff"
                }

                Label {
                    text: (i18n ? i18n.tf("footer_version", "Version") : "Version") + ": " + (nodeStatus ? nodeStatus.mainnetVersion : "...")
                    font.pixelSize: 12
                    color: "#cfd6ff"
                }
            }

            ColumnLayout {
                spacing: 2

                Label {
                    text: i18n ? i18n.tf("footer_testnet", "Testnet") : "Testnet"
                    font.pixelSize: 12
                    font.bold: true
                    color: nodeStatus && nodeStatus.testnetAvailable ? "#62d67a" : "#ff6b6b"
                }

                Label {
                    text: (i18n ? i18n.tf("footer_tip", "Tip") : "Tip") + ": " + (nodeStatus ? nodeStatus.testnetTip : "...")
                    font.pixelSize: 12
                    color: "#cfd6ff"
                }

                Label {
                    text: (i18n ? i18n.tf("footer_version", "Version") : "Version") + ": " + (nodeStatus ? nodeStatus.testnetVersion : "...")
                    font.pixelSize: 12
                    color: "#cfd6ff"
                }
            }
        }

    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 20
        anchors.verticalCenter: parent.verticalCenter
        spacing: 10

        Repeater {
            model: [
                { icon: "qrc:/res/media/icons/IconX.svg", label: qsTr("X"), url: "https://x.com/wiesche89" },
                { icon: "qrc:/res/media/icons/IconTelegram.svg", label: qsTr("Telegram"), url: "https://t.me/+v_Q4TF8uzFVhYjg6" },
                { icon: "qrc:/res/media/icons/IconGrin.svg", label: qsTr("Grin"), url: "https://grin.mw" }
            ]

            Item {
                width: 28
                height: 28
                opacity: mouse.containsMouse ? 0.75 : 1.0

                Image {
                    source: modelData.icon
                    width: 24
                    height: 24
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                    asynchronous: false
                    cache: true

                    readonly property real dpr: Screen.devicePixelRatio
                    sourceSize.width: Math.round(width * dpr * 8)
                    sourceSize.height: Math.round(height * dpr * 8)

                    x: Math.round((parent.width - width) / 2)
                    y: Math.round((parent.height - height) / 2)
                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor

                    onClicked: {
                        Qt.openUrlExternally(modelData.url)
                    }
                }
            }
        }
    }
}
