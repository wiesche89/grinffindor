// Footer.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

Rectangle {
    id: footer

    property Window window: null
    property real baseHeight: 64

    height: window ? Math.max(baseHeight, window.height * 0.06) : baseHeight
    width: window ? window.width : (parent ? parent.width : implicitWidth)

    color: "#05050500"
    border.width: 0
    border.color: "#ffffff11"

    // ---------- Layout ----------
    RowLayout {
        id: rootRow
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        // Left stats
        ColumnLayout {
            spacing: 2
            Label { text: qsTr("height"); font.pixelSize: 10; color: "#7c7c7c" }
            Label { text: qsTr("5,016,784"); font.pixelSize: 14; color: "#ffffff" }
        }

        ColumnLayout {
            spacing: 2
            Label { text: qsTr("network"); font.pixelSize: 10; color: "#7c7c7c" }
            Label { text: qsTr("Mainnet"); font.pixelSize: 14; color: "#ffffff" }
        }

        ColumnLayout {
            spacing: 2
            Label { text: qsTr("last_block_time"); font.pixelSize: 10; color: "#7c7c7c" }
            Label { text: qsTr("2m ago"); font.pixelSize: 14; color: "#ffffff" }
        }

        // Spacer
        Item { Layout.fillWidth: true }

        // Right icons (ultra sharp)
        RowLayout {
            id: iconsRow
            spacing: 10

            Repeater {
                model: [
                    { icon: "qrc:/res/media/icons/IconX.svg",        label: qsTr("X"),        url: "https://twitter.com/grinffindor" },
                    { icon: "qrc:/res/media/icons/IconTelegram.svg", label: qsTr("Telegram"), url: "https://t.me/grinffindor" },
                    { icon: "qrc:/res/media/icons/IconGrin.svg",     label: qsTr("Forum"),    url: "https://forum.grinffindor" }
                ]

                Item {
                    id: iconItem
                    width: 28
                    height: 28

                    // hover fade
                    opacity: mouse.containsMouse ? 0.75 : 1.0

                    // crisp SVG rasterization
                    Image {
                        source: modelData.icon
                        width: 24
                        height: 24
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true            // Qt6: wichtig für Downscale-Schärfe
                        asynchronous: false     // bei Icons besser aus
                        cache: true

                        readonly property real dpr: Screen.devicePixelRatio
                        sourceSize.width:  Math.round(width  * dpr * 8)   // 8x oversample
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
                            console.log(modelData.label + " clicked (" + modelData.url + ")")
                            // Optional: Qt.openUrlExternally(modelData.url)
                        }
                    }
                }
            }
        }
    }
}
