import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../components" as AppComponents

Item {
    id: slatepackSection
    property var walletRoot
    property var walletPageSettings
    property bool syncingAmountControls: false
    property real spendableAmountValue: amountStringToValue(grinWalletController.spendableBalance)
    property string amountDialogMode: "send"
    property bool slatepackActionsBlocked: grinWalletController.fullRescanInFlight
    property string processReviewMode: "-"
    property string processReviewState: "-"
    property string processReviewWorkflowId: "-"
    property string processReviewAmount: "-"
    property string processReviewNote: "-"
    property string processReviewParticipant: "-"
    property string processReviewKernel: "-"
    property string processReviewMessage: ""

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

    function resetProcessReview() {
        processReviewMode = "-"
        processReviewState = "-"
        processReviewWorkflowId = "-"
        processReviewAmount = "-"
        processReviewNote = "-"
        processReviewParticipant = "-"
        processReviewKernel = "-"
        processReviewMessage = ""
    }

    function decodeSlatepackForReview() {
        decodedBuffer.text = grinWalletController.decodeSlatepack(slatepackArea.text)
        walletRoot.updateSlatepackStatus(decodedBuffer.text)
        return decodedBuffer.text
    }

    function firstReviewValue() {
        for (var index = 0; index < arguments.length; ++index) {
            var candidate = arguments[index]
            if (candidate === undefined || candidate === null)
                continue
            candidate = candidate.toString().trim()
            if (candidate.length > 0)
                return candidate
        }
        return "-"
    }

    function deriveReviewMode(parsed, stateCode) {
        var explicitMode = firstReviewValue(parsed.body_type, parsed.mode)
        if (explicitMode !== "-")
            return explicitMode
        if (stateCode.indexOf("I") === 0)
            return "invoice"
        if (stateCode.indexOf("S") === 0)
            return "send"
        return "-"
    }

    function deriveReviewParticipant(parsed) {
        var proof = parsed.proof || {}
        var senderAddress = firstReviewValue(parsed.sender,
                                             parsed.sender_address,
                                             parsed.senderAddress,
                                             proof.saddr,
                                             proof.sender,
                                             proof.sender_address)
        var receiverAddress = firstReviewValue(parsed.receiver,
                                               parsed.receiver_address,
                                               parsed.receiverAddress,
                                               proof.raddr,
                                               proof.receiver,
                                               proof.receiver_address)

        if (senderAddress !== "-" && receiverAddress !== "-")
            return senderAddress + " -> " + receiverAddress
        return firstReviewValue(senderAddress,
                                receiverAddress,
                                parsed.participant_id,
                                parsed.participant,
                                parsed.address)
    }

    function openProcessReviewDialog() {
        if (slatepackActionsBlocked || slatepackArea.text.trim().length === 0)
            return

        resetProcessReview()
        var decodedText = decodeSlatepackForReview()
        var trimmed = decodedText ? decodedText.trim() : ""
        if (trimmed.length === 0) {
            walletRoot.showErrorDialog(walletRoot.tf("browser_wallet_slatepack_review_empty", "The Slatepack could not be decoded."))
            return
        }

        try {
            var parsed = JSON.parse(trimmed)
            if (parsed.encrypted_slatepack) {
                walletRoot.showErrorDialog(parsed.note || walletRoot.tf("browser_wallet_slatepack_status_encrypted", "Encrypted Slatepack detected. Unlock the matching wallet to decrypt it."))
                return
            }
            if (parsed.external_slatepack) {
                walletRoot.showErrorDialog(parsed.note || walletRoot.tf("browser_wallet_slatepack_status_invalid", "Slatepack payload could not be parsed."))
                return
            }

            processReviewState = firstReviewValue(parsed.state, parsed.sta)
            processReviewMode = deriveReviewMode(parsed, processReviewState)
            processReviewWorkflowId = firstReviewValue(parsed.id, parsed.workflow_id)
            processReviewAmount = firstReviewValue(parsed.amount,
                                                   parsed.amount_display,
                                                   parsed.invoice_amount,
                                                   parsed.receiver_amount_display,
                                                   parsed.amt)
            processReviewNote = firstReviewValue(parsed.note,
                                                 parsed.message,
                                                 parsed.memo,
                                                 parsed.msg)
            processReviewParticipant = deriveReviewParticipant(parsed)
            processReviewKernel = firstReviewValue(parsed.kernel_commitment,
                                                   parsed.kernel_excess,
                                                   parsed.excess,
                                                   parsed.kern)
            processReviewMessage = walletRoot.tf("browser_wallet_slatepack_review_ready", "Confirm the decoded Slatepack details before processing.")
            processReviewPopup.open()
            return
        } catch (error) {
            processReviewMessage = walletRoot.tf("browser_wallet_slatepack_review_text_only", "The Slatepack was decoded, but only as plain text. Review the decoded output before processing.")
            processReviewNote = trimmed
            processReviewPopup.open()
        }
    }

    function processValidatedSlatepack() {
        var previousWorkflowId = grinWalletController.workflowId
        var previousWorkflowState = grinWalletController.workflowState
        grinWalletController.processWorkflowSlatepack(slatepackArea.text)
        walletRoot.syncWorkflowEditors()
        processReviewPopup.close()
        var currentWorkflowState = grinWalletController.workflowState
        var currentWorkflowId = grinWalletController.workflowId
        if ((currentWorkflowState === "S3" || currentWorkflowState === "I3")
                && (currentWorkflowState !== previousWorkflowState
                    || currentWorkflowId !== previousWorkflowId)) {
            slatepackSection.clearSlatepackWorkspace()
        }
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
            PlatformBridge.requestFocus(amountField.bridgeId)
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
                font.pixelSize: walletRoot.veryPhoneMode ? 20 : (walletRoot.phoneMode ? 24 : 28)
                font.weight: Font.Bold
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                text: amountDialogMode === "receive"
                      ? walletRoot.tf("browser_wallet_amount_dialog_receive_note", "Enter the invoice amount to start the receive workflow.")
                      : walletRoot.tf("browser_wallet_amount_dialog_send_note", "Choose how much you want to send. Use Max to fill the spendable balance.")
                color: "#d7e9f4"
                    font.pixelSize: walletRoot.bodyTextSize
                wrapMode: Text.WordWrap
            }

            AppComponents.AppTextField {
                id: amountField
                Layout.fillWidth: true
                implicitHeight: 42
                editorTitle: amountDialogMode === "receive"
                             ? walletRoot.tf("browser_wallet_amount_dialog_receive_title", "Receive Amount")
                             : walletRoot.tf("browser_wallet_amount_dialog_send_title", "Send Amount")
                inputMode: "decimal"
                font.pixelSize: walletRoot.controlTextSize
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
                onTextChanged: {
                    walletPageSettings.amountDraft = text
                }
                onEditingFinished: slatepackSection.syncSliderFromAmountField()
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
                    font.pixelSize: walletRoot.controlTextSize
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
                    font.pixelSize: walletRoot.compactTextSize
                    horizontalAlignment: Text.AlignRight
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }

                Button {
                    text: walletRoot.tf("browser_wallet_cancel", "Cancel")
                    font.pixelSize: walletRoot.controlTextSize
                    onClicked: amountActionPopup.close()
                }

                Button {
                    text: amountDialogMode === "receive"
                          ? walletRoot.tf("browser_wallet_receive", "Receive")
                          : walletRoot.tf("browser_wallet_send", "Send")
                    font.pixelSize: walletRoot.controlTextSize
                    enabled: amountField.text.trim().length > 0
                    onClicked: slatepackSection.confirmAmountDialog()
                }
            }
        }
    }

    Popup {
        id: processReviewPopup

        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: Math.min(walletRoot.width - 28, 720)
        padding: 0

        background: Rectangle {
            radius: 28
            color: "#0f1b26"
            border.color: "#2a4f64"
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            Label {
                Layout.fillWidth: true
                text: walletRoot.tf("browser_wallet_slatepack_review_title", "Validate Slatepack")
                color: "#ffffff"
                font.pixelSize: walletRoot.phoneMode ? 24 : 28
                font.weight: Font.Bold
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                text: processReviewMessage
                color: "#d7e9f4"
                font.pixelSize: walletRoot.bodyTextSize
                wrapMode: Text.WordWrap
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 12
                rowSpacing: 8

                Label { text: walletRoot.tf("browser_wallet_slatepack_review_mode", "Mode"); color: "#8fb4c9"; font.pixelSize: walletRoot.compactTextSize }
                Label { text: processReviewMode; color: "#ffffff"; font.pixelSize: walletRoot.compactTextSize; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }

                Label { text: walletRoot.tf("browser_wallet_slatepack_review_state", "State"); color: "#8fb4c9"; font.pixelSize: walletRoot.compactTextSize }
                Label { text: processReviewState; color: "#ffffff"; font.pixelSize: walletRoot.compactTextSize; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }

                Label { text: walletRoot.tf("browser_wallet_slatepack_review_workflow", "Workflow ID"); color: "#8fb4c9"; font.pixelSize: walletRoot.compactTextSize }
                Label { text: processReviewWorkflowId; color: "#ffffff"; font.pixelSize: walletRoot.compactTextSize; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }

                Label { text: walletRoot.tf("browser_wallet_slatepack_review_amount", "Amount"); color: "#8fb4c9"; font.pixelSize: walletRoot.compactTextSize }
                Label { text: processReviewAmount; color: "#ffffff"; font.pixelSize: walletRoot.compactTextSize; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }

                Label { text: walletRoot.tf("browser_wallet_slatepack_review_participant", "Participant"); color: "#8fb4c9"; font.pixelSize: walletRoot.compactTextSize }
                Label { text: processReviewParticipant; color: "#ffffff"; font.pixelSize: walletRoot.compactTextSize; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }

                Label { text: walletRoot.tf("browser_wallet_slatepack_review_kernel", "Kernel"); color: "#8fb4c9"; font.pixelSize: walletRoot.compactTextSize }
                Label { text: processReviewKernel; color: "#ffffff"; font.pixelSize: walletRoot.compactTextSize; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
            }

            Rectangle {
                Layout.fillWidth: true
                visible: processReviewNote !== "-" && processReviewNote.length > 0
                radius: 16
                color: "#122231"
                border.color: "#2a4f64"
                implicitHeight: processReviewNoteLabel.implicitHeight + 20

                Label {
                    id: processReviewNoteLabel
                    anchors.fill: parent
                    anchors.margins: 10
                    text: walletRoot.tf("browser_wallet_slatepack_review_note", "Note") + ": " + processReviewNote
                    color: "#d7e9f4"
                    font.pixelSize: walletRoot.compactTextSize
                    wrapMode: Text.WrapAnywhere
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }

                Button {
                    text: walletRoot.tf("browser_wallet_cancel", "Cancel")
                    font.pixelSize: walletRoot.controlTextSize
                    onClicked: processReviewPopup.close()
                }

                Button {
                    text: walletRoot.tf("browser_wallet_process", "Process")
                    font.pixelSize: walletRoot.controlTextSize
                    onClicked: slatepackSection.processValidatedSlatepack()
                }
            }
        }
    }

    BrowserWalletSectionCard {
        id: slatepackCard
        width: parent.width
        contentPadding: 12
        title: walletRoot.tf("browser_wallet_nav_send_receive", "Send / Receive")
        subtitle: walletRoot.tf("browser_wallet_slatepack_note", "Review a Slatepack in the shared editor, then start a send or receive flow from the same workspace.")

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
                          ? walletRoot.tf("browser_wallet_slatepack_recovery_pending", "Broadcast recovery is still in progress for one or more transactions. You can validate and process Slatepacks, but confirm node status before sending again.")
                          : walletRoot.tf("browser_wallet_slatepack_recovery_offline", "Node connectivity is degraded. Slatepack review still works, but sending and broadcast recovery should wait for a successful refresh.")
                    color: walletRoot.recoveryBannerColor()
                    font.pixelSize: walletRoot.bodyTextSize
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
                    font.pixelSize: walletRoot.bodyTextSize
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
                    font.pixelSize: walletRoot.controlTextSize
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
                        font.pixelSize: walletRoot.controlTextSize
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
                    font.pixelSize: walletRoot.controlTextSize
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
                        font.pixelSize: walletRoot.controlTextSize
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
                    text: walletRoot.tf("browser_wallet_validate_process", "Validate and Process")
                    font.pixelSize: walletRoot.controlTextSize
                    enabled: slatepackArea.text.trim().length > 0 && !slatepackActionsBlocked
                    highlighted: false
                    onClicked: slatepackSection.openProcessReviewDialog()
                }

                Button {
                    text: walletRoot.tf("browser_wallet_clear", "Clear")
                    font.pixelSize: walletRoot.controlTextSize
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
                    font.pixelSize: walletRoot.bodyTextSize
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
                    font.pixelSize: walletRoot.bodyTextSize
                wrapMode: Text.WordWrap
            }

            ScrollView {
                id: slatepackScroll
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                clip: true
                contentWidth: availableWidth

                AppComponents.AppTextArea {
                    id: slatepackArea
                    objectName: "slatepackArea"
                    width: slatepackScroll.availableWidth
                    editorTitle: walletRoot.tf("browser_wallet_paste_slatepack", "Paste Slatepack")
                    wrapMode: TextEdit.WrapAnywhere
                    textFormat: TextEdit.PlainText
                    color: "#e2f4ff"
                    selectionColor: "#2ad4ff"
                    selectedTextColor: "#08131c"
                    font.pixelSize: walletRoot.controlTextSize
                    selectByMouse: true
                    persistentSelection: true
                    activeFocusOnPress: true
                    text: ""
                    placeholderText: "BEGINSLATEPACK. ..."
                }
            }
        }
    }
}