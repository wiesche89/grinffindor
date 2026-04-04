import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: recoveryCard
    property var walletRoot

    width: parent ? parent.width : 0
    radius: 24
    color: walletRoot.nodeStatusMode() === "offline" ? "#34191d"
         : (walletRoot.pendingRecoveryCount() > 0 ? "#352816" : "#132635")
    border.color: walletRoot.recoveryBannerColor()
    visible: walletRoot.recoveryBannerVisible()
    implicitHeight: recoveryColumn.implicitHeight + 30

    ColumnLayout {
        id: recoveryColumn
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: walletRoot.tf("browser_wallet_recovery_title", "Operational Recovery")
            color: "#ffffff"
            font.pixelSize: 24
            font.weight: Font.Bold
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            text: walletRoot.recoveryBannerText()
            color: walletRoot.recoveryBannerColor()
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            visible: grinWalletController.lastError.length > 0 && walletRoot.nodeStatusMode() !== "online"
            text: grinWalletController.lastError
            color: "#ffd3d3"
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true

            Button {
                text: walletRoot.tf("browser_wallet_refresh", "Refresh")
                onClicked: grinWalletController.refreshNodeStatus()
            }

            Button {
                text: walletRoot.tf("browser_wallet_rescan", "Full Rescan")
                enabled: walletRoot.nodeStatusMode() === "online"
                onClicked: grinWalletController.rescanWallet()
            }

            Button {
                text: walletRoot.tf("browser_wallet_nav_settings", "Settings")
                onClicked: walletRoot.activeSection = "settings"
            }

            Item { Layout.fillWidth: true }
        }
    }
}