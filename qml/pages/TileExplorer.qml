import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    property var i18n: null
    signal backRequested()
    anchors.fill: parent
    readonly property bool compact: width < 760
    readonly property bool singleColumn: width < 980
    readonly property int pageGutter: compact ? 16 : 28
    readonly property color glassPanel: "#d9101722"
    readonly property color glassPanelSoft: "#cc131c28"
    readonly property color glassBorder: "#1fd8e1f0"

    Rectangle {
        anchors.fill: parent
        color: "#05070b"
    }

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
        color: "#660a0e14"
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#a00b1017" }
            GradientStop { position: 0.32; color: "#780e141d" }
            GradientStop { position: 0.68; color: "#880b1017" }
            GradientStop { position: 1.0; color: "#b0090d14" }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"

        Rectangle {
            width: parent.width * 0.56
            height: width
            radius: width / 2
            x: parent.width - width * 0.72
            y: -height * 0.24
            color: "#5b7cff"
            opacity: 0.035
        }

        Rectangle {
            width: parent.width * 0.42
            height: width
            radius: width / 2
            x: -width * 0.18
            y: parent.height * 0.30
            color: "#55d6ff"
            opacity: 0.03
        }
    }

    Rectangle {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: compact ? 64 : 76
        color: "#d80b1018"
        border.color: glassBorder
        z: 10

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: pageGutter
            anchors.rightMargin: pageGutter
            spacing: 14

            Rectangle {
                implicitWidth: compact ? 88 : 108
                implicitHeight: compact ? 38 : 42
                radius: 14
                color: backMouse.containsMouse ? "#14ffffff" : "#0dffffff"
                border.color: glassBorder

                Text {
                    anchors.centerIn: parent
                    text: i18n ? i18n.tf("back", "Back") : "Back"
                    color: "#f4f7ff"
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }

                MouseArea {
                    id: backMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.backRequested()
                    cursorShape: Qt.PointingHandCursor
                }
            }

            Item { Layout.fillWidth: true }
        }
    }

    Flickable {
        id: scrollView
        anchors.fill: parent
        anchors.topMargin: topBar.height
        clip: true
        contentWidth: width
        contentHeight: contentColumn.implicitHeight + 56

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        Column {
            id: contentColumn
            width: Math.min(scrollView.width - (compact ? 20 : 48), 1120)
            x: Math.round((scrollView.width - width) / 2)
            y: compact ? 16 : 28
            spacing: compact ? 20 : 28

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
                        text: i18n ? i18n.tf("explorer_page_eyebrow", "Network Visibility") : "Network Visibility"
                        color: "#b8becf"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("explorer_page_title", "Block Explorer") : "Block Explorer"
                        font.pixelSize: scrollView.width < 700 ? 30 : 42
                        font.bold: true
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width * 0.9
                        text: i18n ? i18n.tf("explorer_page_intro", "Do you want to track transactions, analyze blocks, or get an overview of the Grin network? Then use the official block explorer:") : "Do you want to track transactions, analyze blocks, or get an overview of the Grin network? Then use the official block explorer:"
                        color: "#e8ebff"
                        font.pixelSize: 18
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                width: parent.width
                radius: 24
                color: "#101014b0"
                border.color: "#6b738066"
                border.width: 1
                implicitHeight: explorerColumn.implicitHeight + 30

                Column {
                    id: explorerColumn
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 10

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("explorer_link_title", "Official Block Explorer") : "Official Block Explorer"
                        color: "#ffffff"
                        font.pixelSize: 22
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        width: parent.width
                        text: "<a href=\"" + (i18n ? i18n.tf("explorer_link_url", "https://grincoin.org") : "https://grincoin.org") + "\">"
                              + (i18n ? i18n.tf("explorer_link_label", "https://grincoin.org") : "https://grincoin.org")
                              + "</a>"
                        color: "#dfe5ff"
                        font.pixelSize: 18
                        font.bold: true
                        linkColor: "#dfe5ff"
                        wrapMode: Text.WordWrap
                        onLinkActivated: function(link) { Qt.openUrlExternally(link) }
                    }
                }
            }

            GridLayout {
                width: parent.width
                columns: root.width < 620 ? 1 : (root.singleColumn ? 2 : 3)
                columnSpacing: 16
                rowSpacing: 16

                Repeater {
                    model: [
                        {
                            title: i18n ? i18n.tf("explorer_feature_transactions_title", "Inspect Transactions") : "Inspect Transactions",
                            body: i18n ? i18n.tf("explorer_feature_transactions_body", "View transactions and verify details when you want to follow activity on the network more closely.") : "View transactions and verify details when you want to follow activity on the network more closely."
                        },
                        {
                            title: i18n ? i18n.tf("explorer_feature_blocks_title", "Track Block Heights and Network Status") : "Track Block Heights and Network Status",
                            body: i18n ? i18n.tf("explorer_feature_blocks_body", "Follow block heights and keep an eye on the current network status directly from the explorer.") : "Follow block heights and keep an eye on the current network status directly from the explorer."
                        },
                        {
                            title: i18n ? i18n.tf("explorer_feature_network_title", "Retrieve General Network Information") : "Retrieve General Network Information",
                            body: i18n ? i18n.tf("explorer_feature_network_body", "Use the explorer to access broader network information and get a quick operational overview of Grin.") : "Use the explorer to access broader network information and get a quick operational overview of Grin."
                        }
                    ]

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: explorerCardColumn.implicitHeight + 36
                        radius: 24
                        color: "#111318b0"
                        border.color: "#6b738066"
                        border.width: 1

                        Column {
                            id: explorerCardColumn
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
        }
    }
}
