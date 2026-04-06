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

    Rectangle {
        anchors.fill: parent
        color: "#05080dcc"

        MouseArea { anchors.fill: parent }

        Rectangle {
            width: Math.min(parent.width - (veryPhoneMode ? 12 : (phoneMode ? 18 : 28)), 640)
            anchors.centerIn: parent
            radius: veryPhoneMode ? 18 : (phoneMode ? 22 : 30)
            color: "#0f1b26"
            border.color: "#2a4f64"
            implicitHeight: authColumn.implicitHeight + (veryPhoneMode ? 22 : (phoneMode ? 28 : 38))

            ColumnLayout {
                id: authColumn
                anchors.fill: parent
                anchors.margins: veryPhoneMode ? 12 : (phoneMode ? 14 : 20)
                spacing: veryPhoneMode ? 8 : (phoneMode ? 10 : 14)

                Label {
                    Layout.fillWidth: true
                    text: walletRoot.authMode === "unlock"
                          ? walletRoot.tf("browser_wallet_login_title", "Unlock Your Wallet")
                          : walletRoot.tf("browser_wallet_setup_title", "Set Up Your Wallet")
                    color: "#ffffff"
                    font.pixelSize: veryPhoneMode ? 20 : (phoneMode ? 24 : 30)
                    font.weight: Font.Bold
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    text: walletRoot.authMode === "unlock"
                          ? walletRoot.tf("browser_wallet_login_note", "Enter your password to unlock the locally stored wallet for this browser session.")
                          : (walletRoot.authMode === "import_backup"
                             ? walletRoot.tf("browser_wallet_import_backup_note", "Paste a previously exported encrypted wallet backup JSON. The wallet stays locked after import until you unlock it with its password.")
                             : walletRoot.tf("browser_wallet_setup_note", "Create a new wallet with a fresh seed phrase or restore an existing wallet from its 24 words."))
                    color: "#d7e9f4"
                    font.pixelSize: veryPhoneMode ? 12 : (phoneMode ? 13 : 14)
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    visible: walletRoot.authMode !== "import_backup"
                    text: walletRoot.tf("browser_wallet_auth_network", "Active wallet network") + ": " + walletRoot.authNetworkDraft
                    color: "#8ff0c8"
                    font.pixelSize: veryPhoneMode ? 12 : (phoneMode ? 13 : 14)
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: walletRoot.authMode !== "import_backup"
                    spacing: 8
                    layoutDirection: phoneMode ? Qt.Vertical : Qt.LeftToRight

                    Button {
                        text: walletRoot.tf("browser_wallet_network_mainnet", "Mainnet")
                        highlighted: walletRoot.authNetworkDraft === "mainnet"
                        Layout.fillWidth: true
                        font.pixelSize: walletRoot.controlTextSize
                        onClicked: {
                            walletRoot.authNetworkDraft = "mainnet"
                            grinWalletController.setSelectedNetwork("mainnet")
                        }
                    }

                    Button {
                        text: walletRoot.tf("browser_wallet_network_testnet", "Testnet")
                        highlighted: walletRoot.authNetworkDraft === "testnet"
                        Layout.fillWidth: true
                        font.pixelSize: walletRoot.controlTextSize
                        onClicked: {
                            walletRoot.authNetworkDraft = "testnet"
                            grinWalletController.setSelectedNetwork("testnet")
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: !grinWalletController.walletExists
                    spacing: 8
                    layoutDirection: phoneMode ? Qt.Vertical : Qt.LeftToRight

                    Button {
                        text: walletRoot.tf("browser_wallet_create_mode", "New Wallet")
                        highlighted: walletRoot.authMode === "create"
                        Layout.fillWidth: true
                        font.pixelSize: walletRoot.controlTextSize
                        onClicked: {
                            walletRoot.authMode = "create"
                            walletRoot.restoreMnemonicDraft = ""
                            walletRoot.backupImportDraft = ""
                        }
                    }

                    Button {
                        text: walletRoot.tf("browser_wallet_restore_mode", "Restore Seed")
                        highlighted: walletRoot.authMode === "restore"
                        Layout.fillWidth: true
                        font.pixelSize: walletRoot.controlTextSize
                        onClicked: {
                            walletRoot.authMode = "restore"
                            walletRoot.backupImportDraft = ""
                        }
                    }

                    Button {
                        text: walletRoot.tf("browser_wallet_import_backup", "Import Backup")
                        highlighted: walletRoot.authMode === "import_backup"
                        Layout.fillWidth: true
                        font.pixelSize: walletRoot.controlTextSize
                        onClicked: {
                            walletRoot.authMode = "import_backup"
                            walletRoot.restoreMnemonicDraft = ""
                        }
                    }
                }

                AppComponents.AppTextField {
                    id: walletNameField
                    Layout.fillWidth: true
                    visible: walletRoot.authMode !== "unlock" && walletRoot.authMode !== "import_backup"
                    font.pixelSize: walletRoot.controlTextSize
                    text: walletPageSettings.walletNameDraft
                    placeholderText: walletRoot.tf("browser_wallet_name_placeholder", "Wallet name")
                    onTextChanged: {
                        walletPageSettings.walletNameDraft = text
                    }
                }

                AppComponents.AppTextArea {
                    id: restoreMnemonicArea
                    Layout.fillWidth: true
                    Layout.preferredHeight: walletRoot.authMode === "restore" ? (veryPhoneMode ? 104 : (phoneMode ? 116 : 132)) : 0
                    visible: walletRoot.authMode === "restore"
                    editorTitle: walletRoot.tf("browser_wallet_paste_restore_title", "Paste Seed Phrase")
                    font.pixelSize: walletRoot.controlTextSize
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    persistentSelection: true
                    activeFocusOnPress: true
                    text: walletRoot.restoreMnemonicDraft
                    placeholderText: walletRoot.tf("browser_wallet_mnemonic_placeholder", "24-word mnemonic for importing an existing wallet.")
                    onTextChanged: {
                        walletRoot.restoreMnemonicDraft = text
                    }
                }

                AppComponents.AppTextArea {
                    id: importBackupArea
                    Layout.fillWidth: true
                    Layout.preferredHeight: walletRoot.authMode === "import_backup" ? (veryPhoneMode ? 136 : (phoneMode ? 152 : 176)) : 0
                    visible: walletRoot.authMode === "import_backup"
                    editorTitle: walletRoot.tf("browser_wallet_paste_backup_title", "Paste Encrypted Backup")
                    font.pixelSize: walletRoot.controlTextSize
                    wrapMode: TextEdit.WrapAnywhere
                    selectByMouse: true
                    persistentSelection: true
                    activeFocusOnPress: true
                    text: walletRoot.backupImportDraft
                    placeholderText: walletRoot.tf("browser_wallet_backup_import_placeholder", "Paste encrypted wallet backup JSON here.")
                    onTextChanged: {
                        walletRoot.backupImportDraft = text
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: walletRoot.authMode === "restore"
                    Item { Layout.fillWidth: true }

                    Button {
                        text: walletRoot.tf("browser_wallet_paste", "Paste")
                        font.pixelSize: walletRoot.controlTextSize
                        onClicked: {
                            walletRoot.openPasteDialog(
                                restoreMnemonicArea,
                                walletRoot.tf("browser_wallet_paste_restore_title", "Paste Seed Phrase"),
                                walletRoot.tf("browser_wallet_mnemonic_placeholder", "24-word mnemonic for importing an existing wallet."))
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: walletRoot.authMode === "import_backup"
                    Item { Layout.fillWidth: true }

                    Button {
                        text: walletRoot.tf("browser_wallet_paste", "Paste")
                        font.pixelSize: walletRoot.controlTextSize
                        onClicked: {
                            walletRoot.openPasteDialog(
                                importBackupArea,
                                walletRoot.tf("browser_wallet_paste_backup_title", "Paste Encrypted Backup"),
                                walletRoot.tf("browser_wallet_backup_import_placeholder", "Paste encrypted wallet backup JSON here."))
                        }
                    }
                }

                AppComponents.AppTextField {
                    id: unlockPasswordField
                    Layout.fillWidth: true
                    visible: walletRoot.authMode === "unlock"
                    editorTitle: walletRoot.tf("browser_wallet_login_title", "Unlock Your Wallet")
                    font.pixelSize: walletRoot.controlTextSize
                    echoMode: TextInput.Password
                    text: walletRoot.unlockPasswordDraft
                    placeholderText: walletRoot.tf("browser_wallet_password_placeholder", "Encryption password")
                    onTextChanged: {
                        walletRoot.unlockPasswordDraft = text
                    }
                    onAccepted: walletRoot.submitAuth()
                }

                AppComponents.AppTextField {
                    id: createPasswordField
                    Layout.fillWidth: true
                    visible: walletRoot.authMode !== "unlock" && walletRoot.authMode !== "import_backup"
                    editorTitle: walletRoot.tf("browser_wallet_setup_title", "Set Up Your Wallet")
                    font.pixelSize: walletRoot.controlTextSize
                    echoMode: TextInput.Password
                    text: walletRoot.passwordDraft
                    placeholderText: walletRoot.tf("browser_wallet_password_placeholder", "Encryption password")
                    onTextChanged: {
                        walletRoot.passwordDraft = text
                    }
                }

                AppComponents.AppTextField {
                    id: confirmPasswordField
                    Layout.fillWidth: true
                    visible: walletRoot.authMode !== "unlock" && walletRoot.authMode !== "import_backup"
                    editorTitle: walletRoot.tf("browser_wallet_confirm_password_placeholder", "Confirm password")
                    font.pixelSize: walletRoot.controlTextSize
                    echoMode: TextInput.Password
                    text: walletRoot.passwordConfirmDraft
                    placeholderText: walletRoot.tf("browser_wallet_confirm_password_placeholder", "Confirm password")
                    onTextChanged: {
                        walletRoot.passwordConfirmDraft = text
                    }
                    onAccepted: walletRoot.submitAuth()
                }

                Label {
                    Layout.fillWidth: true
                    visible: walletRoot.authMode !== "unlock"
                             && walletRoot.authMode !== "import_backup"
                             && walletRoot.passwordDraft.length > 0
                    text: walletRoot.passwordDraft === walletRoot.passwordConfirmDraft
                          ? walletRoot.tf("browser_wallet_password_match", "Passwords match.")
                          : walletRoot.tf("browser_wallet_password_no_match", "Passwords do not match yet.")
                    color: walletRoot.passwordDraft === walletRoot.passwordConfirmDraft ? "#8ff0c8" : "#ffb4b4"
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    visible: false
                    text: grinWalletController.lastError
                    color: "#ffb4b4"
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    visible: grinWalletController.lastInfo.length > 0
                    text: grinWalletController.lastInfo
                    color: "#8ff0c8"
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true

                    Button {
                        visible: grinWalletController.walletExists
                        text: walletRoot.tf("browser_wallet_delete", "Delete Wallet")
                        onClicked: walletRoot.deleteConfirmOpen = true
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        text: walletRoot.authMode === "unlock"
                              ? walletRoot.tf("browser_wallet_unlock", "Unlock")
                              : (walletRoot.authMode === "create"
                                 ? walletRoot.tf("browser_wallet_create", "Create")
                                 : (walletRoot.authMode === "restore"
                                    ? walletRoot.tf("browser_wallet_restore", "Restore")
                                    : walletRoot.tf("browser_wallet_import_backup", "Import Backup")))
                        enabled: walletRoot.authMode === "unlock"
                                 ? walletRoot.unlockPasswordDraft.length > 0
                                 : (walletRoot.authMode === "import_backup"
                                    ? walletRoot.backupImportDraft.trim().length > 0
                                    : (walletPageSettings.walletNameDraft.trim().length > 0
                                       && walletRoot.passwordDraft.length > 0
                                       && walletRoot.passwordDraft === walletRoot.passwordConfirmDraft
                                       && (walletRoot.authMode !== "restore" || walletRoot.restoreMnemonicDraft.trim().length > 0)))
                        onClicked: walletRoot.submitAuth()
                    }
                }
            }
        }
    }
}