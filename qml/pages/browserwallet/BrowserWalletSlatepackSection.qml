import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: slatepackSection
    property var walletRoot
    property var walletPageSettings

    implicitHeight: slatepackCard.implicitHeight

    Component.onCompleted: {
        walletRoot.slatepackEditor = slatepackArea
        walletRoot.decodedEditor = decodedArea
        walletRoot.syncWorkflowEditors()
    }

    Component.onDestruction: {
        if (walletRoot.slatepackEditor === slatepackArea)
            walletRoot.slatepackEditor = null
        if (walletRoot.decodedEditor === decodedArea)
            walletRoot.decodedEditor = null
    }

    Rectangle {
        id: slatepackCard
        width: parent.width
        radius: 26
        color: "#0f1b26"
        border.color: "#26465b"
        implicitHeight: slatepackColumn.implicitHeight + 34

        ColumnLayout {
            id: slatepackColumn
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            Label {
                text: walletRoot.tf("browser_wallet_nav_slatepack", "Slatepack")
                color: "#ffffff"
                font.pixelSize: 28
                font.weight: Font.Bold
            }

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: walletRoot.tf("browser_wallet_slatepack_note", "Handle all Slatepack flows here. Start SEND at S1, RECEIVE at I1, and process incoming replies or invoices in the same workspace.")
                color: "#cbdbe4"
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 16
                visible: walletRoot.nodeStatusMode() !== "online" || walletRoot.pendingRecoveryCount() > 0
                color: walletRoot.nodeStatusMode() === "offline" ? "#34191d" : "#352816"
                border.color: walletRoot.recoveryBannerColor()
                implicitHeight: slatepackRecoveryLabel.implicitHeight + 20

                Label {
                    id: slatepackRecoveryLabel
                    anchors.fill: parent
                    anchors.margins: 10
                    text: walletRoot.nodeStatusMode() === "online"
                          ? walletRoot.tf("browser_wallet_slatepack_recovery_pending", "Broadcast recovery is still in progress for one or more transactions. You can decode and process Slatepacks, but confirm node status before sending again.")
                          : walletRoot.tf("browser_wallet_slatepack_recovery_offline", "Node connectivity is degraded. Decoding still works, but sending and broadcast recovery should wait for a successful refresh.")
                    color: walletRoot.recoveryBannerColor()
                    wrapMode: Text.WordWrap
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: width < 760 ? 1 : 2
                rowSpacing: 12
                columnSpacing: 12

                TextField {
                    id: amountField
                    Layout.fillWidth: true
                    text: walletPageSettings.amountDraft
                    placeholderText: walletRoot.tf("browser_wallet_amount_placeholder", "Amount in GRIN, e.g. 1.000000000")
                    onActiveFocusChanged: walletRoot.syncBrowserShortcutContext(amountField)
                    onSelectedTextChanged: walletRoot.syncBrowserShortcutContext(amountField)
                    onTextChanged: {
                        walletPageSettings.amountDraft = text
                        walletRoot.syncBrowserShortcutContext(amountField)
                    }
                    Keys.onPressed: function(event) { walletRoot.handleTextControlKeyPress(amountField, event) }
                }

                TextField {
                    id: noteField
                    Layout.fillWidth: true
                    text: walletPageSettings.noteDraft
                    placeholderText: walletRoot.tf("browser_wallet_note_placeholder", "Optional note")
                    onActiveFocusChanged: walletRoot.syncBrowserShortcutContext(noteField)
                    onSelectedTextChanged: walletRoot.syncBrowserShortcutContext(noteField)
                    onTextChanged: {
                        walletPageSettings.noteDraft = text
                        walletRoot.syncBrowserShortcutContext(noteField)
                    }
                    Keys.onPressed: function(event) { walletRoot.handleTextControlKeyPress(noteField, event) }
                }
            }

            RowLayout {
                Layout.fillWidth: true

                Button {
                    text: walletRoot.tf("browser_wallet_paste_slatepack", "Paste Slatepack")
                    onClicked: {
                        walletRoot.openPasteDialog(
                            slatepackArea,
                            walletRoot.tf("browser_wallet_paste_slatepack", "Paste Slatepack"),
                            "BEGINSLATEPACK. ...")
                    }
                }

                Button {
                    text: walletRoot.tf("browser_wallet_decode", "Decode")
                    enabled: slatepackArea.text.trim().length > 0
                    onClicked: {
                        decodedArea.text = grinWalletController.decodeSlatepack(slatepackArea.text)
                        walletRoot.updateSlatepackStatus(decodedArea.text)
                    }
                }

                Button {
                    text: walletRoot.tf("browser_wallet_nav_send", "Send (S1-S3)")
                    enabled: walletRoot.nodeStatusMode() === "online"
                    onClicked: {
                        grinWalletController.startSendWorkflow(amountField.text, noteField.text)
                        walletRoot.syncWorkflowEditors()
                    }
                }

                Button {
                    text: walletRoot.tf("browser_wallet_nav_receive", "Receive (I1-I3)")
                    onClicked: {
                        grinWalletController.startReceiveWorkflow(amountField.text, noteField.text)
                        walletRoot.syncWorkflowEditors()
                    }
                }

                Button {
                    text: walletRoot.tf("browser_wallet_process", "Process")
                    enabled: slatepackArea.text.trim().length > 0
                    onClicked: {
                        grinWalletController.processWorkflowSlatepack(slatepackArea.text)
                        walletRoot.syncWorkflowEditors()
                    }
                }

                Button {
                    text: walletRoot.tf("browser_wallet_clear", "Clear")
                    onClicked: {
                        grinWalletController.clearWorkflow()
                        slatepackArea.text = ""
                        decodedArea.text = ""
                        walletRoot.updateSlatepackStatus("")
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 16
                color: "#122231"
                border.color: walletRoot.slatepackStatusColor
                implicitHeight: slatepackStatusLabel.implicitHeight + 20

                Label {
                    id: slatepackStatusLabel
                    anchors.fill: parent
                    anchors.margins: 10
                    text: walletRoot.slatepackStatusText
                    color: walletRoot.slatepackStatusColor
                    wrapMode: Text.WordWrap
                }
            }

            Label {
                Layout.fillWidth: true
                text: walletRoot.tf("browser_wallet_workflow_status", "Workflow") + ": "
                      + (grinWalletController.workflowMode.length > 0 ? grinWalletController.workflowMode : "-")
                      + " / "
                      + (grinWalletController.workflowState.length > 0 ? grinWalletController.workflowState : "-")
                      + " / "
                      + (grinWalletController.workflowId.length > 0 ? grinWalletController.workflowId : "-")
                color: "#8ff0c8"
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                visible: decodedArea.text.trim().length > 0
                text: {
                    try {
                        var parsed = JSON.parse(decodedArea.text)
                        if (parsed.payment_proof_status)
                            return walletRoot.tf("browser_wallet_history_payment_proof", "Payment Proof") + ": " + parsed.payment_proof_status
                        if (parsed.proof) {
                            return walletRoot.tf("browser_wallet_history_payment_proof", "Payment Proof") + ": "
                                 + (parsed.proof.rsig ? "receiver_signed" : "pending")
                        }
                    } catch (error) {
                    }
                    return ""
                }
                color: {
                    try {
                        var parsed = JSON.parse(decodedArea.text)
                        if (parsed.payment_proof_status === "verified")
                            return "#8ff0c8"
                        if (parsed.payment_proof_status === "invalid")
                            return "#ffb4b4"
                        if (parsed.proof)
                            return "#ffd280"
                    } catch (error) {
                    }
                    return "#8fb4c9"
                }
                wrapMode: Text.WordWrap
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: 320
                clip: true

                TextArea {
                    id: slatepackArea
                    objectName: "slatepackArea"
                    width: parent.width
                    wrapMode: TextEdit.WrapAnywhere
                    textFormat: TextEdit.PlainText
                    color: "#e2f4ff"
                    selectionColor: "#2ad4ff"
                    selectedTextColor: "#08131c"
                    selectByMouse: true
                    persistentSelection: true
                    activeFocusOnPress: true
                    text: ""
                    placeholderText: "BEGINSLATEPACK. ..."
                    onActiveFocusChanged: walletRoot.syncBrowserShortcutContext(slatepackArea)
                    onSelectedTextChanged: walletRoot.syncBrowserShortcutContext(slatepackArea)
                    onTextChanged: walletRoot.syncBrowserShortcutContext(slatepackArea)
                    Keys.onPressed: function(event) { walletRoot.handleTextControlKeyPress(slatepackArea, event) }
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: 240
                clip: true

                TextArea {
                    id: decodedArea
                    width: parent.width
                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.PlainText
                    color: "#e2f4ff"
                    selectionColor: "#2ad4ff"
                    selectedTextColor: "#08131c"
                    readOnly: true
                    selectByMouse: true
                    persistentSelection: true
                    activeFocusOnPress: true
                    text: ""
                    placeholderText: walletRoot.tf("browser_wallet_slatepack_preview", "Decoded Slatepack preview")
                    onActiveFocusChanged: walletRoot.syncBrowserShortcutContext(decodedArea)
                    onSelectedTextChanged: walletRoot.syncBrowserShortcutContext(decodedArea)
                    onTextChanged: walletRoot.syncBrowserShortcutContext(decodedArea)
                    Keys.onPressed: function(event) { walletRoot.handleTextControlKeyPress(decodedArea, event) }
                }
            }
        }
    }
}