import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../components" as AppComponents

BrowserWalletSectionCard {
    id: historyCard
    property var walletRoot

    width: parent ? parent.width : 0
    title: walletRoot.tf("browser_wallet_history_title", "Transaction History")

    ColumnLayout {
        id: historyColumn
        width: parent.width
        spacing: 12

        Repeater {
            model: grinWalletController.transactionHistory

            BrowserWalletPanel {
                property bool detailsExpanded: false

                Layout.fillWidth: true
                visible: modelData.workflow_id !== undefined

                ColumnLayout {
                    id: txColumn
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: (modelData.mode || "-") + " / " + (modelData.state || "-") + " / " + (modelData.status || "-")
                        color: walletRoot.txStatusColor(modelData.status || "")
                        font.pixelSize: walletRoot.bodyTextSize
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        Layout.fillWidth: true
                        text: (modelData.amount || "0") + " GRIN  fee " + (modelData.fee || "0")
                        color: "#ffffff"
                        font.pixelSize: walletRoot.bodyTextSize
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("browser_wallet_history_confirmations", "Confirmations") + ": "
                              + (modelData.confirmations !== undefined ? modelData.confirmations : 0)
                        color: "#d7e9f4"
                        font.pixelSize: walletRoot.bodyTextSize
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        Layout.fillWidth: true
                        text: modelData.workflow_id || ""
                        color: "#8fb4c9"
                        wrapMode: Text.WrapAnywhere
                        font.pixelSize: walletRoot.compactTextSize
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Button {
                            text: detailsExpanded
                                  ? walletRoot.tf("browser_wallet_hide_details", "Hide Details")
                                  : walletRoot.tf("browser_wallet_show_details", "Details")
                            font.pixelSize: walletRoot.controlTextSize
                            onClicked: detailsExpanded = !detailsExpanded
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            text: modelData.status === "broadcast_failed"
                                  ? walletRoot.tf("browser_wallet_history_retry_broadcast", "Retry Broadcast")
                                  : walletRoot.tf("browser_wallet_history_broadcast", "Broadcast")
                            font.pixelSize: walletRoot.controlTextSize
                            enabled: walletRoot.canBroadcastEntry(modelData)
                            onClicked: grinWalletController.broadcastTransaction(modelData.workflow_id)
                        }

                        Button {
                            text: walletRoot.tf("browser_wallet_history_cancel", "Cancel")
                            font.pixelSize: walletRoot.controlTextSize
                            enabled: walletRoot.canCancelEntry(modelData)
                            onClicked: grinWalletController.cancelTransaction(modelData.workflow_id)
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: detailsExpanded
                        spacing: 8

                        Label {
                            Layout.fillWidth: true
                            text: walletRoot.tf("browser_wallet_history_confirmed_height", "Confirmed Height") + ": "
                                  + (modelData.confirmed_height !== undefined && modelData.confirmed_height !== "" ? modelData.confirmed_height : "-")
                            color: "#d7e9f4"
                                font.pixelSize: walletRoot.bodyTextSize
                            wrapMode: Text.WordWrap
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: width < 820 ? 1 : 2
                            rowSpacing: 6
                            columnSpacing: 12

                            Label {
                                Layout.fillWidth: true
                                visible: modelData.payment_proof_status !== undefined
                                text: walletRoot.tf("browser_wallet_history_payment_proof", "Payment Proof") + ": "
                                      + (modelData.payment_proof_status || "-")
                                color: modelData.payment_proof_status === "verified" ? "#8ff0c8"
                                     : modelData.payment_proof_status === "invalid" ? "#ffb4b4"
                                     : "#ffd280"
                                  font.pixelSize: walletRoot.bodyTextSize
                                wrapMode: Text.WordWrap
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: modelData.rescan_rebuilt === true
                                text: walletRoot.tf("browser_wallet_history_rescan", "Rebuilt from rescan backup")
                                color: "#8fb4c9"
                                font.pixelSize: walletRoot.bodyTextSize
                                wrapMode: Text.WordWrap
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: modelData.last_node_check !== undefined && (modelData.last_node_check || "").length > 0
                                text: walletRoot.tf("browser_wallet_history_last_node_check", "Last node check") + ": " + modelData.last_node_check
                                color: "#8fb4c9"
                                font.pixelSize: walletRoot.bodyTextSize
                                wrapMode: Text.WordWrap
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: modelData.last_broadcast_attempt !== undefined && (modelData.last_broadcast_attempt || "").length > 0
                                text: walletRoot.tf("browser_wallet_history_last_broadcast", "Last broadcast attempt") + ": " + modelData.last_broadcast_attempt
                                color: "#8fb4c9"
                                font.pixelSize: walletRoot.bodyTextSize
                                wrapMode: Text.WordWrap
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: walletRoot.txRecoveryHint(modelData).length > 0
                            text: walletRoot.txRecoveryHint(modelData)
                            color: walletRoot.txStatusColor(modelData.status || "")
                            font.pixelSize: walletRoot.bodyTextSize
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: modelData.broadcast_error !== undefined && (modelData.broadcast_error || "").length > 0
                            text: walletRoot.tf("browser_wallet_history_broadcast_error", "Broadcast error") + ": " + modelData.broadcast_error
                            color: "#ffb4b4"
                            font.pixelSize: walletRoot.bodyTextSize
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: modelData.output_commitments !== undefined
                                     && modelData.output_commitments.length !== undefined
                                     && modelData.output_commitments.length > 0
                            text: walletRoot.tf("browser_wallet_history_outputs", "Outputs") + ": "
                                  + ((modelData.output_commitments && modelData.output_commitments.join)
                                        ? modelData.output_commitments.join(", ")
                                        : "")
                            color: "#8fb4c9"
                            wrapMode: Text.WrapAnywhere
                                font.pixelSize: walletRoot.compactTextSize
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: modelData.kernel_excess !== undefined && (modelData.kernel_excess || "").length > 0
                            text: walletRoot.tf("browser_wallet_history_kernel", "Kernel Excess") + ": " + modelData.kernel_excess
                            color: "#8fb4c9"
                            wrapMode: Text.WrapAnywhere
                            font.pixelSize: walletRoot.compactTextSize
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: modelData.payment_proof_error !== undefined && (modelData.payment_proof_error || "").length > 0
                            text: walletRoot.tf("browser_wallet_history_payment_proof_error", "Payment proof error") + ": "
                                  + modelData.payment_proof_error
                            color: "#ffb4b4"
                                font.pixelSize: walletRoot.bodyTextSize
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: modelData.payment_proof !== undefined
                                     && modelData.payment_proof.saddr !== undefined
                                     && (modelData.payment_proof.saddr || "").length > 0
                            text: walletRoot.tf("browser_wallet_history_payment_proof_sender", "Proof sender") + ": "
                                  + ((modelData.payment_proof && modelData.payment_proof.saddr) ? modelData.payment_proof.saddr : "")
                            color: "#8fb4c9"
                            wrapMode: Text.WrapAnywhere
                                font.pixelSize: walletRoot.compactTextSize
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: modelData.payment_proof !== undefined
                                     && modelData.payment_proof.raddr !== undefined
                                     && (modelData.payment_proof.raddr || "").length > 0
                            text: walletRoot.tf("browser_wallet_history_payment_proof_receiver", "Proof receiver") + ": "
                                  + ((modelData.payment_proof && modelData.payment_proof.raddr) ? modelData.payment_proof.raddr : "")
                            color: "#8fb4c9"
                            wrapMode: Text.WrapAnywhere
                                font.pixelSize: walletRoot.compactTextSize
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: modelData.payment_proof !== undefined
                                     && modelData.payment_proof.rsig !== undefined
                                     && (modelData.payment_proof.rsig || "").length > 0
                            text: walletRoot.tf("browser_wallet_history_payment_proof_signature", "Receiver signature") + ": "
                                  + ((modelData.payment_proof && modelData.payment_proof.rsig) ? modelData.payment_proof.rsig : "")
                            color: "#8fb4c9"
                            wrapMode: Text.WrapAnywhere
                                font.pixelSize: walletRoot.compactTextSize
                        }

                        AppComponents.AppTextArea {
                            Layout.fillWidth: true
                            Layout.preferredHeight: visible ? Math.min(140, Math.max(72, contentHeight + 18)) : 0
                            visible: modelData.payment_proof !== undefined
                                     && JSON.stringify(modelData.payment_proof).length > 2
                            editorTitle: walletRoot.tf("browser_wallet_history_payment_proof_signature", "Payment proof")
                            readOnly: true
                            selectByMouse: true
                            persistentSelection: true
                            activeFocusOnPress: true
                            wrapMode: TextEdit.WrapAnywhere
                            color: "#d7e9f4"
                            font.pixelSize: walletRoot.compactTextSize
                            text: modelData.payment_proof !== undefined ? JSON.stringify(modelData.payment_proof, null, 2) : ""
                        }
                    }
                }
            }
        }
    }
}