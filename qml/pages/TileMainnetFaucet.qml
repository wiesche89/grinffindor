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
                        text: i18n ? i18n.tf("mainnet_bot_page_eyebrow", "Telegram Bot") : "Telegram Bot"
                        color: "#b8becf"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("mainnet_bot_page_title", "GRIN Bot (Mainnet)") : "GRIN Bot (Mainnet)"
                        font.pixelSize: scrollView.width < 700 ? 30 : 42
                        font.bold: true
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width * 0.9
                        text: i18n ? i18n.tf("mainnet_bot_page_intro", "The Mainnet Bot brings Grin interactions directly into Telegram, giving users access to faucet, tipping, wallet, and market functions from a simple chat interface.") : "The Mainnet Bot brings Grin interactions directly into Telegram, giving users access to faucet, tipping, wallet, and market functions from a simple chat interface."
                        color: "#e8ebff"
                        font.pixelSize: 18
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width * 0.9
                        text: i18n ? i18n.tf("mainnet_bot_page_store_intro", "The bot is built for practical everyday use and gives the Grin community a lightweight interface for interacting with mainnet services.") : "The bot is built for practical everyday use and gives the Grin community a lightweight interface for interacting with mainnet services."
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
                implicitHeight: identityColumn.implicitHeight + 30

                Column {
                    id: identityColumn
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 10

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("mainnet_bot_identity_title", "Find the Bot on Telegram") : "Find the Bot on Telegram"
                        color: "#ffffff"
                        font.pixelSize: 22
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("mainnet_bot_identity_handle", "Handle: grin-bot") : "Handle: grin-bot"
                        color: "#e8ebff"
                        font.pixelSize: 17
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("mainnet_bot_identity_name", "Display name: GRIN Bot (Mainnet)") : "Display name: GRIN Bot (Mainnet)"
                        color: "#c4cada"
                        font.pixelSize: 15
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        width: parent.width
                        text: "<a href=\"" + (i18n ? i18n.tf("mainnet_bot_link_url", "https://t.me/grin_mw_bot") : "https://t.me/grin_mw_bot") + "\">"
                              + (i18n ? i18n.tf("mainnet_bot_link_label", "Open Mainnet Bot on Telegram") : "Open Mainnet Bot on Telegram")
                              + "</a>"
                        color: "#dfe5ff"
                        font.pixelSize: 16
                        linkColor: "#dfe5ff"
                        wrapMode: Text.WordWrap
                        onLinkActivated: function(link) { Qt.openUrlExternally(link) }
                    }
                }
            }

            GridLayout {
                width: parent.width
                columns: root.singleColumn ? 1 : 2
                columnSpacing: 16
                rowSpacing: 16

                Repeater {
                    model: [
                        {
                            title: i18n ? i18n.tf("bot_core_title", "Core Commands") : "Core Commands",
                            body: i18n ? i18n.tf("mainnet_bot_core_body", "Use start for the introduction, address for your Slatepack address, donate and donatepack for donations, faucet and faucetpack for faucet access, rewindhash and scanrewindhash for rewind tooling.") : "Use start for the introduction, address for your Slatepack address, donate and donatepack for donations, faucet and faucetpack for faucet access, rewindhash and scanrewindhash for rewind tooling."
                        },
                        {
                            title: i18n ? i18n.tf("bot_market_title", "Market Data") : "Market Data",
                            body: i18n ? i18n.tf("mainnet_bot_market_body", "Use price for the USDT price, orderbook for the current order book, chart for the 4h USDT chart, and history for the last 10 USDT trades.") : "Use price for the USDT price, orderbook for the current order book, chart for the 4h USDT chart, and history for the last 10 USDT trades."
                        },
                        {
                            title: i18n ? i18n.tf("bot_wallet_title", "Wallet Actions") : "Wallet Actions",
                            body: i18n ? i18n.tf("mainnet_bot_wallet_body", "Use deposit and withdraw with an amount, tip another Telegram user, view tipping info, inspect your ledger, and check your balance directly in chat.") : "Use deposit and withdraw with an amount, tip another Telegram user, view tipping info, inspect your ledger, and check your balance directly in chat."
                        },
                        {
                            title: i18n ? i18n.tf("bot_usage_title", "Practical Access") : "Practical Access",
                            body: i18n ? i18n.tf("mainnet_bot_usage_body", "The bot is designed to make mainnet Grin easier to access in everyday use, combining faucet, interaction, and lightweight wallet workflows in one place.") : "The bot is designed to make mainnet Grin easier to access in everyday use, combining faucet, interaction, and lightweight wallet workflows in one place."
                        }
                    ]

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: faucetFeatureColumn.implicitHeight + 36
                        radius: 24
                        color: "#111318b0"
                        border.color: "#6b738066"
                        border.width: 1

                        Column {
                            id: faucetFeatureColumn
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
                implicitHeight: commandsColumn.implicitHeight + 34

                Column {
                    id: commandsColumn
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 12

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("bot_commands_title", "Available Commands") : "Available Commands"
                        color: "#ffffff"
                        font.pixelSize: 28
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("mainnet_bot_commands", "start, address, donate, donatepack, faucet, faucetpack, rewindhash, scanrewindhash, price, orderbook, chart, history, deposit, withdraw, tip, tipping, ledger, balance") : "start, address, donate, donatepack, faucet, faucetpack, rewindhash, scanrewindhash, price, orderbook, chart, history, deposit, withdraw, tip, tipping, ledger, balance"
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
                        text: i18n ? i18n.tf("mainnet_bot_journey_title", "Mainnet in Chat") : "Mainnet in Chat"
                        color: "#ffffff"
                        font.pixelSize: 28
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Repeater {
                        model: [
                            i18n ? i18n.tf("mainnet_bot_journey_step_1", "Check the faucet and donation flows.") : "Check the faucet and donation flows.",
                            i18n ? i18n.tf("mainnet_bot_journey_step_2", "Use market tools directly inside Telegram.") : "Use market tools directly inside Telegram.",
                            i18n ? i18n.tf("mainnet_bot_journey_step_3", "Tip other users and track your balance.") : "Tip other users and track your balance.",
                            i18n ? i18n.tf("mainnet_bot_journey_step_4", "Keep Grin interactions lightweight and accessible.") : "Keep Grin interactions lightweight and accessible."
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
                        text: i18n ? i18n.tf("mainnet_bot_journey_footer", "Open Telegram, start the bot, and bring Grin into everyday conversations.") : "Open Telegram, start the bot, and bring Grin into everyday conversations."
                        color: "#d7dbe6"
                        font.pixelSize: 18
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
