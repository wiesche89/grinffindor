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
    width: Math.min(walletRoot.width - 28, 480)
    padding: 0

    onOpened: {
        if (revealSeedPasswordField.bridgeId !== undefined)
            PlatformBridge.requestFocus(revealSeedPasswordField.bridgeId)
    }

    background: Rectangle {
        radius: 22
        color: "#0d1c2a"
        border.color: "#1e3d55"
        border.width: 1
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 22
        spacing: 14

        Label {
            Layout.fillWidth: true
            text: walletRoot.tf("browser_wallet_seed_prompt_title", "Reveal Seed Phrase")
            color: "#ddeeff"
            font.pixelSize: 22
            font.weight: Font.Bold
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            text: walletRoot.tf("browser_wallet_seed_prompt_note", "Enter your wallet password to decrypt and show the seed phrase for the active network wallet.")
            color: "#3a6a84"
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }

        // Security note
        Rectangle {
            Layout.fillWidth: true
            radius: 10
            color: "#0c2030"
            border.color: "#806800"
            border.width: 1
            implicitHeight: secLabel.implicitHeight + 16

            Label {
                id: secLabel
                anchors.fill: parent
                anchors.margins: 10
                text: walletRoot.tf("browser_wallet_seed_prompt_warning",
                      "Keep this phrase private. Never share it digitally or enter it on untrusted devices.")
                color: "#FEF102"
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }

        AppComponents.AppTextField {
            id: revealSeedPasswordField
            Layout.fillWidth: true
            editorTitle: walletRoot.tf("browser_wallet_seed_prompt_title", "Reveal Seed Phrase")
            echoMode: TextInput.Password
            text: walletRoot.revealSeedPasswordDraft
            placeholderText: walletRoot.tf("browser_wallet_password_placeholder", "Encryption password")
            onTextChanged: walletRoot.revealSeedPasswordDraft = text
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
                font.pixelSize: 13
                background: Rectangle {
                    radius: 10; color: parent.down ? "#07111c" : (parent.hovered ? "#060e18" : "transparent")
                    border.color: parent.hovered ? "#1e3a52" : "#152a3c"; border.width: 1
                }
                contentItem: Label {
                    text: parent.text; font: parent.font; color: parent.hovered ? "#88b8d8" : "#5a8eac"
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    walletRoot.revealSeedPasswordDraft = ""
                    revealSeedPopup.close()
                }
            }

            Button {
                text: walletRoot.tf("browser_wallet_seed_show", "Show Seed Phrase")
                font.pixelSize: 13
                font.weight: Font.DemiBold
                enabled: walletRoot.revealSeedPasswordDraft.length > 0
                background: Rectangle {
                    radius: 10
                    color: !parent.enabled ? "transparent" : (parent.down ? "#07111c" : (parent.hovered ? "#060e18" : "transparent"))
                    border.color: !parent.enabled ? "#152a3c" : (parent.down ? "#254460" : (parent.hovered ? "#1e3a52" : "#152a3c"))
                    border.width: 1
                }
                contentItem: Label {
                    text: parent.text; font: parent.font
                    color: !parent.enabled ? "#355468" : (parent.down ? "#a8d0e8" : (parent.hovered ? "#88b8d8" : "#4a7898"))
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
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
