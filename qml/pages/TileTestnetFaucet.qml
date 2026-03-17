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
                        text: i18n ? i18n.tf("testnet_bot_page_eyebrow", "Telegram Bot") : "Telegram Bot"
                        color: "#b8becf"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("testnet_bot_page_title", "Grin Bot (Testnet)") : "Grin Bot (Testnet)"
                        font.pixelSize: scrollView.width < 700 ? 30 : 42
                        font.bold: true
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width * 0.9
                        text: i18n ? i18n.tf("testnet_bot_page_intro", "The Testnet Bot combines faucet access and tipping features in Telegram so users can learn, test, and experiment with Grin workflows safely.") : "The Testnet Bot combines faucet access and tipping features in Telegram so users can learn, test, and experiment with Grin workflows safely."
                        color: "#e8ebff"
                        font.pixelSize: 18
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width * 0.9
                        text: i18n ? i18n.tf("testnet_bot_page_store_intro", "It is the faster playground for onboarding, testing, and validating tipping and wallet flows before they move into broader use.") : "It is the faster playground for onboarding, testing, and validating tipping and wallet flows before they move into broader use."
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
                        text: i18n ? i18n.tf("testnet_bot_identity_title", "Find the Bot on Telegram") : "Find the Bot on Telegram"
                        color: "#ffffff"
                        font.pixelSize: 22
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("testnet_bot_identity_handle", "Handle: grin-test-bot") : "Handle: grin-test-bot"
                        color: "#e8ebff"
                        font.pixelSize: 17
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("testnet_bot_identity_name", "Display name: Grin Bot (Testnet)") : "Display name: Grin Bot (Testnet)"
                        color: "#c4cada"
                        font.pixelSize: 15
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        width: parent.width
                        text: "<a href=\"" + (i18n ? i18n.tf("testnet_bot_link_url", "https://t.me/grin_mw_test_bot") : "https://t.me/grin_mw_test_bot") + "\">"
                              + (i18n ? i18n.tf("testnet_bot_link_label", "Open Testnet Bot on Telegram") : "Open Testnet Bot on Telegram")
                              + "</a>"
                        color: "#dfe5ff"
                        font.pixelSize: 16
                        linkColor: "#dfe5ff"
                        wrapMode: Text.WordWrap
                        onLinkActivated: function(link) { Qt.openUrlExternally(link) }
                    }

                    Text {
                        width: parent.width
                        text: "<a href=\"" + (i18n ? i18n.tf("testnet_bot_group_url", "https://t.me/Grin_Tipping_Bot_Testnet") : "https://t.me/Grin_Tipping_Bot_Testnet") + "\">"
                              + (i18n ? i18n.tf("testnet_bot_group_label", "Open Testnet Tipping Group") : "Open Testnet Tipping Group")
                              + "</a>"
                        color: "#cfd6ff"
                        font.pixelSize: 15
                        linkColor: "#cfd6ff"
                        wrapMode: Text.WordWrap
                        onLinkActivated: function(link) { Qt.openUrlExternally(link) }
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
                            title: i18n ? i18n.tf("bot_core_title", "Core Commands") : "Core Commands",
                            body: i18n ? i18n.tf("testnet_bot_core_body", "Use start for the introduction, address for your Slatepack address, donate and donatepack for donations, faucet and faucetpack for faucet access, rewindhash and scanrewindhash for rewind tooling.") : "Use start for the introduction, address for your Slatepack address, donate and donatepack for donations, faucet and faucetpack for faucet access, rewindhash and scanrewindhash for rewind tooling."
                        },
                        {
                            title: i18n ? i18n.tf("bot_tipping_title", "Testnet Tipping Feature") : "Testnet Tipping Feature",
                            body: i18n ? i18n.tf("testnet_bot_tipping_body", "Last week the testnet bot was extended with tipping functionality. Deposit, withdraw, and tip are available and currently in the testing phase.") : "Last week the testnet bot was extended with tipping functionality. Deposit, withdraw, and tip are available and currently in the testing phase."
                        },
                        {
                            title: i18n ? i18n.tf("bot_technical_title", "Technical Details") : "Technical Details",
                            body: i18n ? i18n.tf("testnet_bot_technical_body", "The wallet runs in a separate account and path. Tips are executed off-chain between Telegram users. Deposits and withdrawals use Slatepacks.") : "The wallet runs in a separate account and path. Tips are executed off-chain between Telegram users. Deposits and withdrawals use Slatepacks."
                        },
                        {
                            title: i18n ? i18n.tf("bot_market_title", "Market Data") : "Market Data",
                            body: i18n ? i18n.tf("testnet_bot_market_body", "Use price, orderbook, chart, and history for market data, and use tipping, ledger, and balance to inspect bot-side wallet activity.") : "Use price, orderbook, chart, and history for market data, and use tipping, ledger, and balance to inspect bot-side wallet activity."
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
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("testnet_bot_commands", "start, address, donate, donatepack, faucet, faucetpack, rewindhash, scanrewindhash, price, orderbook, chart, history, deposit, withdraw, tip, tipping, ledger, balance") : "start, address, donate, donatepack, faucet, faucetpack, rewindhash, scanrewindhash, price, orderbook, chart, history, deposit, withdraw, tip, tipping, ledger, balance"
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
                        text: i18n ? i18n.tf("testnet_bot_journey_title", "Built for Testing and Tipping") : "Built for Testing and Tipping"
                        color: "#ffffff"
                        font.pixelSize: 28
                        font.bold: true
                    }

                    Repeater {
                        model: [
                            i18n ? i18n.tf("testnet_bot_journey_step_1", "Use the faucet to onboard and test quickly.") : "Use the faucet to onboard and test quickly.",
                            i18n ? i18n.tf("testnet_bot_journey_step_2", "Deposit and withdraw with Slatepack-based flows.") : "Deposit and withdraw with Slatepack-based flows.",
                            i18n ? i18n.tf("testnet_bot_journey_step_3", "Tip Telegram users off-chain during the testing phase.") : "Tip Telegram users off-chain during the testing phase.",
                            i18n ? i18n.tf("testnet_bot_journey_step_4", "Validate new Grin interactions before wider rollout.") : "Validate new Grin interactions before wider rollout."
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
                        text: i18n ? i18n.tf("testnet_bot_journey_footer", "The testnet bot is the practical sandbox for learning, trying new flows, and improving the Grin user experience.") : "The testnet bot is the practical sandbox for learning, trying new flows, and improving the Grin user experience."
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
