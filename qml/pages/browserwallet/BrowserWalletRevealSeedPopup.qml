import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../components" as AppComponents

Popup {
    id: revealSeedPopup
    property var walletRoot

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: Math.min(walletRoot.width - 28, 520)
    padding: 0

    onOpened: {
        if (revealSeedPasswordField.bridgeId !== undefined)
            PlatformBridge.requestFocus(revealSeedPasswordField.bridgeId)
    }

    background: Rectangle {
        radius: 28
        color: "#0f1b26"
        border.color: "#2a4f64"
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        Label {
            Layout.fillWidth: true
            text: walletRoot.tf("browser_wallet_seed_prompt_title", "Reveal Seed Phrase")
            color: "#ffffff"
            font.pixelSize: 28
            font.weight: Font.Bold
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            text: walletRoot.tf("browser_wallet_seed_prompt_note", "Enter your wallet password to decrypt and show the seed phrase for the active network wallet.")
            color: "#d7e9f4"
            wrapMode: Text.WordWrap
        }

        AppComponents.AppTextField {
            id: revealSeedPasswordField
            Layout.fillWidth: true
            editorTitle: walletRoot.tf("browser_wallet_seed_prompt_title", "Reveal Seed Phrase")
            echoMode: TextInput.Password
            text: walletRoot.revealSeedPasswordDraft
            placeholderText: walletRoot.tf("browser_wallet_password_placeholder", "Encryption password")
            onTextChanged: {
                walletRoot.revealSeedPasswordDraft = text
            }
            onAccepted: {
                if (grinWalletController.revealSeedPhrase(walletRoot.revealSeedPasswordDraft)) {
                    walletRoot.revealSeedPasswordDraft = ""
                    revealSeedPopup.close()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }

            Button {
                text: walletRoot.tf("browser_wallet_cancel", "Cancel")
                onClicked: {
                    walletRoot.revealSeedPasswordDraft = ""
                    revealSeedPopup.close()
                }
            }

            Button {
                text: walletRoot.tf("browser_wallet_seed_show", "Show Seed Phrase")
                enabled: walletRoot.revealSeedPasswordDraft.length > 0
                onClicked: {
                    if (grinWalletController.revealSeedPhrase(walletRoot.revealSeedPasswordDraft)) {
                        walletRoot.revealSeedPasswordDraft = ""
                        revealSeedPopup.close()
                    }
                }
            }
        }
    }
}