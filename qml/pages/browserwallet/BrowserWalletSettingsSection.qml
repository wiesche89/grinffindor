import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../components" as AppComponents

Item {
    id: settingsSection
    property var walletRoot

    implicitHeight: settingsCard.implicitHeight

    // Reusable button style helpers
    function ghostBtn(ctrl) {
        return { bg: ctrl.down ? "#07111c" : (ctrl.hovered ? "#060e18" : "transparent"),
                 border: ctrl.hovered ? "#1e3a52" : "#152a3c",
                 text: !ctrl.enabled ? "#283c50" : (ctrl.hovered ? "#88b8d8" : "#5a8eac") }
    }

    BrowserWalletSectionCard {
        id: settingsCard
        width: parent.width
        title: walletRoot.tf("browser_wallet_nav_settings", "Settings")

        ColumnLayout {
            id: settingsColumn
            width: parent.width
            spacing: 8

            // Seed Phrase
            BrowserWalletPanel {
                Layout.fillWidth: true
                title: walletRoot.tf("browser_wallet_seed_manage_title", "Seed Phrase")
                description: walletRoot.tf("browser_wallet_seed_manage_note", "Reveal the seed phrase only when you need to verify or back it up. Password confirmation is required every time.")

                ColumnLayout {
                    spacing: 8

                    Flow {
                        Layout.fillWidth: true
                        spacing: 8

                        Button {
                            text: walletRoot.tf("browser_wallet_seed_show", "Show Seed Phrase")
                            font.pixelSize: walletRoot.controlTextSize
                            background: Rectangle {
                                radius: 10; color: parent.down ? "#07111c" : (parent.hovered ? "#060e18" : "transparent")
                                border.color: parent.hovered ? "#1e3a52" : "#152a3c"; border.width: 1
                            }
                            contentItem: Label {
                                text: parent.text; font: parent.font; color: parent.hovered ? "#88b8d8" : "#5a8eac"
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: walletRoot.openRevealSeedPopup()
                        }

                        Button {
                            text: walletRoot.tf("browser_wallet_seed_hide", "Hide Seed Phrase")
                            font.pixelSize: walletRoot.controlTextSize
                            enabled: grinWalletController.mnemonicPreview.length > 0
                            background: Rectangle {
                                radius: 10; color: parent.down ? "#07111c" : (parent.hovered ? "#060e18" : "transparent")
                                border.color: parent.hovered ? "#1e3a52" : "#152a3c"; border.width: 1
                            }
                            contentItem: Label {
                                text: parent.text; font: parent.font
                                color: !parent.enabled ? "#283c50" : (parent.hovered ? "#88b8d8" : "#5a8eac")
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: grinWalletController.dismissMnemonicPreview()
                        }
                    }

                    AppComponents.AppTextArea {
                        id: settingsSeedArea
                        Layout.fillWidth: true
                        Layout.preferredHeight: grinWalletController.mnemonicPreview.length > 0 ? 108 : 0
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

            // Security
            BrowserWalletPanel {
                Layout.fillWidth: true
                title: walletRoot.tf("settings_security_title", "Security")
                description: walletRoot.tf("settings_auto_lock_note", "Lock the wallet automatically when the app loses focus or is minimized.")

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("settings_auto_lock_label", "Lock on app exit / focus loss")
                        color: "#8ab8d0"
                        font.pixelSize: walletRoot.bodyTextSize
                    }

                    Rectangle {
                        id: securityToggle
                        width: 44
                        height: 24
                        radius: 12
                        color: grinWalletController && grinWalletController.autoLockOnAppDeactivate ? "#181200" : "#0c1c28"
                        border.color: grinWalletController && grinWalletController.autoLockOnAppDeactivate ? "#FEF102" : "#1a3448"
                        border.width: 1

                        Rectangle {
                            width: 18; height: 18; radius: 9
                            anchors.verticalCenter: parent.verticalCenter
                            x: grinWalletController && grinWalletController.autoLockOnAppDeactivate ? parent.width - width - 3 : 3
                            color: grinWalletController && grinWalletController.autoLockOnAppDeactivate ? "#FEF102" : "#3a6080"

                            Behavior on x { NumberAnimation { duration: 140; easing.type: Easing.InOutQuad } }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (grinWalletController)
                                    grinWalletController.setAutoLockOnAppDeactivate(!grinWalletController.autoLockOnAppDeactivate)
                            }
                        }
                    }
                }
            }

            // Storage Durability
            BrowserWalletPanel {
                Layout.fillWidth: true
                title: walletRoot.tf("browser_wallet_storage_title", "Storage Durability")

                ColumnLayout {
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.storageStatusText()
                        color: walletRoot.storageStatusColor()
                        font.pixelSize: walletRoot.bodyTextSize
                        wrapMode: Text.WordWrap
                    }

                    Button {
                        text: walletRoot.tf("browser_wallet_storage_request", "Request Persistent Storage")
                        font.pixelSize: walletRoot.controlTextSize
                        enabled: grinWalletController.storagePersistenceState !== "native"
                              && grinWalletController.storagePersistenceState !== "persistent"
                        background: Rectangle {
                            radius: 10; color: parent.down ? "#07111c" : (parent.hovered ? "#060e18" : "transparent")
                            border.color: parent.hovered ? "#1e3a52" : "#152a3c"; border.width: 1
                        }
                        contentItem: Label {
                            text: parent.text; font: parent.font
                            color: !parent.enabled ? "#283c50" : (parent.hovered ? "#88b8d8" : "#5a8eac")
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: grinWalletController.requestPersistentBrowserStorage()
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: grinWalletController.storagePersistenceState === "native"
                        text: walletRoot.tf("browser_wallet_storage_request_browser_only", "This option applies only to Browser/Wasm builds.")
                        color: "#5a8eaa"
                        font.pixelSize: walletRoot.compactTextSize
                        wrapMode: Text.WordWrap
                    }
                }
            }

            // Encrypted Backup
            BrowserWalletPanel {
                Layout.fillWidth: true
                title: walletRoot.tf("browser_wallet_backup_export", "Encrypted Backup")
                description: walletRoot.tf("browser_wallet_backup_download_note", "Download a fresh encrypted backup file for this wallet. Inline display and clipboard copy are intentionally not offered here.")

                Button {
                    text: walletRoot.tf("browser_wallet_backup_download", "Download Backup")
                    font.pixelSize: walletRoot.controlTextSize
                    enabled: grinWalletController.storagePersistenceState !== "native"
                    background: Rectangle {
                        radius: 10; color: parent.down ? "#07111c" : (parent.hovered ? "#060e18" : "transparent")
                        border.color: parent.hovered ? "#1e3a52" : "#152a3c"; border.width: 1
                    }
                    contentItem: Label {
                        text: parent.text; font: parent.font
                        color: !parent.enabled ? "#283c50" : (parent.hovered ? "#88b8d8" : "#5a8eac")
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        var backup = grinWalletController.exportEncryptedWalletBackup()
                        if (backup.trim().length > 0)
                            grinWalletController.downloadTextFile(
                                "grinffindor-wallet-backup-" + grinWalletController.selectedNetwork + ".json",
                                backup)
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: grinWalletController.storagePersistenceState === "native"
                    text: walletRoot.tf("browser_wallet_backup_download_browser_only", "Backup download from this section applies only to Browser/Wasm builds.")
                    color: "#5a8eaa"
                    font.pixelSize: walletRoot.compactTextSize
                    wrapMode: Text.WordWrap
                }
            }

            // External Node
            BrowserWalletPanel {
                Layout.fillWidth: true
                title: walletRoot.tf("browser_wallet_node_title", "External Node")

                ColumnLayout {
                    spacing: 8

                    AppComponents.AppTextField {
                        id: nodeUrlField
                        Layout.fillWidth: true
                        editorTitle: walletRoot.tf("browser_wallet_node_title", "External Node")
                        inputMode: "url"
                        font.pixelSize: walletRoot.controlTextSize
                        text: walletRoot.nodeUrlDraft
                        placeholderText: walletRoot.tf("browser_wallet_node_input", "https://your-node.example/v2/foreign")
                        onTextChanged: walletRoot.nodeUrlDraft = text
                    }

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("browser_wallet_node_title", "External Node")
                              + " (" + grinWalletController.selectedNetwork + "): "
                              + grinWalletController.nodeUrl
                        color: "#4a7898"
                        font.pixelSize: walletRoot.bodyTextSize
                        wrapMode: Text.WrapAnywhere
                    }

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("browser_wallet_node_note", "Use a Grin foreign API endpoint here. Switching the wallet network resets the node to the matching Grinffindor endpoint.")
                        color: "#2a5060"
                        font.pixelSize: walletRoot.bodyTextSize
                        wrapMode: Text.WordWrap
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: 8

                        Repeater {
                            model: [
                                { label: walletRoot.tf("browser_wallet_save_node", "Save Node"),   enabled: walletRoot.nodeUrlDraft.trim() !== grinWalletController.nodeUrl && grinWalletController.isValidNodeUrl(walletRoot.nodeUrlDraft), action: function() { grinWalletController.setNodeUrl(walletRoot.nodeUrlDraft) } },
                                { label: walletRoot.tf("browser_wallet_reset_node", "Reset Node"), enabled: grinWalletController.nodeUrl !== walletRoot.defaultNetworkNodeUrl(), action: function() { grinWalletController.resetNodeUrl() } },
                                { label: walletRoot.tf("browser_wallet_refresh", "Refresh"),       enabled: true, action: function() { grinWalletController.refreshNodeStatus() } },
                                { label: walletRoot.tf("browser_wallet_rescan", "Full Rescan"),    enabled: walletRoot.nodeStatusMode() === "online", action: function() { grinWalletController.rescanWallet() } }
                            ]

                            Button {
                                text: modelData.label
                                font.pixelSize: walletRoot.controlTextSize
                                enabled: modelData.enabled
                                background: Rectangle {
                                    radius: 10; color: parent.down ? "#07111c" : (parent.hovered ? "#060e18" : "transparent")
                                    border.color: parent.hovered ? "#1e3a52" : "#152a3c"; border.width: 1
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

            // Wallet Maintenance
            BrowserWalletPanel {
                Layout.fillWidth: true
                title: walletRoot.tf("browser_wallet_maintenance_title", "Wallet Maintenance")
                description: walletRoot.tf("browser_wallet_maintenance_note", "Remove local (off-chain) UTXOs and cancelled transactions to clean up your wallet.")

                ColumnLayout {
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("browser_wallet_maintenance_warning", "This action is permanent. Confirmed UTXOs on the blockchain remain intact.")
                        color: "#8ab8d0"
                        font.pixelSize: walletRoot.bodyTextSize
                        wrapMode: Text.WordWrap
                    }

                    Button {
                        text: walletRoot.tf("browser_wallet_maintenance_cleanup", "Clean Up Now")
                        font.pixelSize: walletRoot.controlTextSize
                        enabled: grinWalletController.walletUnlocked
                        background: Rectangle {
                            radius: 10; color: parent.down ? "#07111c" : (parent.hovered ? "#060e18" : "transparent")
                            border.color: parent.hovered ? "#1e3a52" : "#152a3c"; border.width: 1
                        }
                        contentItem: Label {
                            text: parent.text; font: parent.font
                            color: !parent.enabled ? "#283c50" : (parent.hovered ? "#88b8d8" : "#5a8eac")
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: grinWalletController.cleanupLocalAndCancelledItems()
                    }
                }
            }

            // Danger Zone
            BrowserWalletPanel {
                Layout.fillWidth: true
                title: walletRoot.tf("browser_wallet_danger_title", "Danger Zone")
                description: walletRoot.tf("browser_wallet_delete_note", "Delete the currently selected wallet only if you have verified your backup and seed phrase.")

                Button {
                    text: walletRoot.tf("browser_wallet_delete", "Delete Wallet")
                    font.pixelSize: walletRoot.controlTextSize
                    background: Rectangle {
                        radius: 10; color: parent.down ? "#1c0c10" : (parent.hovered ? "#160a0e" : "transparent")
                        border.color: parent.hovered ? "#882838" : "#6a2030"; border.width: 1
                    }
                    contentItem: Label {
                        text: parent.text; font: parent.font
                        color: parent.down ? "#ff5868" : (parent.hovered ? "#ee4a5a" : "#cc3050")
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: walletRoot.deleteConfirmOpen = true
                }
            }
        }
    }
}
