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
        if (normalized.length === 0) return 0
        var parsed = Number(normalized)
        if (!isFinite(parsed) || parsed < 0) return 0
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
        if (syncingAmountControls) return
        syncingAmountControls = true
        walletPageSettings.amountDraft = formatAmountValue(amountSlider.value)
        amountField.text = walletPageSettings.amountDraft
        syncingAmountControls = false
    }

    function syncSliderFromAmountField() {
        if (syncingAmountControls) return
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
        if (slatepackActionsBlocked) return
        amountDialogMode = mode
        walletPageSettings.amountDraft = "1"
        amountActionPopup.open()
    }

    function confirmAmountDialog() {
        if (slatepackActionsBlocked) return
        syncSliderFromAmountField()
        if (amountDialogMode === "receive")
            grinWalletController.startReceiveWorkflow(amountField.text, "")
        else
            grinWalletController.startSendWorkflow(amountField.text, "")
        walletRoot.syncWorkflowEditors()
        amountActionPopup.close()
    }

    function resetProcessReview() {
        processReviewMode = "-"; processReviewState = "-"; processReviewWorkflowId = "-"
        processReviewAmount = "-"; processReviewNote = "-"; processReviewParticipant = "-"
        processReviewKernel = "-"; processReviewMessage = ""
    }

    function decodeSlatepackForReview() {
        decodedBuffer.text = grinWalletController.decodeSlatepack(slatepackArea.text)
        walletRoot.updateSlatepackStatus(decodedBuffer.text)
        return decodedBuffer.text
    }

    function firstReviewValue() {
        for (var index = 0; index < arguments.length; ++index) {
            var candidate = arguments[index]
            if (candidate === undefined || candidate === null) continue
            candidate = candidate.toString().trim()
            if (candidate.length > 0) return candidate
        }
        return "-"
    }

    function deriveReviewMode(parsed, stateCode) {
        var explicitMode = firstReviewValue(parsed.body_type, parsed.mode)
        if (explicitMode !== "-") return explicitMode
        if (stateCode.indexOf("I") === 0) return "invoice"
        if (stateCode.indexOf("S") === 0) return "send"
        return "-"
    }

    function deriveReviewParticipant(parsed) {
        var proof = parsed.proof || {}
        var senderAddress = firstReviewValue(parsed.sender, parsed.sender_address, parsed.senderAddress,
                                             proof.saddr, proof.sender, proof.sender_address)
        var receiverAddress = firstReviewValue(parsed.receiver, parsed.receiver_address, parsed.receiverAddress,
                                               proof.raddr, proof.receiver, proof.receiver_address)
        if (senderAddress !== "-" && receiverAddress !== "-")
            return senderAddress + " → " + receiverAddress
        return firstReviewValue(senderAddress, receiverAddress,
                                parsed.participant_id, parsed.participant, parsed.address)
    }

    function openProcessReviewDialog() {
        if (slatepackActionsBlocked || slatepackArea.text.trim().length === 0) return
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
            processReviewState       = firstReviewValue(parsed.state, parsed.sta)
            processReviewMode        = deriveReviewMode(parsed, processReviewState)
            processReviewWorkflowId  = firstReviewValue(parsed.id, parsed.workflow_id)
            processReviewAmount      = firstReviewValue(parsed.amount, parsed.amount_display, parsed.invoice_amount,
                                                        parsed.receiver_amount_display, parsed.amt)
            processReviewNote        = firstReviewValue(parsed.note, parsed.message, parsed.memo, parsed.msg)
            processReviewParticipant = deriveReviewParticipant(parsed)
            processReviewKernel      = firstReviewValue(parsed.kernel_commitment, parsed.kernel_excess,
                                                        parsed.excess, parsed.kern)
            processReviewMessage     = walletRoot.tf("browser_wallet_slatepack_review_ready", "Confirm the decoded Slatepack details before processing.")
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

    // ── Amount dialog popup ───────────────────────────────────────────────────
    Popup {
        id: amountActionPopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: Math.min(walletRoot.width - 28, 500)
        padding: 0

        onOpened: {
            walletPageSettings.amountDraft = "1"
            if (amountDialogMode === "send")
                slatepackSection.syncSliderFromAmountField()
            PlatformBridge.requestFocus(amountField.bridgeId)
        }
        onClosed: walletRoot.syncBrowserShortcutContext(null)

        background: Rectangle {
            radius: 22
            color: "#0d1c2a"
            border.color: amountDialogMode === "receive" ? "#605000" : "#1e3d55"
            border.width: 1
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 14

            Label {
                Layout.fillWidth: true
                text: amountDialogMode === "receive"
                      ? walletRoot.tf("browser_wallet_amount_dialog_receive_title", "Receive Amount")
                      : walletRoot.tf("browser_wallet_amount_dialog_send_title", "Send Amount")
                color: "#ddeeff"
                font.pixelSize: walletRoot.veryPhoneMode ? 20 : (walletRoot.phoneMode ? 24 : 26)
                font.weight: Font.Bold
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                text: amountDialogMode === "receive"
                      ? walletRoot.tf("browser_wallet_amount_dialog_receive_note", "Enter the invoice amount to start the receive workflow.")
                      : walletRoot.tf("browser_wallet_amount_dialog_send_note", "Choose how much you want to send. Use Max to fill the spendable balance.")
                color: "#3a6a84"
                font.pixelSize: walletRoot.bodyTextSize
                wrapMode: Text.WordWrap
            }

            AppComponents.AppTextField {
                id: amountField
                Layout.fillWidth: true
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
                validator: DoubleValidator { bottom: 0; decimals: 9; notation: DoubleValidator.StandardNotation }
                onTextChanged: walletPageSettings.amountDraft = text
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

                background: Rectangle {
                    x: amountSlider.leftPadding
                    y: amountSlider.topPadding + amountSlider.availableHeight / 2 - height / 2
                    width: amountSlider.availableWidth
                    height: 4
                    radius: 2
                    color: "#0c2030"

                    Rectangle {
                        width: amountSlider.visualPosition * parent.width
                        height: parent.height
                        radius: 2
                        color: "#FEF102"
                    }
                }

                handle: Rectangle {
                    x: amountSlider.leftPadding + amountSlider.visualPosition * (amountSlider.availableWidth - width)
                    y: amountSlider.topPadding + amountSlider.availableHeight / 2 - height / 2
                    width: 18; height: 18; radius: 9
                    color: amountSlider.pressed ? "#FEF102" : "#FEF102"
                    border.color: "#0c2030"
                    border.width: 2
                }

                onMoved: slatepackSection.syncAmountFieldFromSlider()
                onValueChanged: { if (pressed) slatepackSection.syncAmountFieldFromSlider() }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: amountDialogMode === "send"

                Button {
                    text: walletRoot.tf("browser_wallet_amount_max", "Max")
                    font.pixelSize: walletRoot.controlTextSize
                    enabled: spendableAmountValue > 0
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
                        amountSlider.value = spendableAmountValue
                        slatepackSection.syncAmountFieldFromSlider()
                    }
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: walletRoot.tf("browser_wallet_spendable", "Spendable") + ": " + grinWalletController.spendableBalance
                    color: "#2a5a72"
                    font.pixelSize: walletRoot.compactTextSize
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }

                Button {
                    text: walletRoot.tf("browser_wallet_cancel", "Cancel")
                    font.pixelSize: walletRoot.controlTextSize
                    background: Rectangle {
                        radius: 10; color: parent.down ? "#07111c" : (parent.hovered ? "#060e18" : "transparent")
                        border.color: parent.hovered ? "#1e3a52" : "#152a3c"; border.width: 1
                    }
                    contentItem: Label {
                        text: parent.text; font: parent.font; color: parent.hovered ? "#88b8d8" : "#5a8eac"
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: amountActionPopup.close()
                }

                Button {
                    text: amountDialogMode === "receive"
                          ? walletRoot.tf("browser_wallet_receive", "Receive")
                          : walletRoot.tf("browser_wallet_send", "Send")
                    font.pixelSize: walletRoot.controlTextSize
                    font.weight: Font.DemiBold
                    enabled: amountField.text.trim().length > 0
                    background: Rectangle {
                        radius: 10
                        color: !parent.enabled ? "#181200" : (parent.down ? "#201800" : (parent.hovered ? "#1a1500" : "#181200"))
                        border.color: !parent.enabled ? "#181000" : (parent.down ? "#A08800" : (parent.hovered ? "#C09800" : "#806800"))
                        border.width: 1
                    }
                    contentItem: Label {
                        text: parent.text; font: parent.font
                        color: !parent.enabled ? "#605000" : (parent.down ? "#FEF102" : (parent.hovered ? "#FEF102" : "#FEF102"))
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: slatepackSection.confirmAmountDialog()
                }
            }
        }
    }

    // ── Process review dialog ─────────────────────────────────────────────────
    Popup {
        id: processReviewPopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: Math.min(walletRoot.width - 28, 700)
        padding: 0

        background: Rectangle {
            radius: 22
            color: "#0d1c2a"
            border.color: "#1e3d55"
            border.width: 1
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 14

            Label {
                Layout.fillWidth: true
                text: walletRoot.tf("browser_wallet_slatepack_review_title", "Validate Slatepack")
                color: "#ddeeff"
                font.pixelSize: walletRoot.phoneMode ? 22 : 26
                font.weight: Font.Bold
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                text: processReviewMessage
                color: "#4a7898"
                font.pixelSize: walletRoot.bodyTextSize
                wrapMode: Text.WordWrap
            }

            // Review fields
            Rectangle {
                Layout.fillWidth: true
                radius: 12
                color: "#091825"
                border.color: "#132e42"
                border.width: 1
                implicitHeight: reviewGrid.implicitHeight + 20

                GridLayout {
                    id: reviewGrid
                    anchors.fill: parent
                    anchors.margins: 14
                    columns: 2
                    columnSpacing: 14
                    rowSpacing: 8

                    Repeater {
                        model: [
                            { label: walletRoot.tf("browser_wallet_slatepack_review_mode",      "Mode"),        value: processReviewMode },
                            { label: walletRoot.tf("browser_wallet_slatepack_review_state",     "State"),       value: processReviewState },
                            { label: walletRoot.tf("browser_wallet_slatepack_review_workflow",  "Workflow ID"), value: processReviewWorkflowId },
                            { label: walletRoot.tf("browser_wallet_slatepack_review_amount",    "Amount"),      value: processReviewAmount },
                            { label: walletRoot.tf("browser_wallet_slatepack_review_participant","Participant"), value: processReviewParticipant },
                            { label: walletRoot.tf("browser_wallet_slatepack_review_kernel",    "Kernel"),      value: processReviewKernel }
                        ]

                        Label {
                            text: modelData.label
                            color: "#2a5a72"
                            font.pixelSize: walletRoot.compactTextSize
                        }
                        Label {
                            text: modelData.value
                            color: "#b0d0e8"
                            font.pixelSize: walletRoot.compactTextSize
                            wrapMode: Text.WrapAnywhere
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            // Note
            Rectangle {
                Layout.fillWidth: true
                visible: processReviewNote !== "-" && processReviewNote.length > 0
                radius: 12
                color: "#091825"
                border.color: "#132e42"
                border.width: 1
                implicitHeight: processReviewNoteLabel.implicitHeight + 20

                Label {
                    id: processReviewNoteLabel
                    anchors.fill: parent
                    anchors.margins: 12
                    text: walletRoot.tf("browser_wallet_slatepack_review_note", "Note") + ": " + processReviewNote
                    color: "#5a8eaa"
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
                    background: Rectangle {
                        radius: 10; color: parent.down ? "#07111c" : (parent.hovered ? "#060e18" : "transparent")
                        border.color: parent.hovered ? "#1e3a52" : "#152a3c"; border.width: 1
                    }
                    contentItem: Label {
                        text: parent.text; font: parent.font; color: parent.hovered ? "#88b8d8" : "#5a8eac"
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: processReviewPopup.close()
                }

                Button {
                    text: walletRoot.tf("browser_wallet_process", "Process")
                    font.pixelSize: walletRoot.controlTextSize
                    font.weight: Font.DemiBold
                    background: Rectangle {
                        radius: 10
                        color: parent.down ? "#201800" : (parent.hovered ? "#1a1500" : "#181200")
                        border.color: parent.down ? "#A08800" : (parent.hovered ? "#C09800" : "#806800")
                        border.width: 1
                    }
                    contentItem: Label {
                        text: parent.text; font: parent.font
                        color: parent.down ? "#FEF102" : (parent.hovered ? "#FEF102" : "#FEF102")
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: slatepackSection.processValidatedSlatepack()
                }
            }
        }
    }

    // ── Main card ─────────────────────────────────────────────────────────────
    BrowserWalletSectionCard {
        id: slatepackCard
        width: parent.width
        contentPadding: 16

        ColumnLayout {
            id: slatepackColumn
            Layout.fillWidth: true
            spacing: 12

            // Recovery / rescan warning
            Rectangle {
                Layout.fillWidth: true
                visible: walletRoot.nodeStatusMode() !== "online" || walletRoot.pendingRecoveryCount() > 0
                radius: 12
                color: walletRoot.nodeStatusMode() === "offline" ? "#1e0c10" : "#1e1508"
                border.color: walletRoot.recoveryBannerColor()
                border.width: 1
                implicitHeight: slatepackRecoveryLabel.implicitHeight + 20

                Label {
                    id: slatepackRecoveryLabel
                    anchors.fill: parent
                    anchors.margins: 12
                    text: walletRoot.nodeStatusMode() === "online"
                          ? walletRoot.tf("browser_wallet_slatepack_recovery_pending", "Broadcast recovery is still in progress for one or more transactions. You can validate and process Slatepacks, but confirm node status before sending again.")
                          : walletRoot.tf("browser_wallet_slatepack_recovery_offline", "Node connectivity is degraded. Slatepack review still works, but sending and broadcast recovery should wait for a successful refresh.")
                    color: walletRoot.recoveryBannerColor()
                    font.pixelSize: walletRoot.bodyTextSize
                    wrapMode: Text.WordWrap
                }
            }

            // Full rescan blocked warning
            Rectangle {
                Layout.fillWidth: true
                visible: slatepackActionsBlocked
                radius: 12
                color: "#1e1508"
                border.color: "#e0a840"
                border.width: 1
                implicitHeight: scanBlockedLabel.implicitHeight + 20

                Label {
                    id: scanBlockedLabel
                    anchors.fill: parent
                    anchors.margins: 12
                    text: walletRoot.tf("browser_wallet_slatepack_full_rescan_blocked", "Full rescan is active. Creating or processing Slatepacks is disabled until the scan completes.")
                    color: "#e0a840"
                    font.pixelSize: walletRoot.bodyTextSize
                    wrapMode: Text.WordWrap
                }
            }

            // Send / Receive primary action buttons
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                // Send
                Button {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    text: walletRoot.tf("browser_wallet_send", "Send")
                    enabled: walletRoot.nodeStatusMode() === "online" && !slatepackActionsBlocked
                    font.pixelSize: walletRoot.controlTextSize
                    font.weight: Font.DemiBold
                    leftPadding: 16; rightPadding: 16; topPadding: 14; bottomPadding: 14
                    background: Rectangle {
                        radius: 14
                        color: !parent.enabled ? "#10111a"
                             : (parent.down ? "#22101a" : (parent.hovered ? "#1a0d14" : "#140810"))
                        border.color: !parent.enabled ? "#1a1a24"
                                    : (parent.down ? "#cc3058" : (parent.hovered ? "#aa2848" : "#882038"))
                        border.width: 1
                    }
                    contentItem: Label {
                        text: parent.text; font: parent.font
                        color: !parent.enabled ? "#302838"
                             : (parent.down ? "#ff6070" : (parent.hovered ? "#ee5060" : "#cc3050"))
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: slatepackSection.openAmountDialog("send")
                }

                // Receive
                Button {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    text: walletRoot.tf("browser_wallet_receive", "Receive")
                    enabled: !slatepackActionsBlocked
                    font.pixelSize: walletRoot.controlTextSize
                    font.weight: Font.DemiBold
                    leftPadding: 16; rightPadding: 16; topPadding: 14; bottomPadding: 14
                    background: Rectangle {
                        radius: 14
                        color: !parent.enabled ? "#0f1612"
                             : (parent.down ? "#0f2015" : (parent.hovered ? "#0d1a12" : "#0d180f"))
                        border.color: !parent.enabled ? "#1b2b20"
                                    : (parent.down ? "#2f9a4f" : (parent.hovered ? "#39b35c" : "#2c7f46"))
                        border.width: 1
                    }
                    contentItem: Label {
                        text: parent.text; font: parent.font
                        color: !parent.enabled ? "#4c6c57"
                             : (parent.down ? "#7dff9f" : (parent.hovered ? "#8cffaa" : "#7fe59b"))
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: slatepackSection.openAmountDialog("receive")
                }
            }

            // Slatepack status indicator
            Rectangle {
                Layout.fillWidth: true
                visible: walletRoot.slatepackStatusText.length > 0
                radius: 10
                color: "#091825"
                border.color: walletRoot.slatepackStatusColor
                border.width: 1
                implicitHeight: slatepackStatusLabel.implicitHeight + 16

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

            // Slatepack input area
            ScrollView {
                id: slatepackScroll
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(130, Math.round((walletRoot.controlTextSize + 6) * 5 + 24))
                clip: true
                contentWidth: availableWidth

                AppComponents.AppTextArea {
                    id: slatepackArea
                    objectName: "slatepackArea"
                    width: slatepackScroll.availableWidth
                    height: Math.max(slatepackScroll.availableHeight, Math.round((font.pixelSize + 6) * 5 + topPadding + bottomPadding))
                    editorTitle: walletRoot.tf("browser_wallet_paste_slatepack", "Paste Slatepack")
                    wrapMode: TextEdit.WrapAnywhere
                    textFormat: TextEdit.PlainText
                    color: "#c8e8ff"
                    selectionColor: "#2a6090"
                    selectedTextColor: "#ddeeff"
                    font.pixelSize: walletRoot.controlTextSize
                    selectByMouse: true
                    persistentSelection: true
                    activeFocusOnPress: true
                    text: ""
                    placeholderText: "BEGINSLATEPACK. ..."
                }
            }

            // Action buttons
            Flow {
                Layout.fillWidth: true
                spacing: 8

                Button {
                    text: walletRoot.tf("browser_wallet_validate_process", "Validate and Process")
                    font.pixelSize: walletRoot.controlTextSize
                    enabled: slatepackArea.text.trim().length > 0 && !slatepackActionsBlocked
                    background: Rectangle {
                        radius: 10
                        color: !parent.enabled ? "transparent" : (parent.down ? "#07111c" : (parent.hovered ? "#060e18" : "transparent"))
                        border.color: !parent.enabled ? "#152a3c" : (parent.down ? "#254460" : (parent.hovered ? "#1e3a52" : "#152a3c"))
                        border.width: 1
                    }
                    contentItem: Label {
                        text: parent.text; font: parent.font
                        color: !parent.enabled ? "#355468" : (parent.down ? "#a8d0e8" : (parent.hovered ? "#88b8d8" : "#4a7898"))
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: slatepackSection.openProcessReviewDialog()
                }

                Button {
                    text: walletRoot.tf("browser_wallet_clear", "Clear")
                    font.pixelSize: walletRoot.controlTextSize
                    background: Rectangle {
                        radius: 10; color: parent.down ? "#07111c" : (parent.hovered ? "#060e18" : "transparent")
                        border.color: parent.hovered ? "#1e3a52" : "#152a3c"; border.width: 1
                    }
                    contentItem: Label {
                        text: parent.text; font: parent.font; color: parent.hovered ? "#88b8d8" : "#5a8eac"
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: slatepackSection.clearSlatepackWorkspace()
                }
            }
        }
    }
}
