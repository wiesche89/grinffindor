import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

BrowserWalletSectionCard {
    id: recoveryCard
    property var walletRoot

    width: parent ? parent.width : 0
    fillColor: walletRoot.nodeStatusMode() === "offline" ? "#34191d"
             : (walletRoot.pendingRecoveryCount() > 0 ? "#352816" : "#132635")
    strokeColor: walletRoot.recoveryBannerColor()
    title: walletRoot.tf("browser_wallet_recovery_title", "Operational Recovery")
    visible: walletRoot.recoveryBannerVisible()

    ColumnLayout {
        id: recoveryColumn
        width: parent.width
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: walletRoot.recoveryBannerText()
            color: walletRoot.recoveryBannerColor()
            font.pixelSize: walletRoot.bodyTextSize
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            visible: grinWalletController.lastError.length > 0 && walletRoot.nodeStatusMode() !== "online"
            text: grinWalletController.lastError
            color: "#ffd3d3"
            font.pixelSize: walletRoot.bodyTextSize
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true

            Button {
                text: walletRoot.tf("browser_wallet_refresh", "Refresh")
                font.pixelSize: walletRoot.controlTextSize
                onClicked: grinWalletController.refreshNodeStatus()
            }

            Button {
                text: walletRoot.tf("browser_wallet_rescan", "Full Rescan")
                font.pixelSize: walletRoot.controlTextSize
                enabled: walletRoot.nodeStatusMode() === "online"
                onClicked: grinWalletController.rescanWallet()
            }

            Button {
                text: walletRoot.tf("browser_wallet_nav_settings", "Settings")
                font.pixelSize: walletRoot.controlTextSize
                onClicked: walletRoot.activeSection = "settings"
            }

            Item { Layout.fillWidth: true }
        }
    }
}