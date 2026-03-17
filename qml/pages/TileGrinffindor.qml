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
                        text: i18n ? i18n.tf("grinffindor_page_eyebrow", "Gateway to Grin") : "Gateway to Grin"
                        color: "#b8becf"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("grinffindor_page_title", "Welcome to Grinffindor") : "Welcome to Grinffindor"
                        font.pixelSize: scrollView.width < 700 ? 30 : 42
                        font.bold: true
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width * 0.9
                        text: i18n ? i18n.tf("grinffindor_page_intro", "Grinffindor is your gateway into the world of Grin and Mimblewimble - a community-driven platform built to make privacy-focused cryptocurrency accessible, practical, and easy to use.") : "Grinffindor is your gateway into the world of Grin and Mimblewimble - a community-driven platform built to make privacy-focused cryptocurrency accessible, practical, and easy to use."
                        color: "#e8ebff"
                        font.pixelSize: 18
                        wrapMode: Text.WordWrap
                    }
                }
            }

            GridLayout {
                width: parent.width
                columns: scrollView.width < 860 ? 1 : 2
                columnSpacing: 16
                rowSpacing: 16

                Repeater {
                    model: [
                        {
                            title: i18n ? i18n.tf("grinffindor_feature_seed_title", "Reliable Seed Nodes") : "Reliable Seed Nodes",
                            body: i18n ? i18n.tf("grinffindor_feature_seed_body", "Mainnet and Testnet seed infrastructure to support a stable and decentralized Grin network.") : "Mainnet and Testnet seed infrastructure to support a stable and decentralized Grin network."
                        },
                        {
                            title: i18n ? i18n.tf("grinffindor_feature_umbrel_title", "UmbrelOS Guides") : "UmbrelOS Guides",
                            body: i18n ? i18n.tf("grinffindor_feature_umbrel_body", "Run your own Grin node at home with straightforward setup guidance and practical documentation.") : "Run your own Grin node at home with straightforward setup guidance and practical documentation."
                        },
                        {
                            title: i18n ? i18n.tf("grinffindor_feature_testnet_title", "Testnet Faucet and Tipping Bot") : "Testnet Faucet and Tipping Bot",
                            body: i18n ? i18n.tf("grinffindor_feature_testnet_body", "Experiment, learn, and build without risk while getting familiar with the Grin ecosystem.") : "Experiment, learn, and build without risk while getting familiar with the Grin ecosystem."
                        },
                        {
                            title: i18n ? i18n.tf("grinffindor_feature_mainnet_title", "Mainnet Faucet and Tipping Bot") : "Mainnet Faucet and Tipping Bot",
                            body: i18n ? i18n.tf("grinffindor_feature_mainnet_body", "Bring real value into everyday interactions and make Grin more tangible in practice.") : "Bring real value into everyday interactions and make Grin more tangible in practice."
                        },
                        {
                            title: i18n ? i18n.tf("grinffindor_feature_shop_title", "Upcoming Grinffindor Shop") : "Upcoming Grinffindor Shop",
                            body: i18n ? i18n.tf("grinffindor_feature_shop_body", "Curated tools, hardware, and community merchandise designed around practical Grin usage.") : "Curated tools, hardware, and community merchandise designed around practical Grin usage."
                        },
                        {
                            title: i18n ? i18n.tf("grinffindor_feature_ecosystem_title", "Built for Builders") : "Built for Builders",
                            body: i18n ? i18n.tf("grinffindor_feature_ecosystem_body", "Whether you run your first node, test transactions, or integrate Grin into your own projects, Grinffindor supports the full journey.") : "Whether you run your first node, test transactions, or integrate Grin into your own projects, Grinffindor supports the full journey."
                        }
                    ]

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.minimumHeight: 132
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
                implicitHeight: communityColumn.implicitHeight + 34

                Column {
                    id: communityColumn
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 12

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("grinffindor_community_title", "Built for the Community") : "Built for the Community"
                        color: "#ffffff"
                        font.pixelSize: 28
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("grinffindor_community_body", "Grinffindor is more than just infrastructure - it is a growing ecosystem designed to empower the Grin community. We believe in privacy, simplicity, and decentralization, and everything we build reflects these principles.") : "Grinffindor is more than just infrastructure - it is a growing ecosystem designed to empower the Grin community. We believe in privacy, simplicity, and decentralization, and everything we build reflects these principles."
                        color: "#e8ebff"
                        font.pixelSize: 16
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                width: parent.width
                radius: 26
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#17191eb8" }
                    GradientStop { position: 1.0; color: "#0e1014b8" }
                }
                border.color: "#6b738066"
                border.width: 1
                implicitHeight: journeyColumn.implicitHeight + 34

                Column {
                    id: journeyColumn
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 12

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("grinffindor_journey_title", "Start Your Journey") : "Start Your Journey"
                        color: "#ffffff"
                        font.pixelSize: 28
                        font.bold: true
                    }

                    Repeater {
                        model: [
                            i18n ? i18n.tf("grinffindor_journey_step_1", "Run your node.") : "Run your node.",
                            i18n ? i18n.tf("grinffindor_journey_step_2", "Test your first transaction.") : "Test your first transaction.",
                            i18n ? i18n.tf("grinffindor_journey_step_3", "Tip someone.") : "Tip someone.",
                            i18n ? i18n.tf("grinffindor_journey_step_4", "Be part of the network.") : "Be part of the network."
                        ]

                        Label {
                            width: journeyColumn.width
                            text: "\u2022  " + modelData
                            color: "#f3f5ff"
                            font.pixelSize: 18
                            wrapMode: Text.WordWrap
                        }
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("grinffindor_journey_footer", "Welcome to Grinffindor.") : "Welcome to Grinffindor."
                        color: "#d7dbe6"
                        font.pixelSize: 18
                        font.bold: true
                    }
                }
            }
        }
    }
}
