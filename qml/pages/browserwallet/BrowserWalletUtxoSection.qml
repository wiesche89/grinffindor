import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: utxoSection
    property var walletRoot

    implicitHeight: utxoCard.implicitHeight

    BrowserWalletSectionCard {
        id: utxoCard
        width: parent.width
        title: walletRoot.tf("browser_wallet_utxo_title", "Wallet Outputs")
        subtitle: walletRoot.tf("browser_wallet_utxo_note", "Tracked outputs, their wallet state, and the commitments currently held in the local wallet.")

        ColumnLayout {
            id: utxoColumn
            width: parent.width
            spacing: 12

            Label {
                Layout.fillWidth: true
                visible: grinWalletController.walletOutputs.length === 0
                text: walletRoot.tf("browser_wallet_utxo_empty", "No wallet outputs are tracked yet. Run a scan or complete a transaction first.")
                color: "#8fb4c9"
                wrapMode: Text.WordWrap
            }

            Repeater {
                model: grinWalletController.walletOutputs

                BrowserWalletPanel {
                    property bool detailsExpanded: false

                    Layout.fillWidth: true

                    ColumnLayout {
                        id: outputColumn
                        spacing: 8

                        Label {
                            Layout.fillWidth: true
                            text: (modelData.amount || "0.000000000") + " GRIN / "
                                  + walletRoot.tf("browser_wallet_utxo_status", "Status") + ": "
                                  + (modelData.status || "-")
                            color: modelData.status === "spendable" ? "#8ff0c8"
                                 : modelData.status === "spent" ? "#ffb4b4"
                                 : "#ffd280"
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            Layout.fillWidth: true
                            text: walletRoot.tf("browser_wallet_utxo_source", "Source") + ": " + (modelData.source || "-")
                            color: "#d7e9f4"
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            Layout.fillWidth: true
                            text: walletRoot.tf("browser_wallet_history_confirmations", "Confirmations") + ": "
                                  + (modelData.confirmations !== undefined ? modelData.confirmations : 0)
                            color: "#d7e9f4"
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Button {
                                text: detailsExpanded
                                      ? walletRoot.tf("browser_wallet_hide_details", "Hide Details")
                                      : walletRoot.tf("browser_wallet_show_details", "Details")
                                onClicked: detailsExpanded = !detailsExpanded
                            }

                            Item { Layout.fillWidth: true }

                            Button {
                                visible: !!(modelData.locked && modelData.workflow_id && modelData.workflow_id.length > 0)
                                text: walletRoot.tf("browser_wallet_utxo_cancel_lock", "Cancel Lock")
                                enabled: grinWalletController.walletUnlocked
                                onClicked: grinWalletController.cancelTransaction(modelData.workflow_id)
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: detailsExpanded
                            spacing: 8

                            GridLayout {
                                Layout.fillWidth: true
                                columns: width < 820 ? 1 : 2
                                rowSpacing: 6
                                columnSpacing: 12

                                Label {
                                    Layout.fillWidth: true
                                    text: walletRoot.tf("browser_wallet_utxo_height", "Height") + ": "
                                          + ((modelData.height || "").length > 0 ? modelData.height : "-")
                                    color: "#d7e9f4"
                                    wrapMode: Text.WordWrap
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: walletRoot.tf("browser_wallet_utxo_coinbase", "Coinbase") + ": "
                                          + (modelData.coinbase ? walletRoot.tf("browser_wallet_utxo_yes", "yes")
                                                                : walletRoot.tf("browser_wallet_utxo_no", "no"))
                                    color: "#d7e9f4"
                                    wrapMode: Text.WordWrap
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: walletRoot.tf("browser_wallet_utxo_onchain", "On chain") + ": "
                                          + (modelData.on_chain ? walletRoot.tf("browser_wallet_utxo_yes", "yes")
                                                                : walletRoot.tf("browser_wallet_utxo_no", "no"))
                                    color: "#d7e9f4"
                                    wrapMode: Text.WordWrap
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: walletRoot.tf("browser_wallet_utxo_workflow", "Workflow") + ": "
                                          + ((modelData.workflow_id || "").length > 0 ? modelData.workflow_id : "-")
                                    color: "#d7e9f4"
                                    wrapMode: Text.WrapAnywhere
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: walletRoot.tf("browser_wallet_utxo_commitment", "Commitment") + ": " + (modelData.commitment || "-")
                                color: "#8fb4c9"
                                wrapMode: Text.WrapAnywhere
                                font.pixelSize: 12
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: modelData.key_path !== undefined && (modelData.key_path || "").length > 0
                                text: walletRoot.tf("browser_wallet_utxo_keypath", "Key path") + ": " + modelData.key_path
                                color: "#8fb4c9"
                                wrapMode: Text.WrapAnywhere
                                font.pixelSize: 12
                            }
                        }
                    }
                }
            }
        }
    }
}