import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

BrowserWalletSectionCard {
    id: overviewCard
    property var walletRoot

    function amountStringToValue(amountText) {
        var parsed = Number(amountText ? amountText.toString().trim() : "")
        return isFinite(parsed) ? parsed : 0
    }

    function formatAmountValue(value) {
        var fixed = Math.max(0, value).toFixed(9)
        return fixed.replace(/0+$/, "").replace(/\.$/, "")
    }

    function syncPercent() {
        var chain = grinWalletController.chainHeight
        var scan  = grinWalletController.scanHeight
        if (chain <= 0) return -1
        return Math.min(100, Math.round(scan * 100 / chain))
    }

    width: parent ? parent.width : 0
    title: ""
    fillColor: "#0a1825"
    strokeColor: "#152e42"

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 0

        // ── Balance hero ──────────────────────────────────────────────────
        Item {
            Layout.fillWidth: true
            implicitHeight: balanceCol.implicitHeight + 8

            Column {
                id: balanceCol
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 4

                Label {
                    text: walletRoot.tf("browser_wallet_metric_balance", "Spendable")
                    color: "#2a5a72"
                    font.pixelSize: 11
                    font.letterSpacing: 1.2
                }

                Label {
                    width: parent.width
                    text: grinWalletController.spendableBalance + " GRIN"
                    color: "#c0e8ff"
                    font.pixelSize: walletRoot.veryPhoneMode ? 26 : (walletRoot.phoneMode ? 30 : (walletRoot.compactNavigation ? 34 : 40))
                    font.weight: Font.Bold
                    wrapMode: Text.WordWrap
                }
            }
        }

        // ── Sub-balances row ──────────────────────────────────────────────
        Item { Layout.fillWidth: true; implicitHeight: 10 }

        GridLayout {
            Layout.fillWidth: true
            columns: width < 480 ? 2 : 3
            rowSpacing: 10
            columnSpacing: 10

            Repeater {
                model: [
                    {
                        label: walletRoot.tf("browser_wallet_awaiting", "Awaiting"),
                        value: overviewCard.formatAmountValue(
                            overviewCard.amountStringToValue(grinWalletController.awaitingConfirmationBalance)
                            + overviewCard.amountStringToValue(grinWalletController.awaitingFinalizationBalance)) + " GRIN"
                    },
                    {
                        label: walletRoot.tf("browser_wallet_locked", "Locked"),
                        value: grinWalletController.lockedBalance + " GRIN"
                    }
                ]

                Column {
                    spacing: 2

                    Label {
                        text: modelData.label
                        color: "#2a5a72"
                        font.pixelSize: 11
                        font.letterSpacing: 0.5
                    }
                    Label {
                        text: modelData.value
                        color: "#7aacca"
                        font.pixelSize: walletRoot.veryPhoneMode ? 13 : 14
                        font.weight: Font.DemiBold
                    }
                }
            }
        }

        // ── Divider ───────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#0e2535"
            Layout.topMargin: 14
            Layout.bottomMargin: 14
        }

        // ── Sync progress ─────────────────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            // Progress bar
            // Block heights
            Flow {
                Layout.fillWidth: true
                spacing: 16

                Repeater {
                    model: [
                        { label: walletRoot.tf("browser_wallet_metric_network", "Network"), value: grinWalletController.selectedNetwork },
                        { label: walletRoot.tf("browser_wallet_metric_chain", "Chain"), value: "" + grinWalletController.chainHeight },
                        { label: walletRoot.tf("browser_wallet_metric_scan", "Scan"), value: "" + grinWalletController.scanHeight }
                    ]

                    Row {
                        spacing: 5

                        Label {
                            text: modelData.label + ":"
                            color: "#2a5a72"
                            font.pixelSize: 11
                        }
                        Label {
                            text: modelData.value
                            color: "#5a9ab8"
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                        }
                    }
                }
            }
        }
    }
}
