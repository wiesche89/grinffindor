import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    property var i18n: null
    signal backRequested()
    function assetPath(path) {
        return (typeof assetBaseUrl === "string" ? assetBaseUrl : "qrc:/res/") + path
    }
    anchors.fill: parent
    readonly property bool compact: width < 760
    readonly property bool narrow: width < 520
    readonly property bool singleColumn: width < 980
    readonly property int pageGutter: narrow ? 10 : (compact ? 16 : 28)
    readonly property int heroTitleSize: narrow ? 24 : (compact ? 30 : 42)
    readonly property int sectionTitleSize: narrow ? 24 : (compact ? 26 : 28)
    readonly property int sectionBodySize: narrow ? 15 : 16
    readonly property int listTextSize: narrow ? 16 : 18
    readonly property color glassPanel: "#d9101722"
    readonly property color glassPanelSoft: "#cc131c28"
    readonly property color glassBorder: "#1fd8e1f0"

    Rectangle {
        anchors.fill: parent
        color: "#05070b"
    }

    Image {
        anchors.fill: parent
        source: root.assetPath("media/images/image_wallpaper_tile.png")
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

            Label {
                text: i18n ? i18n.tf("grinffindor_page_eyebrow", "Gateway to Grin") : "Gateway to Grin"
                color: "#d9e1f1"
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }
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
            width: Math.min(scrollView.width - (narrow ? 12 : (compact ? 20 : 48)), 1120)
            x: Math.round((scrollView.width - width) / 2)
            y: compact ? 16 : 28
            spacing: compact ? 20 : 28

            // ── Hero ─────────────────────────────────────────────────────────
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
                    anchors.margins: narrow ? 16 : 22
                    spacing: 12

                    Label {
                        text: i18n ? i18n.tf("grinffindor_eyebrow", "Privacy · Infrastructure · Community") : "Privacy · Infrastructure · Community"
                        color: "#b8becf"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("grinffindor_page_title", "Welcome to Grinffindor") : "Welcome to Grinffindor"
                        font.pixelSize: root.heroTitleSize
                        font.bold: true
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("grinffindor_page_intro", "Grinffindor is a modern gateway into the world of Grin and Mimblewimble — built to make privacy-focused cryptocurrency more accessible, more practical, and more visual.")
                                   : "Grinffindor is a modern gateway into the world of Grin and Mimblewimble — built to make privacy-focused cryptocurrency more accessible, more practical, and more visual."
                        color: "#e8ebff"
                        font.pixelSize: narrow ? 16 : 18
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("grinffindor_page_sub", "From seed nodes and faucets to browser wallets and UmbrelOS integration — everything the Grin community needs in one place.")
                                   : "From seed nodes and faucets to browser wallets and UmbrelOS integration — everything the Grin community needs in one place."
                        color: "#d8ddff"
                        font.pixelSize: root.sectionBodySize
                        wrapMode: Text.WordWrap
                    }
                }
            }

            // ── Ecosystem CTA ─────────────────────────────────────────────────
            Rectangle {
                width: parent.width
                radius: 24
                color: "#101014b0"
                border.color: "#6b738066"
                border.width: 1
                implicitHeight: ctaColumn.implicitHeight + 30

                Column {
                    id: ctaColumn
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 10

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("grinffindor_cta_title", "Explore the Grinffindor Ecosystem") : "Explore the Grinffindor Ecosystem"
                        color: "#ffffff"
                        font.pixelSize: 22
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("grinffindor_cta_note", "Use the tiles on the main screen to navigate wallets, faucets, nodes, exchange and more.") : "Use the tiles on the main screen to navigate wallets, faucets, nodes, exchange and more."
                        color: "#c4cada"
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                    }
                }
            }

            // ── Feature grid ──────────────────────────────────────────────────
            GridLayout {
                width: parent.width
                columns: root.singleColumn ? 1 : 2
                columnSpacing: 16
                rowSpacing: 16

                Repeater {
                    model: [
                        {
                            title: i18n ? i18n.tf("grinffindor_feature_wallet_title", "Browser Wallet") : "Browser Wallet",
                            body:  i18n ? i18n.tf("grinffindor_feature_wallet_body", "An experimental Grin wallet that runs directly in your browser - no installation, no app store. It is still work in progress, so use it carefully and keep backups.") : "An experimental Grin wallet that runs directly in your browser - no installation, no app store. It is still work in progress, so use it carefully and keep backups."
                        },
                        {
                            title: i18n ? i18n.tf("grinffindor_feature_nodes_title", "Seed Nodes") : "Seed Nodes",
                            body:  i18n ? i18n.tf("grinffindor_feature_nodes_body", "Stable Mainnet and Testnet seed node infrastructure that keeps the Grin network connected and reachable — always available for wallet and node operators.") : "Stable Mainnet and Testnet seed node infrastructure that keeps the Grin network connected and reachable — always available for wallet and node operators."
                        },
                        {
                            title: i18n ? i18n.tf("grinffindor_feature_faucets_title", "Faucet & Tipping Bots") : "Faucet & Tipping Bots",
                            body:  i18n ? i18n.tf("grinffindor_feature_faucets_body", "Mainnet and Testnet bots for distributing Grin and onboarding new users. Experiment safely on Testnet or bring Grin into real interactions on Mainnet.") : "Mainnet and Testnet bots for distributing Grin and onboarding new users. Experiment safely on Testnet or bring Grin into real interactions on Mainnet."
                        },
                        {
                            title: i18n ? i18n.tf("grinffindor_feature_umbrel_title", "UmbrelOS Integration") : "UmbrelOS Integration",
                            body:  i18n ? i18n.tf("grinffindor_feature_umbrel_body", "Run your own Grin node at home through the Grinffindor Community App Store for UmbrelOS. One-click install, no complex setup, full self-sovereignty.") : "Run your own Grin node at home through the Grinffindor Community App Store for UmbrelOS. One-click install, no complex setup, full self-sovereignty."
                        }
                    ]

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: featureColumn.implicitHeight + 36
                        radius: 24
                        color: "#111318b0"
                        border.color: "#6b738066"
                        border.width: 1

                        Column {
                            id: featureColumn
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

            // ── What Grinffindor stands for ────────────────────────────────────
            Rectangle {
                width: parent.width
                radius: 26
                color: "#101014b0"
                border.color: "#6b738066"
                border.width: 1
                implicitHeight: valuesColumn.implicitHeight + 34

                Column {
                    id: valuesColumn
                    anchors.fill: parent
                    anchors.margins: narrow ? 16 : 22
                    spacing: 12

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("grinffindor_values_title", "What Grinffindor Stands For") : "What Grinffindor Stands For"
                        color: "#ffffff"
                        font.pixelSize: root.sectionTitleSize
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Repeater {
                        model: [
                            i18n ? i18n.tf("grinffindor_value_1", "Open infrastructure for Mainnet and Testnet") : "Open infrastructure for Mainnet and Testnet",
                            i18n ? i18n.tf("grinffindor_value_2", "Privacy-first tools built around Mimblewimble") : "Privacy-first tools built around Mimblewimble",
                            i18n ? i18n.tf("grinffindor_value_3", "Community-driven development and support") : "Community-driven development and support",
                            i18n ? i18n.tf("grinffindor_value_4", "Self-hosting made practical with UmbrelOS") : "Self-hosting made practical with UmbrelOS"
                        ]

                        Label {
                            width: valuesColumn.width
                            text: "\u2022  " + modelData
                            color: "#f3f5ff"
                            font.pixelSize: root.listTextSize
                            wrapMode: Text.WordWrap
                        }
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("grinffindor_values_note", "Everything is open source, self-hostable, and designed to keep you in control of your funds and privacy.") : "Everything is open source, self-hostable, and designed to keep you in control of your funds and privacy."
                        color: "#d7dbe6"
                        font.pixelSize: root.sectionBodySize
                        wrapMode: Text.WordWrap
                    }
                }
            }

            // ── Privacy by design ──────────────────────────────────────────────
            Rectangle {
                width: parent.width
                radius: 26
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#17191eb8" }
                    GradientStop { position: 1.0; color: "#0e1014b8" }
                }
                border.color: "#6b738066"
                border.width: 1
                implicitHeight: privacyColumn.implicitHeight + 34

                Column {
                    id: privacyColumn
                    anchors.fill: parent
                    anchors.margins: narrow ? 16 : 22
                    spacing: 12

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("grinffindor_privacy_title", "Privacy by Design. Yours.") : "Privacy by Design. Yours."
                        color: "#ffffff"
                        font.pixelSize: root.sectionTitleSize
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Repeater {
                        model: [
                            i18n ? i18n.tf("grinffindor_privacy_1", "No accounts, no tracking, no KYC") : "No accounts, no tracking, no KYC",
                            i18n ? i18n.tf("grinffindor_privacy_2", "Confidential transactions via Mimblewimble") : "Confidential transactions via Mimblewimble",
                            i18n ? i18n.tf("grinffindor_privacy_3", "Full ownership of your keys and funds") : "Full ownership of your keys and funds",
                            i18n ? i18n.tf("grinffindor_privacy_4", "Run everything locally on your own hardware") : "Run everything locally on your own hardware"
                        ]

                        Label {
                            width: privacyColumn.width
                            text: "\u2022  " + modelData
                            color: "#f3f5ff"
                            font.pixelSize: root.listTextSize
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }
    }
}
