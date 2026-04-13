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
        spacing: 8

        Repeater {
            model: grinWalletController.transactionHistory

            Item {
                id: txItem
                property bool detailsExpanded: false
                Layout.fillWidth: true
                visible: modelData.workflow_id !== undefined
                implicitHeight: txRect.implicitHeight

                Rectangle {
                    id: txRect
                    anchors.left: parent.left
                    anchors.right: parent.right
                    radius: 13
                    clip: true
                    color: "#091825"
                    border.color: "#132e42"
                    border.width: 1
                    implicitHeight: txCol.implicitHeight + txCol.anchors.topMargin + txCol.anchors.bottomMargin

                    ColumnLayout {
                        id: txCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 18
                        anchors.leftMargin: 26
                        anchors.topMargin: 26
                        anchors.bottomMargin: 16
                        spacing: 8

                        // Row 1: direction + status / amount
                        RowLayout {
                            id: headerRow
                            Layout.fillWidth: true
                            spacing: 12
                            layoutDirection: Qt.LeftToRight

                            RowLayout {
                                id: leftMetaRow
                                Layout.alignment: Qt.AlignVCenter
                                spacing: 8

                                Label {
                                    Layout.alignment: Qt.AlignVCenter
                                    text: (modelData.mode || "-").toUpperCase()
                                    color: "#5a8eaa"
                                    font.pixelSize: walletRoot.compactTextSize
                                    font.weight: Font.DemiBold
                                }

                                Label {
                                    Layout.alignment: Qt.AlignVCenter
                                    text: "/" + (modelData.state || "-")
                                    color: "#3a6a84"
                                    font.pixelSize: walletRoot.compactTextSize
                                }
                            }

                            Item { Layout.fillWidth: true }

                            Label {
                                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                text: (modelData.amount || "0") + " GRIN"
                                color: "#b8d8f0"
                                font.pixelSize: walletRoot.bodyTextSize
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignRight
                                wrapMode: Text.NoWrap
                                elide: Text.ElideLeft
                            }
                        }

                        // Row 2: fee + confirmations
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Label {
                                text: walletRoot.tf("browser_wallet_statement_fee", "Fee") + ": " + (modelData.fee || "0")
                                color: "#3a6a84"
                                font.pixelSize: walletRoot.compactTextSize
                            }

                            Label {
                                text: walletRoot.tf("browser_wallet_history_confirmations", "Confs") + ": "
                                      + (modelData.confirmations !== undefined ? modelData.confirmations : 0)
                                color: "#3a6a84"
                                font.pixelSize: walletRoot.compactTextSize
                            }

                            Item { Layout.fillWidth: true }
                        }

                        // Row 3: workflow id (muted)
                        Label {
                            Layout.fillWidth: true
                            text: modelData.workflow_id || ""
                            color: "#254055"
                            wrapMode: Text.WrapAnywhere
                            font.pixelSize: walletRoot.compactTextSize - 1
                        }

                        // Row 4: actions
                        Flow {
                            Layout.fillWidth: true
                            spacing: 6

                            Button {
                                text: txItem.detailsExpanded
                                      ? walletRoot.tf("browser_wallet_hide_details", "Hide Details")
                                      : walletRoot.tf("browser_wallet_show_details", "Details")
                                font.pixelSize: walletRoot.compactTextSize
                                background: Rectangle {
                                    radius: 8; color: parent.down ? "#07111c" : (parent.hovered ? "#060e18" : "transparent")
                                    border.color: parent.hovered ? "#1e3a52" : "#152a3c"; border.width: 1
                                }
                                contentItem: Label {
                                    text: parent.text; font: parent.font; color: parent.hovered ? "#88b8d8" : "#4a7898"
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: txItem.detailsExpanded = !txItem.detailsExpanded
                            }

                            Button {
                                text: modelData.status === "broadcast_failed"
                                      ? walletRoot.tf("browser_wallet_history_retry_broadcast", "Retry Broadcast")
                                      : walletRoot.tf("browser_wallet_history_broadcast", "Broadcast")
                                font.pixelSize: walletRoot.compactTextSize
                                visible: walletRoot.canBroadcastEntry(modelData)
                                background: Rectangle {
                                    radius: 8
                                    color: parent.down ? "#201800" : (parent.hovered ? "#1a1500" : "#181200")
                                    border.color: parent.down ? "#A08800" : (parent.hovered ? "#C09800" : "#806800")
                                    border.width: 1
                                }
                                contentItem: Label {
                                    text: parent.text; font: parent.font; color: parent.down ? "#FEF102" : (parent.hovered ? "#FEF102" : "#FEF102")
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: grinWalletController.broadcastTransaction(modelData.workflow_id)
                            }

                            Button {
                                text: walletRoot.tf("browser_wallet_history_cancel", "Cancel")
                                font.pixelSize: walletRoot.compactTextSize
                                visible: walletRoot.canCancelEntry(modelData)
                                background: Rectangle {
                                    radius: 8; color: parent.down ? "#1c0c10" : (parent.hovered ? "#160a0e" : "transparent")
                                    border.color: parent.hovered ? "#882838" : "#4a1820"; border.width: 1
                                }
                                contentItem: Label {
                                    text: parent.text; font: parent.font; color: parent.down ? "#ff5868" : (parent.hovered ? "#ee4a5a" : "#882030")
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: grinWalletController.cancelTransaction(modelData.workflow_id)
                            }
                        }

                        // Expandable detail section
                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: txItem.detailsExpanded
                            spacing: 6

                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: "#0e2535"
                            }

                            Label {
                                Layout.fillWidth: true
                                text: walletRoot.tf("browser_wallet_history_confirmed_height", "Confirmed Height") + ": "
                                      + (modelData.confirmed_height !== undefined && modelData.confirmed_height !== ""
                                         ? modelData.confirmed_height : "-")
                                color: "#5a8eaa"
                                font.pixelSize: walletRoot.bodyTextSize
                                wrapMode: Text.WordWrap
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: modelData.payment_proof_status !== undefined
                                text: walletRoot.tf("browser_wallet_history_payment_proof", "Payment Proof") + ": "
                                      + (modelData.payment_proof_status || "-")
                                color: modelData.payment_proof_status === "verified" ? "#FEF102"
                                     : modelData.payment_proof_status === "invalid"  ? "#dd4050"
                                     : "#e0a840"
                                font.pixelSize: walletRoot.bodyTextSize
                                wrapMode: Text.WordWrap
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
                                visible: (modelData.broadcast_error || "").length > 0
                                text: walletRoot.tf("browser_wallet_history_broadcast_error", "Broadcast error") + ": " + (modelData.broadcast_error || "")
                                color: "#dd6070"
                                font.pixelSize: walletRoot.bodyTextSize
                                wrapMode: Text.WordWrap
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: (modelData.kernel_excess || "").length > 0
                                text: walletRoot.tf("browser_wallet_history_kernel", "Kernel") + ": " + (modelData.kernel_excess || "")
                                color: "#2e5060"
                                wrapMode: Text.WrapAnywhere
                                font.pixelSize: walletRoot.compactTextSize
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: modelData.output_commitments !== undefined
                                         && modelData.output_commitments.length !== undefined
                                         && modelData.output_commitments.length > 0
                                text: walletRoot.tf("browser_wallet_history_outputs", "Outputs") + ": "
                                      + ((modelData.output_commitments && modelData.output_commitments.join)
                                            ? modelData.output_commitments.join(", ") : "")
                                color: "#2e5060"
                                wrapMode: Text.WrapAnywhere
                                font.pixelSize: walletRoot.compactTextSize
                            }

                            AppComponents.AppTextArea {
                                Layout.fillWidth: true
                                Layout.preferredHeight: visible ? Math.min(120, Math.max(60, contentHeight + 16)) : 0
                                visible: modelData.payment_proof !== undefined
                                         && JSON.stringify(modelData.payment_proof).length > 2
                                editorTitle: walletRoot.tf("browser_wallet_history_payment_proof_signature", "Payment proof")
                                readOnly: true
                                selectByMouse: true
                                persistentSelection: true
                                activeFocusOnPress: true
                                wrapMode: TextEdit.WrapAnywhere
                                color: "#5a8eaa"
                                font.pixelSize: walletRoot.compactTextSize
                                text: modelData.payment_proof !== undefined
                                      ? JSON.stringify(modelData.payment_proof, null, 2) : ""
                            }
                        }
                    }
                }
            }
        }

        // Empty state
        Item {
            Layout.fillWidth: true
            visible: grinWalletController.transactionHistory.length === 0
            implicitHeight: emptyLabel.implicitHeight + 20

            Label {
                id: emptyLabel
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: walletRoot.tf("browser_wallet_history_empty", "No transactions recorded yet.")
                color: "#2a5060"
                font.pixelSize: walletRoot.bodyTextSize
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }
    }
}
