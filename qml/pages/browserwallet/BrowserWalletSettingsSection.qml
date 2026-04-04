import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: settingsSection
    property var walletRoot

    implicitHeight: settingsCard.implicitHeight

    Rectangle {
        id: settingsCard
        width: parent.width
        radius: 26
        color: "#0f1b26"
        border.color: "#26465b"
        implicitHeight: settingsColumn.implicitHeight + 34

        ColumnLayout {
            id: settingsColumn
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            Label {
                text: walletRoot.tf("browser_wallet_nav_settings", "Settings")
                color: "#ffffff"
                font.pixelSize: 28
                font.weight: Font.Bold
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 16
                color: "#132635"
                border.color: "#2a4f64"
                implicitHeight: overviewSettingsColumn.implicitHeight + 20

                ColumnLayout {
                    id: overviewSettingsColumn
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 6

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("browser_wallet_network_title", "Wallet Network")
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        Layout.fillWidth: true
                        text: grinWalletController.selectedNetwork
                        color: "#8ff0c8"
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("browser_wallet_backup_export_note", "Export a password-encrypted wallet backup before moving devices or clearing browser storage. This backup keeps local history, scan state, and the encrypted seed together.")
                        color: "#d7e9f4"
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 16
                color: "#132635"
                border.color: "#2a4f64"
                implicitHeight: seedSettingsColumn.implicitHeight + 20

                ColumnLayout {
                    id: seedSettingsColumn
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("browser_wallet_seed_manage_title", "Seed Phrase")
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("browser_wallet_seed_manage_note", "Reveal the seed phrase only when you need to verify or back it up. Password confirmation is required every time.")
                        color: "#d7e9f4"
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Button {
                            text: walletRoot.tf("browser_wallet_seed_show", "Show Seed Phrase")
                            onClicked: walletRoot.openRevealSeedPopup()
                        }

                        Button {
                            text: walletRoot.tf("browser_wallet_seed_hide", "Hide Seed Phrase")
                            enabled: grinWalletController.mnemonicPreview.length > 0
                            onClicked: grinWalletController.dismissMnemonicPreview()
                        }

                        Item { Layout.fillWidth: true }
                    }

                    TextArea {
                        id: settingsSeedArea
                        Layout.fillWidth: true
                        Layout.preferredHeight: grinWalletController.mnemonicPreview.length > 0 ? 116 : 0
                        visible: grinWalletController.mnemonicPreview.length > 0
                        readOnly: true
                        wrapMode: TextEdit.Wrap
                        textFormat: TextEdit.PlainText
                        selectByMouse: true
                        persistentSelection: true
                        activeFocusOnPress: true
                        text: grinWalletController.mnemonicPreview
                        onActiveFocusChanged: walletRoot.syncBrowserShortcutContext(settingsSeedArea)
                        onSelectedTextChanged: walletRoot.syncBrowserShortcutContext(settingsSeedArea)
                        onTextChanged: walletRoot.syncBrowserShortcutContext(settingsSeedArea)
                        Keys.onPressed: function(event) { walletRoot.handleTextControlKeyPress(settingsSeedArea, event) }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 16
                color: "#132635"
                border.color: "#2a4f64"
                implicitHeight: securitySettingsColumn.implicitHeight + 20

                ColumnLayout {
                    id: securitySettingsColumn
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("settings_security_title", "Security")
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("settings_auto_lock_note", "Lock the wallet automatically when the app loses focus or is minimized.")
                        color: "#d7e9f4"
                        wrapMode: Text.WordWrap
                    }

                    Switch {
                        Layout.alignment: Qt.AlignLeft
                        checked: grinWalletController ? grinWalletController.autoLockOnAppDeactivate : false
                        text: walletRoot.tf("settings_auto_lock_label", "Lock on app exit/focus loss")

                        onToggled: {
                            if (grinWalletController)
                                grinWalletController.setAutoLockOnAppDeactivate(checked)
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 16
                color: "#132635"
                border.color: walletRoot.storageStatusColor()
                implicitHeight: storageColumn.implicitHeight + 20

                ColumnLayout {
                    id: storageColumn
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 6

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("browser_wallet_storage_title", "Storage Durability")
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.storageStatusText()
                        color: walletRoot.storageStatusColor()
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Button {
                            text: walletRoot.tf("browser_wallet_storage_request", "Request Persistent Storage")
                            enabled: grinWalletController.storagePersistenceState !== "native"
                                  && grinWalletController.storagePersistenceState !== "persistent"
                            onClicked: grinWalletController.requestPersistentBrowserStorage()
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 16
                color: "#132635"
                border.color: "#2a4f64"
                implicitHeight: backupSettingsColumn.implicitHeight + 20

                ColumnLayout {
                    id: backupSettingsColumn
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("browser_wallet_backup_export", "Encrypted Backup")
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("browser_wallet_backup_download_note", "Download a fresh encrypted backup file for this wallet. Inline display and clipboard copy are intentionally not offered here.")
                        color: "#d7e9f4"
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Button {
                            text: walletRoot.tf("browser_wallet_backup_download", "Download Backup")
                            enabled: grinWalletController.storagePersistenceState !== "native"
                            onClicked: {
                                var backup = grinWalletController.exportEncryptedWalletBackup()
                                if (backup.trim().length > 0) {
                                    grinWalletController.downloadTextFile(
                                        "grinffindor-wallet-backup-" + grinWalletController.selectedNetwork + ".json",
                                        backup)
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 16
                color: "#132635"
                border.color: "#2a4f64"
                implicitHeight: nodeSettingsColumn.implicitHeight + 20

                ColumnLayout {
                    id: nodeSettingsColumn
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("browser_wallet_node_title", "External Node")
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }

                    TextField {
                        id: nodeUrlField
                        Layout.fillWidth: true
                        text: walletRoot.nodeUrlDraft
                        placeholderText: walletRoot.tf("browser_wallet_node_input", "https://your-node.example/v2/foreign")
                        onActiveFocusChanged: walletRoot.syncBrowserShortcutContext(nodeUrlField)
                        onSelectedTextChanged: walletRoot.syncBrowserShortcutContext(nodeUrlField)
                        onTextChanged: {
                            walletRoot.nodeUrlDraft = text
                            walletRoot.syncBrowserShortcutContext(nodeUrlField)
                        }
                        Keys.onPressed: function(event) { walletRoot.handleTextControlKeyPress(nodeUrlField, event) }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("browser_wallet_node_title", "External Node") + " (" + grinWalletController.selectedNetwork + "): " + grinWalletController.nodeUrl
                        color: "#d7e9f4"
                        wrapMode: Text.WrapAnywhere
                    }

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("browser_wallet_node_note", "Use a Grin foreign API endpoint here. Switching the wallet network resets the node to the matching Grinffindor endpoint.")
                        color: "#8fb4c9"
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Button {
                            text: walletRoot.tf("browser_wallet_save_node", "Save Node")
                            enabled: walletRoot.nodeUrlDraft.trim() !== grinWalletController.nodeUrl
                                     && grinWalletController.isValidNodeUrl(walletRoot.nodeUrlDraft)
                            onClicked: grinWalletController.setNodeUrl(walletRoot.nodeUrlDraft)
                        }

                        Button {
                            text: walletRoot.tf("browser_wallet_reset_node", "Reset Node")
                            enabled: grinWalletController.nodeUrl !== walletRoot.defaultNetworkNodeUrl()
                            onClicked: grinWalletController.resetNodeUrl()
                        }

                        Button {
                            text: walletRoot.tf("browser_wallet_refresh", "Refresh")
                            onClicked: grinWalletController.refreshNodeStatus()
                        }

                        Button {
                            text: walletRoot.tf("browser_wallet_rescan", "Full Rescan")
                            enabled: walletRoot.nodeStatusMode() === "online"
                            onClicked: grinWalletController.rescanWallet()
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 16
                color: walletRoot.nodeStatusMode() === "offline" ? "#34191d"
                     : (walletRoot.nodeStatusMode() === "connecting" ? "#2d2415" : "#132635")
                border.color: walletRoot.recoveryBannerColor()
                implicitHeight: settingsStatusColumn.implicitHeight + 20

                ColumnLayout {
                    id: settingsStatusColumn
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 6

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("browser_wallet_operational_status", "Operational Status") + ": " + grinWalletController.syncStatus
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.recoveryBannerText().length > 0
                              ? walletRoot.recoveryBannerText()
                              : walletRoot.tf("browser_wallet_operational_ok", "Node is reachable and no pending recovery actions are currently flagged.")
                        color: walletRoot.recoveryBannerText().length > 0 ? walletRoot.recoveryBannerColor() : "#8ff0c8"
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 16
                color: "#132635"
                border.color: "#2a4f64"
                implicitHeight: balanceSettingsColumn.implicitHeight + 20

                ColumnLayout {
                    id: balanceSettingsColumn
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("browser_wallet_balances_title", "Wallet Balances")
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: 10
                        columnSpacing: 12

                        Label { text: walletRoot.tf("browser_wallet_total", "Total"); color: "#8fb4c9" }
                        Label { text: grinWalletController.totalBalance + " GRIN"; color: "#ffffff" }
                        Label { text: walletRoot.tf("browser_wallet_spendable", "Spendable"); color: "#8fb4c9" }
                        Label { text: grinWalletController.spendableBalance + " GRIN"; color: "#ffffff" }
                        Label { text: walletRoot.tf("browser_wallet_locked", "Locked"); color: "#8fb4c9" }
                        Label { text: grinWalletController.lockedBalance + " GRIN"; color: "#ffffff" }
                        Label { text: walletRoot.tf("browser_wallet_immature", "Immature"); color: "#8fb4c9" }
                        Label { text: grinWalletController.immatureBalance + " GRIN"; color: "#ffffff" }
                        Label { text: walletRoot.tf("browser_wallet_awaiting_confirmation", "Awaiting Confirmation"); color: "#8fb4c9" }
                        Label { text: grinWalletController.awaitingConfirmationBalance + " GRIN"; color: "#ffffff" }
                        Label { text: walletRoot.tf("browser_wallet_awaiting_finalization", "Awaiting Finalization"); color: "#8fb4c9" }
                        Label { text: grinWalletController.awaitingFinalizationBalance + " GRIN"; color: "#ffffff" }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 16
                color: "#132635"
                border.color: "#2a4f64"
                implicitHeight: maintenanceSettingsColumn.implicitHeight + 20

                ColumnLayout {
                    id: maintenanceSettingsColumn
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("browser_wallet_maintenance_title", "Wallet Maintenance")
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("browser_wallet_maintenance_note", "Remove local (off-chain) UTXOs and cancelled transactions to clean up your wallet.")
                        color: "#d7e9f4"
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("browser_wallet_maintenance_warning", "This action is permanent. Confirmed UTXOs on the blockchain remain intact.")
                        color: "#ffc8a8"
                        wrapMode: Text.WordWrap
                        font.pixelSize: 13
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Button {
                            text: walletRoot.tf("browser_wallet_maintenance_cleanup", "Clean Up Now")
                            enabled: grinWalletController.walletUnlocked
                            onClicked: grinWalletController.cleanupLocalAndCancelledItems()
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 16
                color: "#34191d"
                border.color: "#8b3c46"
                implicitHeight: dangerSettingsColumn.implicitHeight + 20

                ColumnLayout {
                    id: dangerSettingsColumn
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("browser_wallet_danger_title", "Danger Zone")
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("browser_wallet_delete_note", "Delete the currently selected wallet only if you have verified your backup and seed phrase.")
                        color: "#ffd6d6"
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Button {
                            text: walletRoot.tf("browser_wallet_delete", "Delete Wallet")
                            onClicked: walletRoot.deleteConfirmOpen = true
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }
    }
}