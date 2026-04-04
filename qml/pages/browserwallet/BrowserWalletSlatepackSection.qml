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
        walletRoot.decodedEditor = decodedBuffer
        walletRoot.syncWorkflowEditors()
    }

    Component.onDestruction: {
        if (walletRoot.slatepackEditor === slatepackArea)
            walletRoot.slatepackEditor = null
        if (walletRoot.decodedEditor === decodedBuffer)
            walletRoot.decodedEditor = null
    }

    QtObject {
        id: decodedBuffer
        property string text: ""
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
                text: walletRoot.tf("browser_wallet_slatepack_note", "Paste or decode a Slatepack, then start a send or receive flow from the same workspace.")
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
                columns: 1
                rowSpacing: 12
                columnSpacing: 12

                Rectangle {
                    Layout.fillWidth: true
                    radius: 18
                    color: "#101b25"
                    border.color: "#223847"
                    implicitHeight: amountCardColumn.implicitHeight + 28

                    Column {
                        id: amountCardColumn
                        width: parent.width - 28
                        anchors.centerIn: parent
                        spacing: 8

                        Label {
                            text: walletRoot.tf("browser_wallet_amount_label", "Amount")
                            color: "#7ea0b3"
                            font.pixelSize: 12
                        }

                        RowLayout {
                            width: parent.width
                            spacing: 10

                            TextField {
                                id: amountField
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                implicitHeight: 40
                                text: walletPageSettings.amountDraft
                                placeholderText: walletRoot.tf("browser_wallet_amount_placeholder", "Amount, e.g. 1.000000000")
                                selectByMouse: true
                                onActiveFocusChanged: walletRoot.syncBrowserShortcutContext(amountField)
                                onSelectedTextChanged: walletRoot.syncBrowserShortcutContext(amountField)
                                onTextChanged: {
                                    walletPageSettings.amountDraft = text
                                    walletRoot.syncBrowserShortcutContext(amountField)
                                }
                                Keys.onPressed: function(event) { walletRoot.handleTextControlKeyPress(amountField, event) }
                            }

                            Rectangle {
                                id: grinUnitBadge
                                Layout.alignment: Qt.AlignVCenter
                                width: 72
                                height: 40
                                radius: 12
                                color: "#162633"
                                border.color: "#294559"

                                Label {
                                    anchors.centerIn: parent
                                    text: "GRIN"
                                    color: "#d8f3ff"
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                }
                            }
                        }
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                rowSpacing: 12
                columnSpacing: 12

                Button {
                    Layout.fillWidth: true
                    text: walletRoot.tf("browser_wallet_nav_send", "Send (S1-S3)")
                    enabled: walletRoot.nodeStatusMode() === "online"
                    leftPadding: 18
                    rightPadding: 18
                    topPadding: 16
                    bottomPadding: 16
                    background: Rectangle {
                        radius: 18
                        color: "#111c26"
                        border.color: parent.enabled ? "#223847" : "#1a2a35"
                    }
                    contentItem: Label {
                        text: parent.text
                        color: parent.enabled ? "#d96a76" : "#7b6669"
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        grinWalletController.startSendWorkflow(amountField.text, "")
                        walletRoot.syncWorkflowEditors()
                    }
                }

                Button {
                    Layout.fillWidth: true
                    text: walletRoot.tf("browser_wallet_nav_receive", "Receive (I1-I3)")
                    leftPadding: 18
                    rightPadding: 18
                    topPadding: 16
                    bottomPadding: 16
                    background: Rectangle {
                        radius: 18
                        color: "#111c26"
                        border.color: "#223847"
                    }
                    contentItem: Label {
                        text: parent.text
                        color: "#67b98d"
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        grinWalletController.startReceiveWorkflow(amountField.text, "")
                        walletRoot.syncWorkflowEditors()
                    }
                }
            }

            Flow {
                Layout.fillWidth: true
                spacing: 10

                Button {
                    text: walletRoot.tf("browser_wallet_paste_slatepack", "Paste Slatepack")
                    highlighted: false
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
                    highlighted: false
                    onClicked: {
                        decodedBuffer.text = grinWalletController.decodeSlatepack(slatepackArea.text)
                        walletRoot.updateSlatepackStatus(decodedBuffer.text)
                    }
                }

                Button {
                    text: walletRoot.tf("browser_wallet_process", "Process")
                    enabled: slatepackArea.text.trim().length > 0
                    highlighted: false
                    onClicked: {
                        grinWalletController.processWorkflowSlatepack(slatepackArea.text)
                        walletRoot.syncWorkflowEditors()
                    }
                }

                Button {
                    text: walletRoot.tf("browser_wallet_clear", "Clear")
                    highlighted: false
                    onClicked: {
                        grinWalletController.clearWorkflow()
                        slatepackArea.text = ""
                        decodedBuffer.text = ""
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

            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: 220
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
        }
    }
}