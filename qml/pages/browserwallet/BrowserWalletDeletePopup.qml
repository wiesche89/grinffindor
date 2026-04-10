import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: deleteWalletPopup
    property var walletRoot

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    visible: walletRoot.deleteConfirmOpen
    width: Math.min(walletRoot.width - 28, 560)
    padding: 0

    onClosed: walletRoot.deleteConfirmOpen = false

    background: Rectangle {
        radius: 28
        color: "#0f1b26"
        border.color: "#8a3f3f"
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        Label {
            Layout.fillWidth: true
            text: walletRoot.tf("browser_wallet_delete_title", "Delete Local Wallet Configuration?")
            color: "#ffffff"
            font.pixelSize: 28
            font.weight: Font.Bold
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            text: walletRoot.tf("browser_wallet_delete_note", "This removes the encrypted wallet, local history, and sync state from this browser. Make sure you still have the seed phrase before continuing.")
            color: "#ffd3d3"
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true

            Button {
                text: walletRoot.tf("browser_wallet_cancel_delete", "Cancel")
                onClicked: deleteWalletPopup.close()
            }

            Item { Layout.fillWidth: true }

            Button {
                text: walletRoot.tf("browser_wallet_delete_and_restore", "Delete and Restore")
                onClicked: {
                    deleteWalletPopup.close()
                    walletRoot.beginRestoreReset()
                }
            }
        }
    }
}