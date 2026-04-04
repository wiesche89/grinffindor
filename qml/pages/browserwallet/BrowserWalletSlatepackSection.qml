import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: slatepackSection
    property var walletRoot
    property var walletPageSettings
    property bool syncingAmountControls: false
    property real spendableAmountValue: amountStringToValue(grinWalletController.spendableBalance)
    property string amountDialogMode: "send"
    property bool slatepackActionsBlocked: grinWalletController.fullRescanInFlight

    implicitHeight: slatepackCard.implicitHeight

    Component.onCompleted: {
        walletPageSettings.amountDraft = "1"
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

    function normalizeAmountText(amountText) {
        var normalized = amountText ? amountText.toString().trim() : ""
        normalized = normalized.replace(/\s+/g, "")
        if (normalized.indexOf(",") !== -1 && normalized.indexOf(".") === -1)
            normalized = normalized.replace(/,/g, ".")
        return normalized
    }

    function amountStringToValue(amountText) {
        var normalized = normalizeAmountText(amountText)
        if (normalized.length === 0)
            return 0
        var parsed = Number(normalized)
        if (!isFinite(parsed) || parsed < 0)
            return 0
        return parsed
    }

    function formatAmountValue(value) {
        var clamped = Math.max(0, value)
        var fixed = clamped.toFixed(9)
        fixed = fixed.replace(/0+$/, "")
        fixed = fixed.replace(/\.$/, "")
        return fixed.length > 0 ? fixed : "0"
    }

    function syncAmountFieldFromSlider() {
        if (syncingAmountControls)
            return
        syncingAmountControls = true
        walletPageSettings.amountDraft = formatAmountValue(amountSlider.value)
        amountField.text = walletPageSettings.amountDraft
        syncingAmountControls = false
    }

    function syncSliderFromAmountField() {
        if (syncingAmountControls)
            return
        syncingAmountControls = true
        var parsed = amountStringToValue(amountField.text)
        if (amountDialogMode === "send" && parsed > spendableAmountValue)
            parsed = spendableAmountValue
        walletPageSettings.amountDraft = formatAmountValue(parsed)
        amountField.text = walletPageSettings.amountDraft
        if (amountDialogMode === "send")
            amountSlider.value = parsed
        syncingAmountControls = false
    }

    function clearSlatepackWorkspace() {
        grinWalletController.clearWorkflow()
        slatepackArea.text = ""
        decodedBuffer.text = ""
        walletRoot.updateSlatepackStatus("")
    }

    function openAmountDialog(mode) {
        if (slatepackActionsBlocked)
            return
        amountDialogMode = mode
        walletPageSettings.amountDraft = "1"
        amountActionPopup.open()
    }

    function confirmAmountDialog() {
        if (slatepackActionsBlocked)
            return
        syncSliderFromAmountField()
        if (amountDialogMode === "receive")
            grinWalletController.startReceiveWorkflow(amountField.text, "")
        else
            grinWalletController.startSendWorkflow(amountField.text, "")
        walletRoot.syncWorkflowEditors()
        amountActionPopup.close()
    }

    Popup {
        id: amountActionPopup

        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: Math.min(walletRoot.width - 28, 520)
        padding: 0

        onOpened: {
            walletPageSettings.amountDraft = "1"
            if (amountDialogMode === "send")
                slatepackSection.syncSliderFromAmountField()
            amountField.forceActiveFocus()
            walletRoot.syncBrowserShortcutContext(amountField)
        }

        onClosed: walletRoot.syncBrowserShortcutContext(null)

        background: Rectangle {
            radius: 28
            color: "#0f1b26"
            border.color: amountDialogMode === "receive" ? "#3f6d57" : "#2a4f64"
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            Label {
                Layout.fillWidth: true
                text: amountDialogMode === "receive"
                      ? walletRoot.tf("browser_wallet_amount_dialog_receive_title", "Receive Amount")
                      : walletRoot.tf("browser_wallet_amount_dialog_send_title", "Send Amount")
                color: "#ffffff"
                font.pixelSize: 28
                font.weight: Font.Bold
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                text: amountDialogMode === "receive"
                      ? walletRoot.tf("browser_wallet_amount_dialog_receive_note", "Enter the invoice amount to start the receive workflow.")
                      : walletRoot.tf("browser_wallet_amount_dialog_send_note", "Choose how much you want to send. Use Max to fill the spendable balance.")
                color: "#d7e9f4"
                wrapMode: Text.WordWrap
            }

            TextField {
                id: amountField
                Layout.fillWidth: true
                implicitHeight: 42
                text: walletPageSettings.amountDraft
                placeholderText: walletRoot.tf("browser_wallet_amount_placeholder", "Amount")
                selectByMouse: true
                leftPadding: 14
                rightPadding: 14
                validator: DoubleValidator {
                    bottom: 0
                    decimals: 9
                    notation: DoubleValidator.StandardNotation
                }
                onActiveFocusChanged: walletRoot.syncBrowserShortcutContext(amountField)
                onSelectedTextChanged: walletRoot.syncBrowserShortcutContext(amountField)
                onTextChanged: {
                    walletPageSettings.amountDraft = text
                    walletRoot.syncBrowserShortcutContext(amountField)
                }
                onEditingFinished: slatepackSection.syncSliderFromAmountField()
                Keys.onPressed: function(event) { walletRoot.handleTextControlKeyPress(amountField, event) }
            }

            Slider {
                id: amountSlider
                Layout.fillWidth: true
                visible: amountDialogMode === "send"
                from: 0
                to: Math.max(spendableAmountValue, 0)
                value: amountStringToValue(walletPageSettings.amountDraft)
                stepSize: spendableAmountValue > 0 ? Math.max(spendableAmountValue / 1000.0, 0.000000001) : 0
                enabled: visible && spendableAmountValue > 0
                onMoved: slatepackSection.syncAmountFieldFromSlider()
                onValueChanged: {
                    if (pressed)
                        slatepackSection.syncAmountFieldFromSlider()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: amountDialogMode === "send"

                Button {
                    text: walletRoot.tf("browser_wallet_amount_max", "Max")
                    enabled: spendableAmountValue > 0
                    onClicked: {
                        amountSlider.value = spendableAmountValue
                        slatepackSection.syncAmountFieldFromSlider()
                    }
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: walletRoot.tf("browser_wallet_spendable", "Spendable") + ": " + grinWalletController.spendableBalance
                    color: "#7ea0b3"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignRight
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }

                Button {
                    text: walletRoot.tf("browser_wallet_cancel", "Cancel")
                    onClicked: amountActionPopup.close()
                }

                Button {
                    text: amountDialogMode === "receive"
                          ? walletRoot.tf("browser_wallet_receive", "Receive")
                          : walletRoot.tf("browser_wallet_send", "Send")
                    enabled: amountField.text.trim().length > 0
                    onClicked: slatepackSection.confirmAmountDialog()
                }
            }
        }
    }

    BrowserWalletSectionCard {
        id: slatepackCard
        width: parent.width
        contentPadding: 12
        title: walletRoot.tf("browser_wallet_nav_send_receive", "Send / Receive")
        subtitle: walletRoot.tf("browser_wallet_slatepack_note", "Paste or decode a Slatepack, then start a send or receive flow from the same workspace.")

        ColumnLayout {
            id: slatepackColumn
            Layout.fillWidth: true
            spacing: 12

            BrowserWalletPanel {
                Layout.fillWidth: true
                visible: walletRoot.nodeStatusMode() !== "online" || walletRoot.pendingRecoveryCount() > 0
                fillColor: walletRoot.nodeStatusMode() === "offline" ? "#34191d" : "#352816"
                strokeColor: walletRoot.recoveryBannerColor()

                Label {
                    id: slatepackRecoveryLabel
                    Layout.fillWidth: true
                    text: walletRoot.nodeStatusMode() === "online"
                          ? walletRoot.tf("browser_wallet_slatepack_recovery_pending", "Broadcast recovery is still in progress for one or more transactions. You can decode and process Slatepacks, but confirm node status before sending again.")
                          : walletRoot.tf("browser_wallet_slatepack_recovery_offline", "Node connectivity is degraded. Decoding still works, but sending and broadcast recovery should wait for a successful refresh.")
                    color: walletRoot.recoveryBannerColor()
                    wrapMode: Text.WordWrap
                }
            }

            BrowserWalletPanel {
                Layout.fillWidth: true
                visible: slatepackActionsBlocked
                fillColor: "#352816"
                strokeColor: "#ffd280"

                Label {
                    Layout.fillWidth: true
                    text: walletRoot.tf("browser_wallet_slatepack_full_rescan_blocked", "Full rescan is active. Creating or processing Slatepacks is disabled until the scan completes.")
                    color: "#ffd280"
                    wrapMode: Text.WordWrap
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Button {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    text: walletRoot.tf("browser_wallet_send", "Send")
                    enabled: walletRoot.nodeStatusMode() === "online" && !slatepackActionsBlocked
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
                    onClicked: slatepackSection.openAmountDialog("send")
                }

                Button {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    text: walletRoot.tf("browser_wallet_receive", "Receive")
                    enabled: !slatepackActionsBlocked
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
                        color: parent.enabled ? "#67b98d" : "#6c8378"
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: slatepackSection.openAmountDialog("receive")
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
                    enabled: slatepackArea.text.trim().length > 0 && !slatepackActionsBlocked
                    highlighted: false
                    onClicked: {
                        var previousWorkflowId = grinWalletController.workflowId
                        var previousWorkflowState = grinWalletController.workflowState
                        grinWalletController.processWorkflowSlatepack(slatepackArea.text)
                        walletRoot.syncWorkflowEditors()
                        var currentWorkflowState = grinWalletController.workflowState
                        var currentWorkflowId = grinWalletController.workflowId
                        if ((currentWorkflowState === "S3" || currentWorkflowState === "I3")
                                && (currentWorkflowState !== previousWorkflowState
                                    || currentWorkflowId !== previousWorkflowId)) {
                            slatepackSection.clearSlatepackWorkspace()
                        }
                    }
                }

                Button {
                    text: walletRoot.tf("browser_wallet_clear", "Clear")
                    highlighted: false
                    onClicked: slatepackSection.clearSlatepackWorkspace()
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