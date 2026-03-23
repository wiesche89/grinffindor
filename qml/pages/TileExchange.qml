import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    property var i18n: null
    signal backRequested()
    anchors.fill: parent

    Image {
        anchors.fill: parent
        source: "qrc:/res/media/images/image_wallpaper_tile.png"
        fillMode: Image.PreserveAspectCrop
        smooth: true
        asynchronous: true
        z: 0
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#140716d9" }
            GradientStop { position: 0.45; color: "#1b0b1fcc" }
            GradientStop { position: 1.0; color: "#09040dcc" }
        }
        z: 1
    }

    Button {
        text: i18n ? i18n.tf("back", "Back") : "Back"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 18
        anchors.topMargin: 18
        onClicked: root.backRequested()
        z: 4
    }

    Flickable {
        id: scrollView
        anchors.fill: parent
        anchors.topMargin: 72
        clip: true
        contentWidth: width
        contentHeight: contentColumn.implicitHeight + 48
        z: 3

        Column {
            id: contentColumn
            width: Math.min(scrollView.width * 0.86, 980)
            x: Math.round((scrollView.width - width) / 2)
            y: 22
            spacing: 18

            Rectangle {
                width: parent.width
                radius: 30
                color: "#111318b8"
                border.color: "#6b738066"
                border.width: 1
                implicitHeight: heroColumn.implicitHeight + 34

                Column {
                    id: heroColumn
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 12

                    Label {
                        text: i18n ? i18n.tf("exchange_page_eyebrow", "Trading Access") : "Trading Access"
                        color: "#b8becf"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("exchange_page_title", "Exchange") : "Exchange"
                        font.pixelSize: scrollView.width < 700 ? 30 : 42
                        font.bold: true
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width * 0.9
                        text: i18n ? i18n.tf("exchange_page_intro", "Do you want to trade or acquire GRIN? Here you will find a selection of exchanges where GRIN is currently available:") : "Do you want to trade or acquire GRIN? Here you will find a selection of exchanges where GRIN is currently available:"
                        color: "#e8ebff"
                        font.pixelSize: 18
                        wrapMode: Text.WordWrap
                    }
                }
            }

            GridLayout {
                width: parent.width
                columns: scrollView.width < 860 ? 1 : 3
                columnSpacing: 16
                rowSpacing: 16

                Repeater {
                    model: [
                        {
                            title: "nonlongs.io",
                            url: "https://nonlongs.io",
                            body: i18n ? i18n.tf("exchange_nonlongs_body", "Access the platform directly and review its available GRIN markets, trading flow, and account requirements.") : "Access the platform directly and review its available GRIN markets, trading flow, and account requirements."
                        },
                        {
                            title: "gate.io",
                            url: "https://gate.io",
                            body: i18n ? i18n.tf("exchange_gate_body", "Check the exchange for GRIN availability, market depth, and the specific trading options offered on the platform.") : "Check the exchange for GRIN availability, market depth, and the specific trading options offered on the platform."
                        },
                        {
                            title: "noirtrade.com",
                            url: "https://noirtrade.com",
                            body: i18n ? i18n.tf("exchange_noirtrade_body", "Review the service details, supported GRIN access paths, and the platform conditions before using it.") : "Review the service details, supported GRIN access paths, and the platform conditions before using it."
                        }
                    ]

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.minimumHeight: 180
                        radius: 24
                        color: "#111318b0"
                        border.color: "#6b738066"
                        border.width: 1

                        Column {
                            anchors.fill: parent
                            anchors.margins: 18
                            spacing: 10

                            Label {
                                width: parent.width
                                text: modelData.title
                                color: "#ffffff"
                                font.pixelSize: 21
                                font.bold: true
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                width: parent.width
                                text: "<a href=\"" + modelData.url + "\">" + modelData.url + "</a>"
                                color: "#dfe5ff"
                                font.pixelSize: 16
                                linkColor: "#dfe5ff"
                                wrapMode: Text.WordWrap
                                onLinkActivated: function(link) { Qt.openUrlExternally(link) }
                            }

                            Label {
                                width: parent.width
                                text: modelData.body
                                color: "#d8ddff"
                                font.pixelSize: 15
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                radius: 26
                color: "#101014b0"
                border.color: "#6b738066"
                border.width: 1
                implicitHeight: noteColumn.implicitHeight + 34

                Column {
                    id: noteColumn
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 12

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("exchange_note_title", "Please Note") : "Please Note"
                        color: "#ffffff"
                        font.pixelSize: 28
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("exchange_note_body", "Please note that each platform offers different features, fee structures, and liquidity. Before using any exchange, review the relevant conditions and security measures carefully.") : "Please note that each platform offers different features, fee structures, and liquidity. Before using any exchange, review the relevant conditions and security measures carefully."
                        color: "#e8ebff"
                        font.pixelSize: 16
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
