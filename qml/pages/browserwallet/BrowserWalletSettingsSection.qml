import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../components" as AppComponents

Item {
    id: settingsSection
    property var walletRoot

    implicitHeight: settingsCard.implicitHeight

    BrowserWalletSectionCard {
        id: settingsCard
        width: parent.width
        title: walletRoot.tf("browser_wallet_nav_settings", "Settings")

        ColumnLayout {
            id: settingsColumn
            width: parent.width
            spacing: 12

            BrowserWalletPanel {
                Layout.fillWidth: true
                title: walletRoot.tf("browser_wallet_network_title", "Wallet Network")
                description: walletRoot.tf("browser_wallet_backup_export_note", "Export a password-encrypted wallet backup before moving devices or clearing browser storage. This backup keeps local history, scan state, and the encrypted seed together.")

                ColumnLayout {
                    id: overviewSettingsColumn
                    spacing: 6

                    Label {
                        Layout.fillWidth: true
                        text: grinWalletController.selectedNetwork
                        color: "#8ff0c8"
                        font.pixelSize: walletRoot.bodyTextSize
                        wrapMode: Text.WordWrap
                    }
                }
            }

            BrowserWalletPanel {
                Layout.fillWidth: true
                title: walletRoot.tf("browser_wallet_seed_manage_title", "Seed Phrase")
                description: walletRoot.tf("browser_wallet_seed_manage_note", "Reveal the seed phrase only when you need to verify or back it up. Password confirmation is required every time.")

                ColumnLayout {
                    id: seedSettingsColumn
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true

                        Button {
                            text: walletRoot.tf("browser_wallet_seed_show", "Show Seed Phrase")
                            font.pixelSize: walletRoot.controlTextSize
                            onClicked: walletRoot.openRevealSeedPopup()
                        }

                        Button {
                            text: walletRoot.tf("browser_wallet_seed_hide", "Hide Seed Phrase")
                            font.pixelSize: walletRoot.controlTextSize
                            enabled: grinWalletController.mnemonicPreview.length > 0
                            onClicked: grinWalletController.dismissMnemonicPreview()
                        }

                        Item { Layout.fillWidth: true }
                    }

                    AppComponents.AppTextArea {
                        id: settingsSeedArea
                        Layout.fillWidth: true
                        Layout.preferredHeight: grinWalletController.mnemonicPreview.length > 0 ? 116 : 0
                        visible: grinWalletController.mnemonicPreview.length > 0
                        editorTitle: walletRoot.tf("browser_wallet_seed_manage_title", "Seed Phrase")
                        readOnly: true
                        font.pixelSize: walletRoot.controlTextSize
                        wrapMode: TextEdit.Wrap
                        textFormat: TextEdit.PlainText
                        selectByMouse: true
                        persistentSelection: true
                        activeFocusOnPress: true
                        text: grinWalletController.mnemonicPreview
                    }
                }
            }

            BrowserWalletPanel {
                Layout.fillWidth: true
                title: walletRoot.tf("settings_security_title", "Security")
                description: walletRoot.tf("settings_auto_lock_note", "Lock the wallet automatically when the app loses focus or is minimized.")

                ColumnLayout {
                    id: securitySettingsColumn
                    spacing: 8

                    Switch {
                        Layout.alignment: Qt.AlignLeft
                        checked: grinWalletController ? grinWalletController.autoLockOnAppDeactivate : false
                        text: walletRoot.tf("settings_auto_lock_label", "Lock on app exit/focus loss")
                        font.pixelSize: walletRoot.controlTextSize

                        onToggled: {
                            if (grinWalletController)
                                grinWalletController.setAutoLockOnAppDeactivate(checked)
                        }
                    }
                }
            }

            BrowserWalletPanel {
                Layout.fillWidth: true
                title: walletRoot.tf("browser_wallet_storage_title", "Storage Durability")
                fillColor: "#132635"
                strokeColor: walletRoot.storageStatusColor()

                ColumnLayout {
                    id: storageColumn
                    spacing: 6

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.storageStatusText()
                        color: walletRoot.storageStatusColor()
                        font.pixelSize: walletRoot.bodyTextSize
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Button {
                            text: walletRoot.tf("browser_wallet_storage_request", "Request Persistent Storage")
                            font.pixelSize: walletRoot.controlTextSize
                            enabled: grinWalletController.storagePersistenceState !== "native"
                                  && grinWalletController.storagePersistenceState !== "persistent"
                            onClicked: grinWalletController.requestPersistentBrowserStorage()
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }

            BrowserWalletPanel {
                Layout.fillWidth: true
                title: walletRoot.tf("browser_wallet_backup_export", "Encrypted Backup")
                description: walletRoot.tf("browser_wallet_backup_download_note", "Download a fresh encrypted backup file for this wallet. Inline display and clipboard copy are intentionally not offered here.")

                ColumnLayout {
                    id: backupSettingsColumn
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true

                        Button {
                            text: walletRoot.tf("browser_wallet_backup_download", "Download Backup")
                            font.pixelSize: walletRoot.controlTextSize
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

            BrowserWalletPanel {
                Layout.fillWidth: true
                title: walletRoot.tf("browser_wallet_node_title", "External Node")

                ColumnLayout {
                    id: nodeSettingsColumn
                    spacing: 8

                    AppComponents.AppTextField {
                        id: nodeUrlField
                        Layout.fillWidth: true
                        editorTitle: walletRoot.tf("browser_wallet_node_title", "External Node")
                        inputMode: "url"
                        font.pixelSize: walletRoot.controlTextSize
                        text: walletRoot.nodeUrlDraft
                        placeholderText: walletRoot.tf("browser_wallet_node_input", "https://your-node.example/v2/foreign")
                        onTextChanged: {
                            walletRoot.nodeUrlDraft = text
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("browser_wallet_node_title", "External Node") + " (" + grinWalletController.selectedNetwork + "): " + grinWalletController.nodeUrl
                        color: "#d7e9f4"
                        font.pixelSize: walletRoot.bodyTextSize
                        wrapMode: Text.WrapAnywhere
                    }

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("browser_wallet_node_note", "Use a Grin foreign API endpoint here. Switching the wallet network resets the node to the matching Grinffindor endpoint.")
                        color: "#8fb4c9"
                        font.pixelSize: walletRoot.bodyTextSize
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Button {
                            text: walletRoot.tf("browser_wallet_save_node", "Save Node")
                            font.pixelSize: walletRoot.controlTextSize
                            enabled: walletRoot.nodeUrlDraft.trim() !== grinWalletController.nodeUrl
                                     && grinWalletController.isValidNodeUrl(walletRoot.nodeUrlDraft)
                            onClicked: grinWalletController.setNodeUrl(walletRoot.nodeUrlDraft)
                        }

                        Button {
                            text: walletRoot.tf("browser_wallet_reset_node", "Reset Node")
                            font.pixelSize: walletRoot.controlTextSize
                            enabled: grinWalletController.nodeUrl !== walletRoot.defaultNetworkNodeUrl()
                            onClicked: grinWalletController.resetNodeUrl()
                        }

                        Button {
                            text: walletRoot.tf("browser_wallet_refresh", "Refresh")
                            font.pixelSize: walletRoot.controlTextSize
                            onClicked: grinWalletController.refreshNodeStatus()
                        }

                        Button {
                            text: walletRoot.tf("browser_wallet_rescan", "Full Rescan")
                            font.pixelSize: walletRoot.controlTextSize
                            enabled: walletRoot.nodeStatusMode() === "online"
                            onClicked: grinWalletController.rescanWallet()
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }

            BrowserWalletPanel {
                Layout.fillWidth: true
                fillColor: walletRoot.nodeStatusMode() === "offline" ? "#34191d"
                         : (walletRoot.nodeStatusMode() === "connecting" ? "#2d2415" : "#132635")
                strokeColor: walletRoot.recoveryBannerColor()
                title: walletRoot.tf("browser_wallet_operational_status", "Operational Status")

                ColumnLayout {
                    id: settingsStatusColumn
                    spacing: 6

                    Label {
                        Layout.fillWidth: true
                        text: grinWalletController.syncStatus
                        color: "#ffffff"
                        font.pixelSize: walletRoot.bodyTextSize
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.recoveryBannerText().length > 0
                              ? walletRoot.recoveryBannerText()
                              : walletRoot.tf("browser_wallet_operational_ok", "Node is reachable and no pending recovery actions are currently flagged.")
                        color: walletRoot.recoveryBannerText().length > 0 ? walletRoot.recoveryBannerColor() : "#8ff0c8"
                        font.pixelSize: walletRoot.bodyTextSize
                        wrapMode: Text.WordWrap
                    }
                }
            }

            BrowserWalletPanel {
                Layout.fillWidth: true
                title: walletRoot.tf("browser_wallet_balances_title", "Wallet Balances")

                ColumnLayout {
                    id: balanceSettingsColumn
                    spacing: 8

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: 10
                        columnSpacing: 12

                        Label { text: walletRoot.tf("browser_wallet_total", "Total"); color: "#8fb4c9"; font.pixelSize: walletRoot.bodyTextSize }
                        Label { text: grinWalletController.totalBalance + " GRIN"; color: "#ffffff"; font.pixelSize: walletRoot.bodyTextSize }
                        Label { text: walletRoot.tf("browser_wallet_spendable", "Spendable"); color: "#8fb4c9"; font.pixelSize: walletRoot.bodyTextSize }
                        Label { text: grinWalletController.spendableBalance + " GRIN"; color: "#ffffff"; font.pixelSize: walletRoot.bodyTextSize }
                        Label { text: walletRoot.tf("browser_wallet_locked", "Locked"); color: "#8fb4c9"; font.pixelSize: walletRoot.bodyTextSize }
                        Label { text: grinWalletController.lockedBalance + " GRIN"; color: "#ffffff"; font.pixelSize: walletRoot.bodyTextSize }
                        Label { text: walletRoot.tf("browser_wallet_immature", "Immature"); color: "#8fb4c9"; font.pixelSize: walletRoot.bodyTextSize }
                        Label { text: grinWalletController.immatureBalance + " GRIN"; color: "#ffffff"; font.pixelSize: walletRoot.bodyTextSize }
                        Label { text: walletRoot.tf("browser_wallet_awaiting_confirmation", "Awaiting Confirmation"); color: "#8fb4c9"; font.pixelSize: walletRoot.bodyTextSize }
                        Label { text: grinWalletController.awaitingConfirmationBalance + " GRIN"; color: "#ffffff"; font.pixelSize: walletRoot.bodyTextSize }
                        Label { text: walletRoot.tf("browser_wallet_awaiting_finalization", "Awaiting Finalization"); color: "#8fb4c9"; font.pixelSize: walletRoot.bodyTextSize }
                        Label { text: grinWalletController.awaitingFinalizationBalance + " GRIN"; color: "#ffffff"; font.pixelSize: walletRoot.bodyTextSize }
                    }
                }
            }

            BrowserWalletPanel {
                Layout.fillWidth: true
                title: walletRoot.tf("browser_wallet_maintenance_title", "Wallet Maintenance")
                description: walletRoot.tf("browser_wallet_maintenance_note", "Remove local (off-chain) UTXOs and cancelled transactions to clean up your wallet.")

                ColumnLayout {
                    id: maintenanceSettingsColumn
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("browser_wallet_maintenance_warning", "This action is permanent. Confirmed UTXOs on the blockchain remain intact.")
                        color: "#ffc8a8"
                        wrapMode: Text.WordWrap
                        font.pixelSize: walletRoot.bodyTextSize
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Button {
                            text: walletRoot.tf("browser_wallet_maintenance_cleanup", "Clean Up Now")
                            font.pixelSize: walletRoot.controlTextSize
                            enabled: grinWalletController.walletUnlocked
                            onClicked: grinWalletController.cleanupLocalAndCancelledItems()
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }

            BrowserWalletPanel {
                Layout.fillWidth: true
                fillColor: "#34191d"
                strokeColor: "#8b3c46"
                title: walletRoot.tf("browser_wallet_danger_title", "Danger Zone")
                description: walletRoot.tf("browser_wallet_delete_note", "Delete the currently selected wallet only if you have verified your backup and seed phrase.")
                descriptionColor: "#ffd6d6"

                ColumnLayout {
                    id: dangerSettingsColumn
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true

                        Button {
                            text: walletRoot.tf("browser_wallet_delete", "Delete Wallet")
                            font.pixelSize: walletRoot.controlTextSize
                            onClicked: walletRoot.deleteConfirmOpen = true
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }
    }
}