import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: topBar
    property var walletRoot

    height: 72
    color: "#101822"
    border.color: "#234b63"
    z: 2

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        spacing: 14

        Button {
            text: walletRoot.tf("browser_wallet_back_lock", "Back (Lock)")
            onClicked: {
                if (grinWalletController.walletUnlocked)
                    grinWalletController.lockWallet()
                walletRoot.backRequested()
            }
        }

        Label {
            text: walletRoot.tf("browser_wallet_title", "Grin Browser Wallet") + " / "
                  + walletRoot.tf("browser_wallet_self_custodial", "Self-Custodial")
            color: "#f7fbff"
            font.pixelSize: 28
            font.weight: Font.Bold
        }

        Item { Layout.fillWidth: true }

        Rectangle {
            radius: 14
            color: "#122231"
            border.color: walletRoot.nodeStatusMode() === "offline" ? "#8a3f3f"
                         : (walletRoot.nodeStatusMode() === "connecting" ? "#8a7440" : "#2f607a")
            implicitWidth: statusLabel.implicitWidth + 24
            implicitHeight: 34

            Label {
                id: statusLabel
                anchors.centerIn: parent
                text: grinWalletController.syncStatus
                color: "#d8f3ff"
                font.pixelSize: 13
            }
        }
    }
}