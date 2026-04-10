import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: errorPopup
    property var walletRoot

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: Math.min(walletRoot.width - 28, 620)
    padding: 0

    onClosed: grinWalletController.clearLastError()

    background: Rectangle {
        radius: 28
        color: "#221216"
        border.color: "#c65b5b"
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        Label {
            Layout.fillWidth: true
            text: walletRoot.tf("browser_wallet_error_title", "Wallet Error")
            color: "#ffffff"
            font.pixelSize: 28
            font.weight: Font.Bold
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            text: walletRoot.errorDialogText
            color: "#ffd7d7"
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }

            Button {
                text: walletRoot.tf("browser_wallet_error_close", "Close")
                onClicked: errorPopup.close()
            }
        }
    }
}