import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

BrowserWalletSectionCard {
    id: recoveryCard
    property var walletRoot

    width: parent ? parent.width : 0
    fillColor: walletRoot.nodeStatusMode() === "offline" ? "#1e0c10"
             : (walletRoot.pendingRecoveryCount() > 0    ? "#1e1508" : "#0c1f2e")
    strokeColor: walletRoot.recoveryBannerColor()
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
            color: "#e08090"
            font.pixelSize: walletRoot.bodyTextSize
            wrapMode: Text.WordWrap
        }

        Flow {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: [
                    { label: walletRoot.tf("browser_wallet_refresh",  "Refresh"),     enabled: true,                                     action: function() { grinWalletController.refreshNodeStatus() } },
                    { label: walletRoot.tf("browser_wallet_rescan",   "Full Rescan"), enabled: walletRoot.nodeStatusMode() === "online",  action: function() { grinWalletController.rescanWallet() } },
                    { label: walletRoot.tf("browser_wallet_nav_settings", "Settings"), enabled: true,                                    action: function() { walletRoot.activeSection = "settings" } }
                ]

                Button {
                    text: modelData.label
                    font.pixelSize: walletRoot.controlTextSize
                    enabled: modelData.enabled
                    background: Rectangle {
                        radius: 10
                        color: parent.down ? "#07111c" : (parent.hovered ? "#060e18" : "transparent")
                        border.color: parent.hovered ? "#1e3a52" : "#152a3c"
                        border.width: 1
                    }
                    contentItem: Label {
                        text: parent.text; font: parent.font
                        color: !parent.enabled ? "#283c50" : (parent.hovered ? "#88b8d8" : "#5a8eac")
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: modelData.action()
                }
            }
        }
    }
}
