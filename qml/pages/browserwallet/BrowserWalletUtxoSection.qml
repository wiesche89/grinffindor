import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../components" as AppComponents

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
            spacing: 8

            // Empty state
            Item {
                Layout.fillWidth: true
                visible: grinWalletController.walletOutputs.length === 0
                implicitHeight: emptyLabel.implicitHeight + 20

                Label {
                    id: emptyLabel
                    anchors.centerIn: parent
                    width: parent.width
                    text: walletRoot.tf("browser_wallet_utxo_empty", "No wallet outputs are tracked yet. Run a scan or complete a transaction first.")
                    color: "#2a5060"
                    font.pixelSize: walletRoot.bodyTextSize
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            Repeater {
                model: grinWalletController.walletOutputs

                Item {
                    id: utxoItem
                    property bool detailsExpanded: false
                    Layout.fillWidth: true
                    implicitHeight: utxoRect.implicitHeight

                    Rectangle {
                        id: utxoRect
                        anchors.left: parent.left
                        anchors.right: parent.right
                        clip: true
                        radius: 13
                        color: "#091825"
                        border.color: "#132e42"
                        border.width: 1
                        implicitHeight: utxoCol.implicitHeight + utxoCol.anchors.topMargin + utxoCol.anchors.bottomMargin

                        ColumnLayout {
                            id: utxoCol
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.margins: 18
                            anchors.leftMargin: 26
                            anchors.topMargin: 26
                            anchors.bottomMargin: 32
                            spacing: 8

                            // Amount + status
                            RowLayout {
                                id: utxoHeaderRow
                                Layout.fillWidth: true
                                spacing: 12

                                Label {
                                    Layout.alignment: Qt.AlignVCenter
                                    text: walletRoot.tf("browser_wallet_utxo_status", "Status") + ": " + (modelData.status || "-")
                                    color: modelData.status === "spendable" ? "#FEF102"
                                         : modelData.status === "spent" ? "#dd4050"
                                         : "#e0a840"
                                    font.pixelSize: walletRoot.compactTextSize
                                }

                                Item { Layout.fillWidth: true }

                                Label {
                                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                    text: (modelData.amount || "0.000000000") + " GRIN"
                                    color: "#b8d8f0"
                                    font.pixelSize: walletRoot.bodyTextSize
                                    font.weight: Font.DemiBold
                                    horizontalAlignment: Text.AlignRight
                                    wrapMode: Text.NoWrap
                                    elide: Text.ElideLeft
                                }
                            }

                            // Source + confirmations
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 14

                                Label {
                                    text: walletRoot.tf("browser_wallet_utxo_source", "Source") + ": " + (modelData.source || "-")
                                    color: "#3a6a84"
                                    font.pixelSize: walletRoot.compactTextSize
                                }

                                Label {
                                    text: walletRoot.tf("browser_wallet_history_confirmations", "Confs") + ": "
                                          + (modelData.confirmations !== undefined ? modelData.confirmations : 0)
                                    color: "#3a6a84"
                                    font.pixelSize: walletRoot.compactTextSize
                                }
                            }

                            // Details toggle
                            Button {
                                text: utxoItem.detailsExpanded
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
                                onClicked: utxoItem.detailsExpanded = !utxoItem.detailsExpanded
                            }

                            // Expanded details
                            ColumnLayout {
                                Layout.fillWidth: true
                                visible: utxoItem.detailsExpanded
                                spacing: 6

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: "#0e2535"
                                }

                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: width < 820 ? 1 : 2
                                    rowSpacing: 5
                                    columnSpacing: 14

                                    Repeater {
                                        model: [
                                            { label: walletRoot.tf("browser_wallet_utxo_height",   "Height"),   value: (modelData.height || "-"),  visible: true },
                                            { label: walletRoot.tf("browser_wallet_utxo_coinbase", "Coinbase"), value: modelData.coinbase ? walletRoot.tf("browser_wallet_utxo_yes","yes") : walletRoot.tf("browser_wallet_utxo_no","no"), visible: true },
                                            { label: walletRoot.tf("browser_wallet_utxo_onchain",  "On chain"), value: modelData.on_chain  ? walletRoot.tf("browser_wallet_utxo_yes","yes") : walletRoot.tf("browser_wallet_utxo_no","no"), visible: true }
                                        ]

                                        Row {
                                            spacing: 5
                                            Layout.fillWidth: true
                                            Label { text: modelData.label + ":"; color: "#2a5060"; font.pixelSize: walletRoot.bodyTextSize }
                                            Label { text: modelData.value;      color: "#5a8eaa"; font.pixelSize: walletRoot.bodyTextSize }
                                        }
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    visible: (modelData.lock_workflow_id || "").length > 0
                                    text: walletRoot.tf("browser_wallet_utxo_locked_by", "Locked by") + ": "
                                          + (modelData.lock_workflow_mode || "-") + " / "
                                          + (modelData.lock_workflow_state || "-") + " / "
                                          + (modelData.lock_workflow_id || "-")
                                    color: "#e0a840"
                                    font.pixelSize: walletRoot.bodyTextSize
                                    wrapMode: Text.WrapAnywhere
                                }

                                AppComponents.CopyableLabel {
                                    Layout.fillWidth: true
                                    text: walletRoot.tf("browser_wallet_utxo_commitment", "Commitment") + ": " + (modelData.commitment || "-")
                                    copiedValue: modelData.commitment || ""
                                    copyEnabled: (modelData.commitment || "").length > 0
                                    color: "#204050"
                                    wrapMode: Text.WrapAnywhere
                                    font.pixelSize: walletRoot.compactTextSize
                                }

                                Label {
                                    Layout.fillWidth: true
                                    visible: (modelData.key_path || "").length > 0
                                    text: walletRoot.tf("browser_wallet_utxo_keypath", "Key path") + ": " + (modelData.key_path || "")
                                    color: "#204050"
                                    wrapMode: Text.WrapAnywhere
                                    font.pixelSize: walletRoot.compactTextSize
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
