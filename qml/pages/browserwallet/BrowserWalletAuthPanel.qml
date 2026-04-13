import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../components" as AppComponents

Item {
    id: authPanel
    property var walletRoot
    property var walletPageSettings
    readonly property bool phoneMode: walletRoot && walletRoot.phoneMode
    readonly property bool veryPhoneMode: walletRoot && walletRoot.veryPhoneMode

    onVisibleChanged: {
        if (!visible && walletRoot)
            walletRoot.unlockPasswordDraft = ""
    }

    // Shared button style helpers (inline, no helper component dependency)
    function btnBg(pressed, hovered) {
        if (pressed)  return "#07111c"
        if (hovered)  return "#060e18"
        return "transparent"
    }
    function btnBorder(pressed, hovered) {
        if (pressed)  return "#254460"
        if (hovered)  return "#1e3a52"
        return "#152a3c"
    }
    function btnText(pressed, hovered) {
        if (pressed)  return "#a8d0e8"
        if (hovered)  return "#88b8d8"
        return "#5a8eac"
    }

    Rectangle {
        anchors.fill: parent
        color: "#05090fdd"

        MouseArea { anchors.fill: parent }

        Flickable {
            anchors.fill: parent
            contentWidth: width
            contentHeight: authCard.height + 60
            clip: true
            ScrollBar.vertical: ScrollBar {}

            Rectangle {
                id: authCard
                width: Math.min(parent.width - (veryPhoneMode ? 16 : (phoneMode ? 24 : 40)), 600)
                anchors.horizontalCenter: parent.horizontalCenter
                y: Math.max(20, (parent.height - height) / 2)
                radius: veryPhoneMode ? 18 : (phoneMode ? 20 : 24)
                color: "#0d1c2a"
                border.color: "#1e3d55"
                border.width: 1
                implicitHeight: authColumn.implicitHeight + (veryPhoneMode ? 24 : (phoneMode ? 30 : 36))

                ColumnLayout {
                    id: authColumn
                    anchors.fill: parent
                    anchors.margins: veryPhoneMode ? 16 : (phoneMode ? 18 : 24)
                    spacing: veryPhoneMode ? 10 : (phoneMode ? 12 : 14)

                    // Title
                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.authMode === "unlock"
                              ? walletRoot.tf("browser_wallet_login_title", "Unlock Your Wallet")
                              : walletRoot.tf("browser_wallet_setup_title", "Set Up Your Wallet")
                        color: "#ddeeff"
                        font.pixelSize: veryPhoneMode ? 22 : (phoneMode ? 26 : 30)
                        font.weight: Font.Bold
                        wrapMode: Text.WordWrap
                    }

                    // Subtitle
                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.authMode === "unlock"
                              ? walletRoot.tf("browser_wallet_login_note", "Enter your password to unlock the locally stored wallet for this browser session.")
                              : walletRoot.tf("browser_wallet_setup_note", "Create a new wallet with a fresh seed phrase or restore an existing wallet from its 24 words.")
                        color: "#4a7a94"
                        font.pixelSize: veryPhoneMode ? 12 : 13
                        wrapMode: Text.WordWrap
                    }

                    // Network selector
                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: true
                        spacing: 6

                        Label {
                            text: walletRoot.tf("browser_wallet_auth_network", "Active wallet network")
                            color: "#3a6a84"
                            font.pixelSize: 11
                            font.letterSpacing: 0.8
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Repeater {
                                model: ["mainnet", "testnet"]

                                Button {
                                    Layout.fillWidth: true
                                    text: modelData === "mainnet"
                                          ? walletRoot.tf("browser_wallet_network_mainnet", "Mainnet")
                                          : walletRoot.tf("browser_wallet_network_testnet", "Testnet")
                                    font.pixelSize: walletRoot.controlTextSize
                                    highlighted: walletRoot.authNetworkDraft === modelData
                                    background: Rectangle {
                                        radius: 10
                                        color: parent.highlighted ? "#0c2035"
                                             : (parent.down ? "#07111c" : (parent.hovered ? "#060e18" : "transparent"))
                                        border.color: parent.highlighted ? "#386888"
                                                    : (parent.hovered ? "#1e3a52" : "#152a3c")
                                        border.width: 1
                                    }
                                    contentItem: Label {
                                        text: parent.text
                                        font: parent.font
                                        color: parent.highlighted ? "#90c8e8"
                                             : (parent.hovered ? "#88b8d8" : "#4a7898")
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    onClicked: {
                                        walletRoot.authNetworkDraft = modelData
                                        grinWalletController.setSelectedNetwork(modelData)
                                    }
                                }
                            }
                        }
                    }

                    // Mode selector (new wallet / restore / import)
                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: !grinWalletController.walletExists
                        spacing: 6

                        Label {
                            text: walletRoot.tf("browser_wallet_setup_mode", "Setup mode")
                            color: "#3a6a84"
                            font.pixelSize: 11
                            font.letterSpacing: 0.8
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: 8

                            Repeater {
                                model: [
                                    { key: "create",        label: walletRoot.tf("browser_wallet_create_mode", "New Wallet") },
                                    { key: "restore",       label: walletRoot.tf("browser_wallet_restore_mode", "Restore Seed") }
                                ]

                                Button {
                                    text: modelData.label
                                    font.pixelSize: walletRoot.controlTextSize
                                    highlighted: walletRoot.authMode === modelData.key
                                    background: Rectangle {
                                        radius: 10
                                        color: parent.highlighted ? "#0c2035"
                                             : (parent.down ? "#07111c" : (parent.hovered ? "#060e18" : "transparent"))
                                        border.color: parent.highlighted ? "#386888"
                                                    : (parent.hovered ? "#1e3a52" : "#152a3c")
                                        border.width: 1
                                    }
                                    contentItem: Label {
                                        text: parent.text; font: parent.font
                                        color: parent.highlighted ? "#90c8e8"
                                             : (parent.hovered ? "#88b8d8" : "#4a7898")
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    onClicked: {
                                        walletRoot.authMode = modelData.key
                                        if (modelData.key !== "restore")  walletRoot.restoreMnemonicDraft = ""
                                    }
                                }
                            }
                        }
                    }

                    // Wallet name
                    AppComponents.AppTextField {
                        id: walletNameField
                        Layout.fillWidth: true
                        visible: walletRoot.authMode !== "unlock"
                        font.pixelSize: walletRoot.controlTextSize
                        text: walletPageSettings.walletNameDraft
                        placeholderText: walletRoot.tf("browser_wallet_name_placeholder", "Wallet name")
                        onTextChanged: walletPageSettings.walletNameDraft = text
                    }

                    // Restore mnemonic
                    AppComponents.AppTextArea {
                        id: restoreMnemonicArea
                        Layout.fillWidth: true
                        Layout.preferredHeight: walletRoot.authMode === "restore"
                                                ? (veryPhoneMode ? 100 : (phoneMode ? 112 : 128)) : 0
                        visible: walletRoot.authMode === "restore"
                        editorTitle: walletRoot.tf("browser_wallet_paste_restore_title", "Paste Seed Phrase")
                        font.pixelSize: walletRoot.controlTextSize
                        wrapMode: TextEdit.Wrap
                        selectByMouse: true
                        persistentSelection: true
                        activeFocusOnPress: true
                        text: walletRoot.restoreMnemonicDraft
                        placeholderText: walletRoot.tf("browser_wallet_mnemonic_placeholder", "24-word mnemonic for importing an existing wallet.")
                        onTextChanged: walletRoot.restoreMnemonicDraft = text
                    }

                    // Paste button for restore
                    RowLayout {
                        Layout.fillWidth: true
                        visible: walletRoot.authMode === "restore"
                        Item { Layout.fillWidth: true }

                        Button {
                            text: walletRoot.tf("browser_wallet_paste", "Paste")
                            font.pixelSize: walletRoot.controlTextSize
                            background: Rectangle {
                                radius: 10; color: parent.down ? "#07111c" : (parent.hovered ? "#060e18" : "transparent")
                                border.color: parent.hovered ? "#1e3a52" : "#152a3c"; border.width: 1
                            }
                            contentItem: Label {
                                text: parent.text; font: parent.font; color: parent.hovered ? "#88b8d8" : "#5a8eac"
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                walletRoot.openPasteDialog(
                                    restoreMnemonicArea,
                                    walletRoot.tf("browser_wallet_paste_restore_title", "Paste Seed Phrase"),
                                    walletRoot.tf("browser_wallet_mnemonic_placeholder", "24-word mnemonic for importing an existing wallet."))
                            }
                        }
                    }

                    // Unlock password
                    AppComponents.AppTextField {
                        id: unlockPasswordField
                        Layout.fillWidth: true
                        visible: walletRoot.authMode === "unlock"
                        editorTitle: walletRoot.tf("browser_wallet_login_title", "Unlock Your Wallet")
                        font.pixelSize: walletRoot.controlTextSize
                        echoMode: TextInput.Password
                        text: walletRoot.unlockPasswordDraft
                        placeholderText: walletRoot.tf("browser_wallet_password_placeholder", "Encryption password")
                        onTextChanged: walletRoot.unlockPasswordDraft = text
                        onAccepted: walletRoot.submitAuth()
                    }

                    // Create password
                    AppComponents.AppTextField {
                        id: createPasswordField
                        Layout.fillWidth: true
                        visible: walletRoot.authMode !== "unlock"
                        editorTitle: walletRoot.tf("browser_wallet_setup_title", "Set Up Your Wallet")
                        font.pixelSize: walletRoot.controlTextSize
                        echoMode: TextInput.Password
                        text: walletRoot.passwordDraft
                        placeholderText: walletRoot.tf("browser_wallet_password_placeholder", "Encryption password")
                        onTextChanged: walletRoot.passwordDraft = text
                    }

                    // Confirm password
                    AppComponents.AppTextField {
                        id: confirmPasswordField
                        Layout.fillWidth: true
                        visible: walletRoot.authMode !== "unlock"
                        editorTitle: walletRoot.tf("browser_wallet_confirm_password_placeholder", "Confirm password")
                        font.pixelSize: walletRoot.controlTextSize
                        echoMode: TextInput.Password
                        text: walletRoot.passwordConfirmDraft
                        placeholderText: walletRoot.tf("browser_wallet_confirm_password_placeholder", "Confirm password")
                        onTextChanged: walletRoot.passwordConfirmDraft = text
                        onAccepted: walletRoot.submitAuth()
                    }

                    // Password match indicator
                    Label {
                        Layout.fillWidth: true
                        visible: walletRoot.authMode !== "unlock"
                                 && walletRoot.passwordDraft.length > 0
                        text: walletRoot.passwordDraft === walletRoot.passwordConfirmDraft
                              ? walletRoot.tf("browser_wallet_password_match", "Passwords match.")
                              : walletRoot.tf("browser_wallet_password_no_match", "Passwords do not match yet.")
                        color: walletRoot.passwordDraft === walletRoot.passwordConfirmDraft ? "#FEF102" : "#dd4050"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }

                    // Actions row
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        // Delete wallet (secondary destructive)
                        Button {
                            visible: grinWalletController.walletExists
                            text: walletRoot.tf("browser_wallet_delete", "Delete Wallet")
                            font.pixelSize: walletRoot.controlTextSize
                            enabled: true
                            background: Rectangle {
                                radius: 10
                                color: parent.down ? "#07111c" : (parent.hovered ? "#060e18" : "transparent")
                                border.color: parent.down ? "#254460"
                                            : (parent.hovered ? "#1e3a52" : "#152a3c")
                                border.width: 1
                            }
                            contentItem: Label {
                                text: parent.text; font: parent.font
                                color: parent.down ? "#a8d0e8"
                                     : (parent.hovered ? "#88b8d8" : "#4a7898")
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: walletRoot.deleteConfirmOpen = true
                        }

                        Item { Layout.fillWidth: true }

                        // Primary action (Unlock / Create / Restore)
                        Button {
                            text: walletRoot.authMode === "unlock"
                                  ? walletRoot.tf("browser_wallet_unlock", "Unlock")
                                  : (walletRoot.authMode === "create"
                                     ? walletRoot.tf("browser_wallet_create", "Create")
                                     : walletRoot.tf("browser_wallet_restore", "Restore"))
                            font.pixelSize: walletRoot.controlTextSize
                            font.weight: Font.DemiBold
                            enabled: walletRoot.authMode === "unlock"
                                     ? unlockPasswordField.text.length > 0
                                     : (walletNameField.text.trim().length > 0
                                        && createPasswordField.text.length > 0
                                        && createPasswordField.text === confirmPasswordField.text
                                        && (walletRoot.authMode !== "restore" || restoreMnemonicArea.text.trim().length > 0))
                            background: Rectangle {
                                radius: 10
                                color: !parent.enabled ? "transparent"
                                     : (parent.down ? "#07111c" : (parent.hovered ? "#060e18" : "transparent"))
                                border.color: !parent.enabled ? "#152a3c"
                                            : (parent.down ? "#254460" : (parent.hovered ? "#1e3a52" : "#152a3c"))
                                border.width: 1
                            }
                            contentItem: Label {
                                text: parent.text; font: parent.font
                                color: !parent.enabled ? "#355468"
                                     : (parent.down ? "#a8d0e8" : (parent.hovered ? "#88b8d8" : "#4a7898"))
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                walletPageSettings.walletNameDraft = walletNameField.text
                                walletRoot.unlockPasswordDraft = unlockPasswordField.text
                                walletRoot.passwordDraft = createPasswordField.text
                                walletRoot.passwordConfirmDraft = confirmPasswordField.text
                                walletRoot.restoreMnemonicDraft = restoreMnemonicArea.text
                                walletRoot.submitAuth()
                            }
                        }
                    }
                }
            }
        }
    }
}
