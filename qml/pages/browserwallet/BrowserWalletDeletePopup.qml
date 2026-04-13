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
    width: Math.min(walletRoot.width - 28, 520)
    padding: 0

    onClosed: {
        walletRoot.deleteConfirmOpen = false
        walletRoot.deletePasswordDraft = ""
    }

    background: Rectangle {
        radius: 22
        color: "#0d1c2a"
        border.color: "#6a2030"
        border.width: 1
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 22
        spacing: 14

        Label {
            Layout.fillWidth: true
            text: walletRoot.tf("browser_wallet_delete_title", "Delete Local Wallet Configuration?")
            color: "#ddeeff"
            font.pixelSize: 22
            font.weight: Font.Bold
            wrapMode: Text.WordWrap
        }

        // Warning banner
        Rectangle {
            Layout.fillWidth: true
            radius: 12
            color: "#1a0a10"
            border.color: "#7a2030"
            border.width: 1
            implicitHeight: warningLabel.implicitHeight + 20

            Label {
                id: warningLabel
                anchors.fill: parent
                anchors.margins: 12
                text: walletRoot.tf("browser_wallet_delete_note", "This removes the encrypted wallet, local history, and sync state from this browser. Make sure you still have the seed phrase before continuing.")
                color: "#c07080"
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }
        }

        TextField {
            Layout.fillWidth: true
            echoMode: TextInput.Password
            placeholderText: walletRoot.tf("browser_wallet_unlock_password", "Wallet password")
            text: walletRoot.deletePasswordDraft
            onTextChanged: walletRoot.deletePasswordDraft = text
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                text: walletRoot.tf("browser_wallet_cancel_delete", "Cancel")
                font.pixelSize: 13
                background: Rectangle {
                    radius: 10; color: parent.down ? "#07111c" : (parent.hovered ? "#060e18" : "transparent")
                    border.color: parent.hovered ? "#1e3a52" : "#152a3c"; border.width: 1
                }
                contentItem: Label {
                    text: parent.text; font: parent.font; color: parent.hovered ? "#88b8d8" : "#5a8eac"
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                onClicked: deleteWalletPopup.close()
            }

            Item { Layout.fillWidth: true }

            Button {
                text: walletRoot.tf("browser_wallet_delete_and_restore", "Delete and Restore")
                font.pixelSize: 13
                font.weight: Font.DemiBold
                enabled: walletRoot.deletePasswordDraft.length > 0
                background: Rectangle {
                    radius: 10; color: parent.down ? "#07111c" : (parent.hovered ? "#060e18" : "transparent")
                    border.color: parent.down ? "#254460" : (parent.hovered ? "#1e3a52" : "#152a3c")
                    border.width: 1
                }
                contentItem: Label {
                    text: parent.text; font: parent.font
                    color: parent.down ? "#a8d0e8" : (parent.hovered ? "#88b8d8" : "#4a7898")
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    walletRoot.beginRestoreReset()
                }
            }
        }
    }
}
