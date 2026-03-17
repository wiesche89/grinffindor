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
                        text: i18n ? i18n.tf("umbrel_page_eyebrow", "Grin infrastructure at home") : "Grin infrastructure at home"
                        color: "#b8becf"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("umbrel_page_title", "Grinffindor on UmbrelOS") : "Grinffindor on UmbrelOS"
                        font.pixelSize: scrollView.width < 700 ? 30 : 42
                        font.bold: true
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width * 0.9
                        text: i18n ? i18n.tf("umbrel_page_intro", "Grinffindor brings the power of Grin infrastructure directly into your home server through a custom UmbrelOS integration.") : "Grinffindor brings the power of Grin infrastructure directly into your home server through a custom UmbrelOS integration."
                        color: "#e8ebff"
                        font.pixelSize: 18
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width * 0.9
                        text: i18n ? i18n.tf("umbrel_page_store_intro", "With our dedicated Community App Store, you can easily install and manage Grin-related services in just a few clicks - no complex setup required.") : "With our dedicated Community App Store, you can easily install and manage Grin-related services in just a few clicks - no complex setup required."
                        color: "#d8ddff"
                        font.pixelSize: 16
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
                implicitHeight: storeColumn.implicitHeight + 30

                Column {
                    id: storeColumn
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 10

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("umbrel_store_cta_title", "Open Grinffindor Umbrel App Store") : "Open Grinffindor Umbrel App Store"
                        color: "#ffffff"
                        font.pixelSize: 22
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("umbrel_store_cta_note", "Template / concept basis for community stores") : "Template / concept basis for community stores"
                        color: "#c4cada"
                        font.pixelSize: 14
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
                            title: i18n ? i18n.tf("umbrel_feature_os_title", "What is UmbrelOS?") : "What is UmbrelOS?",
                            body: i18n ? i18n.tf("umbrel_feature_os_body", "UmbrelOS is a modern self-hosted home server operating system that lets you run apps through a simple web interface. It is Linux-based, accessible for beginners, and powerful enough for advanced users. All applications run in isolated Docker containers, making them secure, modular, and easy to maintain.") : "UmbrelOS is a modern self-hosted home server operating system that lets you run apps through a simple web interface. It is Linux-based, accessible for beginners, and powerful enough for advanced users. All applications run in isolated Docker containers, making them secure, modular, and easy to maintain."
                        },
                        {
                            title: i18n ? i18n.tf("umbrel_feature_store_title", "Grinffindor Community App Store") : "Grinffindor Community App Store",
                            body: i18n ? i18n.tf("umbrel_feature_store_body", "Grinffindor provides its own Umbrel Community App Store, enabling the distribution of custom Grin services outside of the official Umbrel ecosystem. Community stores can be added via GitHub URL and allow independent developers to distribute apps freely without official store approval.") : "Grinffindor provides its own Umbrel Community App Store, enabling the distribution of custom Grin services outside of the official Umbrel ecosystem. Community stores can be added via GitHub URL and allow independent developers to distribute apps freely without official store approval."
                        },
                        {
                            title: i18n ? i18n.tf("umbrel_feature_install_title", "One-click Grin Services") : "One-click Grin Services",
                            body: i18n ? i18n.tf("umbrel_feature_install_body", "Install Grin nodes, tools, and services directly via the Umbrel UI, extend your Umbrel with Grin-specific functionality, and avoid manual CLI setup.") : "Install Grin nodes, tools, and services directly via the Umbrel UI, extend your Umbrel with Grin-specific functionality, and avoid manual CLI setup."
                        },
                        {
                            title: i18n ? i18n.tf("umbrel_feature_adds_title", "What Grinffindor Adds") : "What Grinffindor Adds",
                            body: i18n ? i18n.tf("umbrel_feature_adds_body", "Run Grin Mainnet and Testnet nodes directly on Umbrel, use integrated wallet and tipping services, access faucets for testing and onboarding, connect via QR codes and web interfaces, and operate a full Grin setup with minimal effort.") : "Run Grin Mainnet and Testnet nodes directly on Umbrel, use integrated wallet and tipping services, access faucets for testing and onboarding, connect via QR codes and web interfaces, and operate a full Grin setup with minimal effort."
                        }
                    ]

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.minimumHeight: 150
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
                implicitHeight: setupColumn.implicitHeight + 34

                Column {
                    id: setupColumn
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 12

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("umbrel_setup_title", "Easy Setup") : "Easy Setup"
                        color: "#ffffff"
                        font.pixelSize: 28
                        font.bold: true
                    }

                    Repeater {
                        model: [
                            i18n ? i18n.tf("umbrel_setup_step_1", "Open your Umbrel App Store.") : "Open your Umbrel App Store.",
                            i18n ? i18n.tf("umbrel_setup_step_2", "Add the Grinffindor Community App Store via GitHub URL.") : "Add the Grinffindor Community App Store via GitHub URL.",
                            i18n ? i18n.tf("umbrel_setup_step_3", "Install the desired apps with one click.") : "Install the desired apps with one click.",
                            i18n ? i18n.tf("umbrel_setup_step_4", "Start using Grin instantly.") : "Start using Grin instantly."
                        ]

                        Label {
                            width: setupColumn.width
                            text: "\u2022  " + modelData
                            color: "#f3f5ff"
                            font.pixelSize: 18
                            wrapMode: Text.WordWrap
                        }
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("umbrel_setup_note", "No deep technical knowledge required - everything runs in a clean, user-friendly interface.") : "No deep technical knowledge required - everything runs in a clean, user-friendly interface."
                        color: "#d7dbe6"
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
                implicitHeight: ownershipColumn.implicitHeight + 34

                Column {
                    id: ownershipColumn
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 12

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("umbrel_ownership_title", "Self-Hosted. Private. Yours.") : "Self-Hosted. Private. Yours."
                        color: "#ffffff"
                        font.pixelSize: 28
                        font.bold: true
                    }

                    Repeater {
                        model: [
                            i18n ? i18n.tf("umbrel_ownership_point_1", "No centralized services") : "No centralized services",
                            i18n ? i18n.tf("umbrel_ownership_point_2", "Full ownership of your node and funds") : "Full ownership of your node and funds",
                            i18n ? i18n.tf("umbrel_ownership_point_3", "Privacy-first infrastructure") : "Privacy-first infrastructure",
                            i18n ? i18n.tf("umbrel_ownership_point_4", "Plug-and-play experience") : "Plug-and-play experience"
                        ]

                        Label {
                            width: ownershipColumn.width
                            text: "\u2022  " + modelData
                            color: "#f3f5ff"
                            font.pixelSize: 18
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }
    }
}
