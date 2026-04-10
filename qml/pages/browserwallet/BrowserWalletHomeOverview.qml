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

    width: parent ? parent.width : 0
    title: walletRoot.tf("browser_wallet_overview_title", "Overview")
    fillColor: "#102131"
    strokeColor: "#29516a"

    GridLayout {
        Layout.fillWidth: true
        width: parent.width
        columns: width < 760 ? 1 : 3
        rowSpacing: 12
        columnSpacing: 12

        BrowserWalletMetricTile {
            title: walletRoot.tf("browser_wallet_metric_network", "Network")
            value: grinWalletController.selectedNetwork
        }

        BrowserWalletMetricTile {
            title: walletRoot.tf("browser_wallet_metric_chain", "Chain Height")
            value: "" + grinWalletController.chainHeight
        }

        BrowserWalletMetricTile {
            title: walletRoot.tf("browser_wallet_metric_scan", "Scan Height")
            value: "" + grinWalletController.scanHeight
        }

        BrowserWalletMetricTile {
            title: walletRoot.tf("browser_wallet_metric_balance", "Spendable")
            value: grinWalletController.spendableBalance + " GRIN"
        }

        BrowserWalletMetricTile {
            title: walletRoot.tf("browser_wallet_awaiting", "Awaiting")
            value: overviewCard.formatAmountValue(
                       overviewCard.amountStringToValue(grinWalletController.awaitingConfirmationBalance)
                       + overviewCard.amountStringToValue(grinWalletController.awaitingFinalizationBalance)) + " GRIN"
        }

        BrowserWalletMetricTile {
            title: walletRoot.tf("browser_wallet_locked", "Locked")
            value: grinWalletController.lockedBalance + " GRIN"
        }
    }
}