import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore

Item {
    id: root
    property var i18n: null
    property string authMode: "unlock"
    property string authNetworkDraft: "mainnet"
    property string activeSection: "home"
    property bool deleteConfirmOpen: false
    property string errorDialogText: ""
    property string restoreMnemonicDraft: ""
    property string backupImportDraft: ""
    property string revealSeedPasswordDraft: ""
    property string unlockPasswordDraft: ""
    property string passwordDraft: ""
    property string passwordConfirmDraft: ""
    property string nodeUrlDraft: ""
    property string slatepackStatusText: ""
    property var pasteTargetControl: null
    property string pasteDialogTitle: ""
    property string pasteDialogPlaceholder: ""
    property string pasteDialogText: ""
    property color slatepackStatusColor: "#8fb4c9"
    signal backRequested()

    anchors.fill: parent

    function tf(key, fallback) { return i18n ? i18n.tf(key, fallback) : fallback }
    function syncAuthMode() { authMode = grinWalletController.walletExists ? "unlock" : "create" }
    function clearPasswordDrafts() {
        unlockPasswordDraft = ""
        passwordDraft = ""
        passwordConfirmDraft = ""
    }
    function syncNodeDraft() {
        nodeUrlDraft = grinWalletController.nodeUrl
    }
    function openPasteDialog(targetControl, title, placeholder) {
        pasteTargetControl = targetControl
        pasteDialogTitle = title
        pasteDialogPlaceholder = placeholder
        pasteDialogText = targetControl && targetControl.text !== undefined ? targetControl.text : ""
        pasteInputPopup.open()
    }
    function applyPasteDialog() {
        if (pasteTargetControl && pasteTargetControl.text !== undefined) {
            pasteTargetControl.text = pasteDialogText
            pasteTargetControl.forceActiveFocus()
            root.syncBrowserShortcutContext(pasteTargetControl)
        }
        pasteInputPopup.close()
    }
    function syncAuthNetworkDraft() {
        authNetworkDraft = grinWalletController.selectedNetwork && grinWalletController.selectedNetwork.length > 0
            ? grinWalletController.selectedNetwork
            : "mainnet"
    }
    function setSlatepackStatus(text, color) {
        slatepackStatusText = text
        slatepackStatusColor = color || "#8fb4c9"
    }
    function showErrorDialog(text) {
        if (!text || text.trim().length === 0)
            return
        errorDialogText = text
        errorPopup.open()
    }
    function handleTextControlKeyPress(control, event) {
        event.accepted = false
    }
    function syncBrowserShortcutContext(control) {
        if (!control) {
            grinWalletController.updateBrowserShortcutContext("", "", false)
            return
        }
        var textValue = control.text !== undefined && control.text !== null ? control.text : ""
        var selectedValue = control.selectedText !== undefined && control.selectedText !== null ? control.selectedText : ""
        grinWalletController.updateBrowserShortcutContext(textValue, selectedValue, !!control.activeFocus)
    }
    function openRevealSeedPopup() {
        revealSeedPasswordDraft = ""
        revealSeedPopup.open()
    }
    function defaultNetworkNodeUrl() {
        return grinWalletController.selectedNetwork === "testnet"
            ? "https://testnet.grinffindor.org/v2/foreign"
            : "https://mainnet.grinffindor.org/v2/foreign"
    }
    function storageStatusColor() {
        if (grinWalletController.storagePersistenceState === "persistent" || grinWalletController.storagePersistenceState === "native")
            return "#8ff0c8"
        if (grinWalletController.storagePersistenceState === "best-effort")
            return "#ffd280"
        return "#8fb4c9"
    }
    function storageStatusText() {
        if (grinWalletController.storagePersistenceState === "persistent")
            return root.tf("browser_wallet_storage_persistent", "Browser storage is persistent.")
        if (grinWalletController.storagePersistenceState === "best-effort")
            return root.tf("browser_wallet_storage_best_effort", "Browser storage is best-effort only. Export a backup and request persistent storage on this device.")
        if (grinWalletController.storagePersistenceState === "native")
            return root.tf("browser_wallet_storage_native", "Desktop storage is handled by the local app data directory.")
        return root.tf("browser_wallet_storage_unknown", "Browser storage status has not been confirmed yet.")
    }
    function nodeStatusMode() {
        if (grinWalletController.syncStatus === "Node query failed")
            return "offline"
        if (grinWalletController.syncStatus === "Querying node..." || grinWalletController.chainHeight === 0)
            return "connecting"
        return "online"
    }
    function pendingRecoveryCount() {
        var list = grinWalletController.transactionHistory || []
        var count = 0
        for (var i = 0; i < list.length; ++i) {
            var entry = list[i]
            var status = entry.status || ""
            if (status === "broadcast_pending" || status === "broadcast_failed" || status === "in_mempool")
                count += 1
        }
        return count
    }
    function recoveryBannerVisible() {
        return nodeStatusMode() !== "online" || pendingRecoveryCount() > 0
    }
    function recoveryBannerColor() {
        if (nodeStatusMode() === "offline")
            return "#ffb4b4"
        if (pendingRecoveryCount() > 0)
            return "#ffd280"
        return "#8fb4c9"
    }
    function recoveryBannerText() {
        var count = pendingRecoveryCount()
        if (nodeStatusMode() === "offline") {
            return root.tf("browser_wallet_recovery_offline", "The wallet cannot currently reach the external node. Balances and broadcast recovery may be stale until the node responds again.")
        }
        if (nodeStatusMode() === "connecting") {
            return root.tf("browser_wallet_recovery_connecting", "The wallet is still querying the node. Wait for a tip update before starting sends or trusting the displayed chain state.")
        }
        if (count > 0) {
            return root.tf("browser_wallet_recovery_pending", "Recovery check active for ") + count + root.tf("browser_wallet_recovery_pending_suffix", " transaction(s). Refresh the node after reconnecting so broadcast and mempool status can be reconciled.")
        }
        return ""
    }
    function txStatusColor(status) {
        if (status === "confirmed")
            return "#8ff0c8"
        if (status === "broadcast_failed" || status === "cancelled")
            return "#ffb4b4"
        if (status === "broadcast_pending" || status === "in_mempool" || status === "broadcasted")
            return "#ffd280"
        return "#8fb4c9"
    }
    function txRecoveryHint(modelData) {
        var status = modelData.status || ""
        if (status === "broadcast_pending")
            return root.tf("browser_wallet_history_recovery_pending", "Broadcast was submitted locally and is waiting for node confirmation. Use Refresh after reconnecting to reconcile it.")
        if (status === "broadcast_failed")
            return root.tf("browser_wallet_history_recovery_failed", "Node broadcast failed. Check node connectivity, then retry broadcast for this workflow.")
        if (status === "in_mempool")
            return root.tf("browser_wallet_history_recovery_mempool", "Transaction is seen in the mempool but not confirmed yet. Keep the node online until kernel confirmation arrives.")
        if (status === "broadcasted")
            return root.tf("browser_wallet_history_recovery_broadcasted", "Transaction was broadcast and is awaiting mempool or kernel confirmation. A refresh or reload will continue status recovery.")
        return ""
    }
    function canBroadcastEntry(modelData) {
        return nodeStatusMode() === "online"
            && modelData.tx_ready === true
            && modelData.broadcasted !== true
            && modelData.status !== "cancelled"
            && modelData.status !== "confirmed"
    }
    function canCancelEntry(modelData) {
        return modelData.status !== "cancelled"
            && modelData.status !== "confirmed"
            && modelData.status !== "broadcast_pending"
            && modelData.status !== "broadcasted"
            && modelData.status !== "in_mempool"
            && (modelData.confirmations === undefined || modelData.confirmations < 1)
    }
    function updateSlatepackStatus(decodedText) {
        var trimmed = decodedText ? decodedText.trim() : ""
        if (trimmed.length === 0) {
            setSlatepackStatus(root.tf("browser_wallet_slatepack_status_idle", "Paste a Slatepack, decode it, or start a SEND/RECEIVE workflow."), "#8fb4c9")
            return
        }

        try {
            var parsed = JSON.parse(trimmed)
            if (parsed.encrypted_slatepack) {
                setSlatepackStatus(parsed.note || root.tf("browser_wallet_slatepack_status_encrypted", "Encrypted Slatepack detected. Unlock the matching wallet to decrypt it."), "#ffd280")
                return
            }
            if (parsed.external_slatepack) {
                setSlatepackStatus(parsed.note || root.tf("browser_wallet_slatepack_status_invalid", "Slatepack payload could not be parsed."), "#ffb4b4")
                return
            }

            var state = parsed.state || parsed.sta || "-"
            var workflowId = parsed.id || parsed.workflow_id || "-"
            var mode = parsed.body_type || parsed.mode || ((state && state.indexOf("I") === 0) ? "invoice" : ((state && state.indexOf("S") === 0) ? "send" : "-"))
            setSlatepackStatus(root.tf("browser_wallet_slatepack_status_ready", "Decoded Slatepack ready") + ": " + mode + " / " + state + " / " + workflowId, "#8ff0c8")
            return
        } catch (error) {
        }

        setSlatepackStatus(root.tf("browser_wallet_slatepack_status_preview", "Decoded text preview loaded."), "#8fb4c9")
    }
    function syncWorkflowEditors() {
        slatepackArea.text = grinWalletController.workflowSlatepack
        decodedArea.text = grinWalletController.workflowDecoded
        updateSlatepackStatus(decodedArea.text)
    }
    function beginRestoreReset() {
        deleteConfirmOpen = false
        grinWalletController.deleteWallet()
        authMode = "restore"
        restoreMnemonicDraft = ""
        backupImportDraft = ""
        clearPasswordDrafts()
    }
    function submitAuth() {
        var targetNetwork = authNetworkDraft
        var walletName = walletPageSettings.walletNameDraft
        var unlockPassword = unlockPasswordDraft
        var createPassword = passwordDraft
        var restoreMnemonic = restoreMnemonicDraft
        var importBackup = backupImportDraft

        if (authMode === "unlock") {
            grinWalletController.setSelectedNetwork(targetNetwork)
            grinWalletController.unlockWallet(unlockPassword)
            return
        }
        if (authMode === "import_backup") {
            grinWalletController.importEncryptedWalletBackup(importBackup)
            return
        }
        grinWalletController.setSelectedNetwork(targetNetwork)
        if (walletName.trim().length === 0) return
        if (createPassword.length === 0 || createPassword !== passwordConfirmDraft) return
        if (authMode === "create") {
            grinWalletController.createWallet(walletName, createPassword)
            return
        }
        grinWalletController.restoreWallet(walletName, restoreMnemonic, createPassword)
    }

    Settings {
        id: walletPageSettings
        category: "browserWalletPage"
        property string walletNameDraft: ""
        property string amountDraft: ""
        property string noteDraft: ""
    }

    Rectangle { anchors.fill: parent; color: "#091018" }
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#10273a" }
            GradientStop { position: 0.45; color: "#091018" }
            GradientStop { position: 1.0; color: "#05080d" }
        }
    }

    Rectangle {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 72
        color: "#101822"
        border.color: "#234b63"
        z: 2

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            spacing: 14

            Button {
                text: root.tf("browser_wallet_back_lock", "Back (Lock)")
                onClicked: {
                    if (grinWalletController.walletUnlocked) grinWalletController.lockWallet()
                    root.backRequested()
                }
            }

            Label {
                text: root.tf("browser_wallet_title", "Grin Browser Wallet") + " / "
                      + root.tf("browser_wallet_self_custodial", "Self-Custodial")
                color: "#f7fbff"
                font.pixelSize: 28
                font.weight: Font.Bold
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                radius: 14
                color: "#122231"
                border.color: root.nodeStatusMode() === "offline" ? "#8a3f3f"
                             : (root.nodeStatusMode() === "connecting" ? "#8a7440" : "#2f607a")
                implicitWidth: statusLabel.implicitWidth + 24
                implicitHeight: 34

                Label {
                    id: statusLabel
                    anchors.centerIn: parent
                    text: grinWalletController.syncStatus
                    color: "#d8f3ff"
                    font.pixelSize: 13
                }
            }
        }
    }

    Column {
        anchors.top: topBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 18
        spacing: 10
        visible: grinWalletController.walletUnlocked

        Label {
            width: parent.width
            visible: grinWalletController.lastInfo.length > 0
            text: grinWalletController.lastInfo
            color: "#8ff0c8"
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }

        Label {
            width: parent.width
            visible: grinWalletController.lastError.length > 0
            text: grinWalletController.lastError
            color: "#ffb4b4"
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
    }

    RowLayout {
        anchors.top: topBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 18
        anchors.topMargin: 54
        spacing: 18
        visible: grinWalletController.walletUnlocked

        Rectangle {
            Layout.preferredWidth: Math.min(300, root.width * 0.28)
            Layout.fillHeight: true
            radius: 28
            color: "#0f1722"
            border.color: "#26465b"

            Column {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 12

                Label {
                    width: parent.width
                    text: root.tf("browser_wallet_sidebar_title", "Wallet")
                    color: "#ffffff"
                    font.pixelSize: 28
                    font.weight: Font.Bold
                }

                Label {
                    width: parent.width
                    text: grinWalletController.walletName.length > 0 ? grinWalletController.walletName : root.tf("browser_wallet_metric_empty", "Not created")
                    color: "#8ff0c8"
                    wrapMode: Text.WordWrap
                }

                Repeater {
                    model: [
                        { key: "home", title: root.tf("browser_wallet_nav_home", "Home") },
                        { key: "utxos", title: root.tf("browser_wallet_nav_utxos", "UTXOs") },
                        { key: "slatepack", title: root.tf("browser_wallet_nav_slatepack", "Slatepack") },
                        { key: "settings", title: root.tf("browser_wallet_nav_settings", "Settings") }
                    ]

                    Column {
                        width: parent.width
                        spacing: 6

                        Button {
                            width: parent.width
                            text: modelData.title
                            flat: true
                            highlighted: root.activeSection === modelData.key
                            onClicked: root.activeSection = modelData.key
                        }
                    }
                }

            }
        }

        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: contentColumn.implicitHeight + 24
            clip: true
            ScrollBar.vertical: ScrollBar {}

            Column {
                id: contentColumn
                width: parent.width
                spacing: 18

                Rectangle {
                    width: parent.width
                    radius: 24
                    color: root.nodeStatusMode() === "offline" ? "#34191d"
                         : (root.pendingRecoveryCount() > 0 ? "#352816" : "#132635")
                    border.color: root.recoveryBannerColor()
                    visible: root.recoveryBannerVisible()
                    implicitHeight: recoveryColumn.implicitHeight + 30

                    ColumnLayout {
                        id: recoveryColumn
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 10

                        Label {
                            Layout.fillWidth: true
                            text: root.tf("browser_wallet_recovery_title", "Operational Recovery")
                            color: "#ffffff"
                            font.pixelSize: 24
                            font.weight: Font.Bold
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.recoveryBannerText()
                            color: root.recoveryBannerColor()
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: grinWalletController.lastError.length > 0 && root.nodeStatusMode() !== "online"
                            text: grinWalletController.lastError
                            color: "#ffd3d3"
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Button {
                                text: root.tf("browser_wallet_refresh", "Refresh")
                                onClicked: grinWalletController.refreshNodeStatus()
                            }
                            Button {
                                text: root.tf("browser_wallet_rescan", "Full Rescan")
                                enabled: root.nodeStatusMode() === "online"
                                onClicked: grinWalletController.rescanWallet()
                            }
                            Button {
                                text: root.tf("browser_wallet_nav_settings", "Settings")
                                onClicked: root.activeSection = "settings"
                            }
                            Item { Layout.fillWidth: true }
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    radius: 28
                    color: "#102131"
                    border.color: "#29516a"
                    visible: root.activeSection === "home"
                    implicitHeight: overviewColumn.implicitHeight + 36

                    Column {
                        id: overviewColumn
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 0

                        GridLayout {
                            width: parent.width
                            columns: width < 760 ? 1 : 3
                            rowSpacing: 12
                            columnSpacing: 12

                            Repeater {
                                model: [
                                    { title: root.tf("browser_wallet_metric_network", "Network"), value: grinWalletController.selectedNetwork },
                                    { title: root.tf("browser_wallet_metric_chain", "Chain Height"), value: "" + grinWalletController.chainHeight },
                                    { title: root.tf("browser_wallet_metric_scan", "Scan Height"), value: "" + grinWalletController.scanHeight },
                                    { title: root.tf("browser_wallet_metric_balance", "Spendable"), value: grinWalletController.spendableBalance + " GRIN" },
                                    { title: root.tf("browser_wallet_awaiting_confirmation", "Awaiting Confirmation"), value: grinWalletController.awaitingConfirmationBalance + " GRIN" },
                                    { title: root.tf("browser_wallet_locked", "Locked"), value: grinWalletController.lockedBalance + " GRIN" }
                                ]

                                Rectangle {
                                    Layout.fillWidth: true
                                    radius: 20
                                    color: "#0e1b27"
                                    border.color: "#26465b"
                                    implicitHeight: 88

                                    Column {
                                        anchors.fill: parent
                                        anchors.margins: 14
                                        spacing: 4

                                        Label { text: modelData.title; color: "#8fb4c9"; font.pixelSize: 13 }
                                        Label {
                                            width: parent.width
                                            text: modelData.value
                                            color: "#ffffff"
                                            wrapMode: Text.WordWrap
                                            font.pixelSize: 22
                                            font.weight: Font.Bold
                                        }
                                    }
                                }
                            }
                        }

                    }
                }

                Rectangle {
                    width: parent.width
                    radius: 26
                    color: "#143326"
                    border.color: "#2d7055"
                    visible: root.activeSection === "home" && grinWalletController.mnemonicPreview.length > 0
                    implicitHeight: backupColumn.implicitHeight + 34

                    ColumnLayout {
                        id: backupColumn
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12

                        Label {
                            Layout.fillWidth: true
                            text: root.tf("browser_wallet_backup_title", "Write Down Your Seed Phrase Now")
                            color: "#ffffff"
                            font.pixelSize: 26
                            font.weight: Font.Bold
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.tf("browser_wallet_backup_note", "This is the only time the seed phrase is shown automatically. Store it offline and keep it away from screenshots, chat logs, and cloud notes.")
                            color: "#d9f8e9"
                            wrapMode: Text.WordWrap
                        }

                        TextArea {
                            id: mnemonicPreviewArea
                            Layout.fillWidth: true
                            Layout.preferredHeight: 116
                            readOnly: true
                            wrapMode: TextEdit.Wrap
                            textFormat: TextEdit.PlainText
                            selectByMouse: true
                            persistentSelection: true
                            activeFocusOnPress: true
                            text: grinWalletController.mnemonicPreview
                            onActiveFocusChanged: root.syncBrowserShortcutContext(mnemonicPreviewArea)
                            onSelectedTextChanged: root.syncBrowserShortcutContext(mnemonicPreviewArea)
                            onTextChanged: root.syncBrowserShortcutContext(mnemonicPreviewArea)
                            Keys.onPressed: function(event) { root.handleTextControlKeyPress(mnemonicPreviewArea, event) }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Item { Layout.fillWidth: true }
                            Button {
                                text: root.tf("browser_wallet_backup_done", "I saved it")
                                onClicked: grinWalletController.dismissMnemonicPreview()
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    radius: 26
                    color: "#0f1b26"
                    border.color: "#26465b"
                    visible: root.activeSection === "utxos"
                    implicitHeight: utxoColumn.implicitHeight + 34

                    ColumnLayout {
                        id: utxoColumn
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12

                        Label {
                            text: root.tf("browser_wallet_utxo_title", "Wallet Outputs")
                            color: "#ffffff"
                            font.pixelSize: 28
                            font.weight: Font.Bold
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.tf("browser_wallet_utxo_note", "Tracked outputs, their wallet state, and the commitments currently held in the local wallet.")
                            color: "#cbdbe4"
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: grinWalletController.walletOutputs.length === 0
                            text: root.tf("browser_wallet_utxo_empty", "No wallet outputs are tracked yet. Run a scan or complete a transaction first.")
                            color: "#8fb4c9"
                            wrapMode: Text.WordWrap
                        }

                        Repeater {
                            model: grinWalletController.walletOutputs

                            Rectangle {
                                Layout.fillWidth: true
                                radius: 18
                                color: "#132635"
                                border.color: "#2a4f64"
                                implicitHeight: outputColumn.implicitHeight + 24

                                ColumnLayout {
                                    id: outputColumn
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 8

                                    Label {
                                        Layout.fillWidth: true
                                        text: (modelData.amount || "0.000000000") + " GRIN / "
                                              + root.tf("browser_wallet_utxo_status", "Status") + ": "
                                              + (modelData.status || "-")
                                        color: modelData.status === "spendable" ? "#8ff0c8"
                                             : modelData.status === "spent" ? "#ffb4b4"
                                             : "#ffd280"
                                        wrapMode: Text.WordWrap
                                    }

                                    GridLayout {
                                        Layout.fillWidth: true
                                        columns: width < 820 ? 1 : 2
                                        rowSpacing: 6
                                        columnSpacing: 12

                                        Label {
                                            Layout.fillWidth: true
                                            text: root.tf("browser_wallet_utxo_source", "Source") + ": "
                                                  + (modelData.source || "-")
                                            color: "#d7e9f4"
                                            wrapMode: Text.WordWrap
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            text: root.tf("browser_wallet_utxo_height", "Height") + ": "
                                                  + ((modelData.height || "").length > 0 ? modelData.height : "-")
                                            color: "#d7e9f4"
                                            wrapMode: Text.WordWrap
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            text: root.tf("browser_wallet_history_confirmations", "Confirmations") + ": "
                                                  + (modelData.confirmations !== undefined ? modelData.confirmations : 0)
                                            color: "#d7e9f4"
                                            wrapMode: Text.WordWrap
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            text: root.tf("browser_wallet_utxo_coinbase", "Coinbase") + ": "
                                                  + (modelData.coinbase ? root.tf("browser_wallet_utxo_yes", "yes")
                                                                        : root.tf("browser_wallet_utxo_no", "no"))
                                            color: "#d7e9f4"
                                            wrapMode: Text.WordWrap
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            text: root.tf("browser_wallet_utxo_onchain", "On chain") + ": "
                                                  + (modelData.on_chain ? root.tf("browser_wallet_utxo_yes", "yes")
                                                                        : root.tf("browser_wallet_utxo_no", "no"))
                                            color: "#d7e9f4"
                                            wrapMode: Text.WordWrap
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            text: root.tf("browser_wallet_utxo_workflow", "Workflow") + ": "
                                                  + ((modelData.workflow_id || "").length > 0 ? modelData.workflow_id : "-")
                                            color: "#d7e9f4"
                                            wrapMode: Text.WrapAnywhere
                                        }
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: root.tf("browser_wallet_utxo_commitment", "Commitment") + ": "
                                              + (modelData.commitment || "-")
                                        color: "#8fb4c9"
                                        wrapMode: Text.WrapAnywhere
                                        font.pixelSize: 12
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        visible: modelData.key_path !== undefined && (modelData.key_path || "").length > 0
                                        text: root.tf("browser_wallet_utxo_keypath", "Key path") + ": "
                                              + modelData.key_path
                                        color: "#8fb4c9"
                                        wrapMode: Text.WrapAnywhere
                                        font.pixelSize: 12
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        visible: !!(modelData.locked && modelData.workflow_id && modelData.workflow_id.length > 0)

                                        Button {
                                            text: root.tf("browser_wallet_utxo_cancel_lock", "Cancel Lock")
                                            enabled: grinWalletController.walletUnlocked
                                            onClicked: grinWalletController.cancelTransaction(modelData.workflow_id)
                                        }

                                        Item { Layout.fillWidth: true }
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    radius: 26
                    color: "#0f1b26"
                    border.color: "#26465b"
                    visible: root.activeSection === "home"
                    implicitHeight: historyColumn.implicitHeight + 34

                    ColumnLayout {
                        id: historyColumn
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12

                        Label {
                            text: root.tf("browser_wallet_history_title", "Transaction History")
                            color: "#ffffff"
                            font.pixelSize: 28
                            font.weight: Font.Bold
                        }

                        Repeater {
                            model: grinWalletController.transactionHistory

                            Rectangle {
                                Layout.fillWidth: true
                                radius: 18
                                color: "#132635"
                                border.color: "#2a4f64"
                                implicitHeight: txColumn.implicitHeight + 24
                                visible: modelData.workflow_id !== undefined

                                ColumnLayout {
                                    id: txColumn
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 8

                                    Label {
                                        Layout.fillWidth: true
                                        text: (modelData.mode || "-") + " / " + (modelData.state || "-") + " / " + (modelData.status || "-")
                                        color: root.txStatusColor(modelData.status || "")
                                        wrapMode: Text.WordWrap
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: (modelData.amount || "0") + " GRIN  fee " + (modelData.fee || "0")
                                        color: "#ffffff"
                                        wrapMode: Text.WordWrap
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.workflow_id || ""
                                        color: "#8fb4c9"
                                        wrapMode: Text.WrapAnywhere
                                        font.pixelSize: 12
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: root.tf("browser_wallet_history_confirmations", "Confirmations") + ": "
                                              + (modelData.confirmations !== undefined ? modelData.confirmations : 0)
                                        color: "#d7e9f4"
                                        wrapMode: Text.WordWrap
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: root.tf("browser_wallet_history_confirmed_height", "Confirmed Height") + ": "
                                              + (modelData.confirmed_height !== undefined && modelData.confirmed_height !== "" ? modelData.confirmed_height : "-")
                                        color: "#d7e9f4"
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
                                            text: root.tf("browser_wallet_history_payment_proof", "Payment Proof") + ": "
                                                  + (modelData.payment_proof_status || "-")
                                            color: modelData.payment_proof_status === "verified" ? "#8ff0c8"
                                                 : modelData.payment_proof_status === "invalid" ? "#ffb4b4"
                                                 : "#ffd280"
                                            wrapMode: Text.WordWrap
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            visible: modelData.rescan_rebuilt === true
                                            text: root.tf("browser_wallet_history_rescan", "Rebuilt from rescan backup")
                                            color: "#8fb4c9"
                                            wrapMode: Text.WordWrap
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            visible: modelData.last_node_check !== undefined && (modelData.last_node_check || "").length > 0
                                            text: root.tf("browser_wallet_history_last_node_check", "Last node check") + ": " + modelData.last_node_check
                                            color: "#8fb4c9"
                                            wrapMode: Text.WordWrap
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            visible: modelData.last_broadcast_attempt !== undefined && (modelData.last_broadcast_attempt || "").length > 0
                                            text: root.tf("browser_wallet_history_last_broadcast", "Last broadcast attempt") + ": " + modelData.last_broadcast_attempt
                                            color: "#8fb4c9"
                                            wrapMode: Text.WordWrap
                                        }
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        visible: root.txRecoveryHint(modelData).length > 0
                                        text: root.txRecoveryHint(modelData)
                                        color: root.txStatusColor(modelData.status || "")
                                        wrapMode: Text.WordWrap
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        visible: modelData.broadcast_error !== undefined && (modelData.broadcast_error || "").length > 0
                                        text: root.tf("browser_wallet_history_broadcast_error", "Broadcast error") + ": " + modelData.broadcast_error
                                        color: "#ffb4b4"
                                        wrapMode: Text.WordWrap
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        visible: modelData.output_commitments !== undefined
                                                 && modelData.output_commitments.length !== undefined
                                                 && modelData.output_commitments.length > 0
                                        text: root.tf("browser_wallet_history_outputs", "Outputs") + ": "
                                              + ((modelData.output_commitments && modelData.output_commitments.join)
                                                    ? modelData.output_commitments.join(", ")
                                                    : "")
                                        color: "#8fb4c9"
                                        wrapMode: Text.WrapAnywhere
                                        font.pixelSize: 12
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        visible: modelData.kernel_excess !== undefined && (modelData.kernel_excess || "").length > 0
                                        text: root.tf("browser_wallet_history_kernel", "Kernel Excess") + ": " + modelData.kernel_excess
                                        color: "#8fb4c9"
                                        wrapMode: Text.WrapAnywhere
                                        font.pixelSize: 12
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        visible: modelData.payment_proof_error !== undefined && (modelData.payment_proof_error || "").length > 0
                                        text: root.tf("browser_wallet_history_payment_proof_error", "Payment proof error") + ": "
                                              + modelData.payment_proof_error
                                        color: "#ffb4b4"
                                        wrapMode: Text.WordWrap
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        visible: modelData.payment_proof !== undefined
                                                 && modelData.payment_proof.saddr !== undefined
                                                 && (modelData.payment_proof.saddr || "").length > 0
                                        text: root.tf("browser_wallet_history_payment_proof_sender", "Proof sender") + ": "
                                              + ((modelData.payment_proof && modelData.payment_proof.saddr) ? modelData.payment_proof.saddr : "")
                                        color: "#8fb4c9"
                                        wrapMode: Text.WrapAnywhere
                                        font.pixelSize: 12
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        visible: modelData.payment_proof !== undefined
                                                 && modelData.payment_proof.raddr !== undefined
                                                 && (modelData.payment_proof.raddr || "").length > 0
                                        text: root.tf("browser_wallet_history_payment_proof_receiver", "Proof receiver") + ": "
                                              + ((modelData.payment_proof && modelData.payment_proof.raddr) ? modelData.payment_proof.raddr : "")
                                        color: "#8fb4c9"
                                        wrapMode: Text.WrapAnywhere
                                        font.pixelSize: 12
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        visible: modelData.payment_proof !== undefined
                                                 && modelData.payment_proof.rsig !== undefined
                                                 && (modelData.payment_proof.rsig || "").length > 0
                                        text: root.tf("browser_wallet_history_payment_proof_signature", "Receiver signature") + ": "
                                              + ((modelData.payment_proof && modelData.payment_proof.rsig) ? modelData.payment_proof.rsig : "")
                                        color: "#8fb4c9"
                                        wrapMode: Text.WrapAnywhere
                                        font.pixelSize: 12
                                    }

                                    TextArea {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: visible ? Math.min(140, Math.max(72, contentHeight + 18)) : 0
                                        visible: modelData.payment_proof !== undefined
                                                 && JSON.stringify(modelData.payment_proof).length > 2
                                        readOnly: true
                                        selectByMouse: true
                                        persistentSelection: true
                                        activeFocusOnPress: true
                                        wrapMode: TextEdit.WrapAnywhere
                                        color: "#d7e9f4"
                                        text: modelData.payment_proof !== undefined ? JSON.stringify(modelData.payment_proof, null, 2) : ""
                                        Keys.onPressed: function(event) { root.handleTextControlKeyPress(this, event) }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Button {
                                            text: modelData.status === "broadcast_failed"
                                                  ? root.tf("browser_wallet_history_retry_broadcast", "Retry Broadcast")
                                                  : root.tf("browser_wallet_history_broadcast", "Broadcast")
                                            enabled: root.canBroadcastEntry(modelData)
                                            onClicked: grinWalletController.broadcastTransaction(modelData.workflow_id)
                                        }
                                        Button {
                                            text: root.tf("browser_wallet_history_cancel", "Cancel")
                                            enabled: root.canCancelEntry(modelData)
                                            onClicked: grinWalletController.cancelTransaction(modelData.workflow_id)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    radius: 26
                    color: "#0f1b26"
                    border.color: "#26465b"
                    visible: root.activeSection === "slatepack"
                    implicitHeight: slatepackColumn.implicitHeight + 34

                    ColumnLayout {
                        id: slatepackColumn
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12

                        Label {
                            text: root.tf("browser_wallet_nav_slatepack", "Slatepack")
                            color: "#ffffff"
                            font.pixelSize: 28
                            font.weight: Font.Bold
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: root.tf("browser_wallet_slatepack_note", "Handle all Slatepack flows here. Start SEND at S1, RECEIVE at I1, and process incoming replies or invoices in the same workspace.")
                            color: "#cbdbe4"
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 16
                            visible: root.nodeStatusMode() !== "online" || root.pendingRecoveryCount() > 0
                            color: root.nodeStatusMode() === "offline" ? "#34191d" : "#352816"
                            border.color: root.recoveryBannerColor()
                            implicitHeight: slatepackRecoveryLabel.implicitHeight + 20

                            Label {
                                id: slatepackRecoveryLabel
                                anchors.fill: parent
                                anchors.margins: 10
                                text: root.nodeStatusMode() === "online"
                                      ? root.tf("browser_wallet_slatepack_recovery_pending", "Broadcast recovery is still in progress for one or more transactions. You can decode and process Slatepacks, but confirm node status before sending again.")
                                      : root.tf("browser_wallet_slatepack_recovery_offline", "Node connectivity is degraded. Decoding still works, but sending and broadcast recovery should wait for a successful refresh.")
                                color: root.recoveryBannerColor()
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
                                placeholderText: root.tf("browser_wallet_amount_placeholder", "Amount in GRIN, e.g. 1.000000000")
                                onActiveFocusChanged: root.syncBrowserShortcutContext(amountField)
                                onSelectedTextChanged: root.syncBrowserShortcutContext(amountField)
                                onTextChanged: {
                                    walletPageSettings.amountDraft = text
                                    root.syncBrowserShortcutContext(amountField)
                                }
                                Keys.onPressed: function(event) { root.handleTextControlKeyPress(amountField, event) }
                            }

                            TextField {
                                id: noteField
                                Layout.fillWidth: true
                                text: walletPageSettings.noteDraft
                                placeholderText: root.tf("browser_wallet_note_placeholder", "Optional note")
                                onActiveFocusChanged: root.syncBrowserShortcutContext(noteField)
                                onSelectedTextChanged: root.syncBrowserShortcutContext(noteField)
                                onTextChanged: {
                                    walletPageSettings.noteDraft = text
                                    root.syncBrowserShortcutContext(noteField)
                                }
                                Keys.onPressed: function(event) { root.handleTextControlKeyPress(noteField, event) }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Button {
                                text: root.tf("browser_wallet_paste_slatepack", "Paste Slatepack")
                                onClicked: {
                                    root.openPasteDialog(
                                        slatepackArea,
                                        root.tf("browser_wallet_paste_slatepack", "Paste Slatepack"),
                                        "BEGINSLATEPACK. ...")
                                }
                            }

                            Button {
                                text: root.tf("browser_wallet_decode", "Decode")
                                enabled: slatepackArea.text.trim().length > 0
                                onClicked: {
                                    decodedArea.text = grinWalletController.decodeSlatepack(slatepackArea.text)
                                    root.updateSlatepackStatus(decodedArea.text)
                                }
                            }

                            Button {
                                text: root.tf("browser_wallet_nav_send", "Send (S1-S3)")
                                enabled: root.nodeStatusMode() === "online"
                                onClicked: {
                                    grinWalletController.startSendWorkflow(amountField.text, noteField.text)
                                    root.syncWorkflowEditors()
                                }
                            }

                            Button {
                                text: root.tf("browser_wallet_nav_receive", "Receive (I1-I3)")
                                onClicked: {
                                    grinWalletController.startReceiveWorkflow(amountField.text, noteField.text)
                                    root.syncWorkflowEditors()
                                }
                            }

                            Button {
                                text: root.tf("browser_wallet_process", "Process")
                                enabled: slatepackArea.text.trim().length > 0
                                onClicked: {
                                    grinWalletController.processWorkflowSlatepack(slatepackArea.text)
                                    root.syncWorkflowEditors()
                                }
                            }

                            Button {
                                text: root.tf("browser_wallet_clear", "Clear")
                                onClicked: {
                                    grinWalletController.clearWorkflow()
                                    slatepackArea.text = ""
                                    decodedArea.text = ""
                                    root.updateSlatepackStatus("")
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 16
                            color: "#122231"
                            border.color: root.slatepackStatusColor
                            implicitHeight: slatepackStatusLabel.implicitHeight + 20

                            Label {
                                id: slatepackStatusLabel
                                anchors.fill: parent
                                anchors.margins: 10
                                text: root.slatepackStatusText
                                color: root.slatepackStatusColor
                                wrapMode: Text.WordWrap
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.tf("browser_wallet_workflow_status", "Workflow") + ": "
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
                                    if (parsed.payment_proof_status) {
                                        return root.tf("browser_wallet_history_payment_proof", "Payment Proof") + ": " + parsed.payment_proof_status
                                    }
                                    if (parsed.proof) {
                                        return root.tf("browser_wallet_history_payment_proof", "Payment Proof") + ": "
                                             + (parsed.proof.rsig ? "receiver_signed" : "pending")
                                    }
                                } catch (error) {
                                }
                                return ""
                            }
                            color: {
                                try {
                                    var parsed = JSON.parse(decodedArea.text)
                                    if (parsed.payment_proof_status === "verified") return "#8ff0c8"
                                    if (parsed.payment_proof_status === "invalid") return "#ffb4b4"
                                    if (parsed.proof) return "#ffd280"
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
                                onActiveFocusChanged: root.syncBrowserShortcutContext(slatepackArea)
                                onSelectedTextChanged: root.syncBrowserShortcutContext(slatepackArea)
                                onTextChanged: root.syncBrowserShortcutContext(slatepackArea)
                                Keys.onPressed: function(event) { root.handleTextControlKeyPress(slatepackArea, event) }
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
                                placeholderText: root.tf("browser_wallet_slatepack_preview", "Decoded Slatepack preview")
                                onActiveFocusChanged: root.syncBrowserShortcutContext(decodedArea)
                                onSelectedTextChanged: root.syncBrowserShortcutContext(decodedArea)
                                onTextChanged: root.syncBrowserShortcutContext(decodedArea)
                                Keys.onPressed: function(event) { root.handleTextControlKeyPress(decodedArea, event) }
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    radius: 26
                    color: "#0f1b26"
                    border.color: "#26465b"
                    visible: root.activeSection === "settings"
                    implicitHeight: settingsColumn.implicitHeight + 34

                    ColumnLayout {
                        id: settingsColumn
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12

                        Label {
                            text: root.tf("browser_wallet_nav_settings", "Settings")
                            color: "#ffffff"
                            font.pixelSize: 28
                            font.weight: Font.Bold
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 16
                            color: "#132635"
                            border.color: "#2a4f64"
                            implicitHeight: overviewSettingsColumn.implicitHeight + 20

                            ColumnLayout {
                                id: overviewSettingsColumn
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 6

                                Label {
                                    Layout.fillWidth: true
                                    text: root.tf("browser_wallet_network_title", "Wallet Network")
                                    color: "#ffffff"
                                    wrapMode: Text.WordWrap
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: grinWalletController.selectedNetwork
                                    color: "#8ff0c8"
                                    wrapMode: Text.WordWrap
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: root.tf("browser_wallet_backup_export_note", "Export a password-encrypted wallet backup before moving devices or clearing browser storage. This backup keeps local history, scan state, and the encrypted seed together.")
                                    color: "#d7e9f4"
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 16
                            color: "#132635"
                            border.color: "#2a4f64"
                            implicitHeight: seedSettingsColumn.implicitHeight + 20

                            ColumnLayout {
                                id: seedSettingsColumn
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 8

                                Label {
                                    Layout.fillWidth: true
                                    text: root.tf("browser_wallet_seed_manage_title", "Seed Phrase")
                                    color: "#ffffff"
                                    wrapMode: Text.WordWrap
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: root.tf("browser_wallet_seed_manage_note", "Reveal the seed phrase only when you need to verify or back it up. Password confirmation is required every time.")
                                    color: "#d7e9f4"
                                    wrapMode: Text.WordWrap
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Button {
                                        text: root.tf("browser_wallet_seed_show", "Show Seed Phrase")
                                        onClicked: root.openRevealSeedPopup()
                                    }
                                    Button {
                                        text: root.tf("browser_wallet_seed_hide", "Hide Seed Phrase")
                                        enabled: grinWalletController.mnemonicPreview.length > 0
                                        onClicked: grinWalletController.dismissMnemonicPreview()
                                    }
                                    Item { Layout.fillWidth: true }
                                }

                                TextArea {
                                    id: settingsSeedArea
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: grinWalletController.mnemonicPreview.length > 0 ? 116 : 0
                                    visible: grinWalletController.mnemonicPreview.length > 0
                                    readOnly: true
                                    wrapMode: TextEdit.Wrap
                                    textFormat: TextEdit.PlainText
                                    selectByMouse: true
                                    persistentSelection: true
                                    activeFocusOnPress: true
                                    text: grinWalletController.mnemonicPreview
                                    onActiveFocusChanged: root.syncBrowserShortcutContext(settingsSeedArea)
                                    onSelectedTextChanged: root.syncBrowserShortcutContext(settingsSeedArea)
                                    onTextChanged: root.syncBrowserShortcutContext(settingsSeedArea)
                                    Keys.onPressed: function(event) { root.handleTextControlKeyPress(settingsSeedArea, event) }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 16
                            color: "#132635"
                            border.color: "#2a4f64"
                            implicitHeight: securitySettingsColumn.implicitHeight + 20

                            ColumnLayout {
                                id: securitySettingsColumn
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 8

                                Label {
                                    Layout.fillWidth: true
                                    text: root.tf("settings_security_title", "Security")
                                    color: "#ffffff"
                                    wrapMode: Text.WordWrap
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: root.tf("settings_auto_lock_note", "Lock the wallet automatically when the app loses focus or is minimized.")
                                    color: "#d7e9f4"
                                    wrapMode: Text.WordWrap
                                }

                                Switch {
                                    Layout.alignment: Qt.AlignLeft
                                    checked: grinWalletController ? grinWalletController.autoLockOnAppDeactivate : false
                                    text: root.tf("settings_auto_lock_label", "Lock on app exit/focus loss")

                                    onToggled: {
                                        if (grinWalletController)
                                            grinWalletController.setAutoLockOnAppDeactivate(checked)
                                    }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 16
                            color: "#132635"
                            border.color: root.storageStatusColor()
                            implicitHeight: storageColumn.implicitHeight + 20

                            ColumnLayout {
                                id: storageColumn
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 6

                                Label {
                                    Layout.fillWidth: true
                                    text: root.tf("browser_wallet_storage_title", "Storage Durability")
                                    color: "#ffffff"
                                    wrapMode: Text.WordWrap
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: root.storageStatusText()
                                    color: root.storageStatusColor()
                                    wrapMode: Text.WordWrap
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Button {
                                        text: root.tf("browser_wallet_storage_request", "Request Persistent Storage")
                                        enabled: grinWalletController.storagePersistenceState !== "native"
                                              && grinWalletController.storagePersistenceState !== "persistent"
                                        onClicked: grinWalletController.requestPersistentBrowserStorage()
                                    }
                                    Item { Layout.fillWidth: true }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 16
                            color: "#132635"
                            border.color: "#2a4f64"
                            implicitHeight: backupSettingsColumn.implicitHeight + 20

                            ColumnLayout {
                                id: backupSettingsColumn
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 8

                                Label {
                                    Layout.fillWidth: true
                                    text: root.tf("browser_wallet_backup_export", "Encrypted Backup")
                                    color: "#ffffff"
                                    wrapMode: Text.WordWrap
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: root.tf("browser_wallet_backup_download_note", "Download a fresh encrypted backup file for this wallet. Inline display and clipboard copy are intentionally not offered here.")
                                    color: "#d7e9f4"
                                    wrapMode: Text.WordWrap
                                }

                                RowLayout {
                                    Layout.fillWidth: true

                                    Button {
                                        text: root.tf("browser_wallet_backup_download", "Download Backup")
                                        enabled: grinWalletController.storagePersistenceState !== "native"
                                        onClicked: {
                                            var backup = grinWalletController.exportEncryptedWalletBackup()
                                            if (backup.trim().length > 0) {
                                                grinWalletController.downloadTextFile(
                                                    "grinffindor-wallet-backup-" + grinWalletController.selectedNetwork + ".json",
                                                    backup)
                                            }
                                        }
                                    }

                                    Item { Layout.fillWidth: true }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 16
                            color: "#132635"
                            border.color: "#2a4f64"
                            implicitHeight: nodeSettingsColumn.implicitHeight + 20

                            ColumnLayout {
                                id: nodeSettingsColumn
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 8

                                Label {
                                    Layout.fillWidth: true
                                    text: root.tf("browser_wallet_node_title", "External Node")
                                    color: "#ffffff"
                                    wrapMode: Text.WordWrap
                                }

                                TextField {
                                    id: nodeUrlField
                                    Layout.fillWidth: true
                                    text: root.nodeUrlDraft
                                    placeholderText: root.tf("browser_wallet_node_input", "https://your-node.example/v2/foreign")
                                    onActiveFocusChanged: root.syncBrowserShortcutContext(nodeUrlField)
                                    onSelectedTextChanged: root.syncBrowserShortcutContext(nodeUrlField)
                                    onTextChanged: {
                                        root.nodeUrlDraft = text
                                        root.syncBrowserShortcutContext(nodeUrlField)
                                    }
                                    Keys.onPressed: function(event) { root.handleTextControlKeyPress(nodeUrlField, event) }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: root.tf("browser_wallet_node_title", "External Node") + " (" + grinWalletController.selectedNetwork + "): " + grinWalletController.nodeUrl
                                    color: "#d7e9f4"
                                    wrapMode: Text.WrapAnywhere
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: root.tf("browser_wallet_node_note", "Use a Grin foreign API endpoint here. Switching the wallet network resets the node to the matching Grinffindor endpoint.")
                                    color: "#8fb4c9"
                                    wrapMode: Text.WordWrap
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Button {
                                        text: root.tf("browser_wallet_save_node", "Save Node")
                                        enabled: root.nodeUrlDraft.trim() !== grinWalletController.nodeUrl
                                                 && grinWalletController.isValidNodeUrl(root.nodeUrlDraft)
                                        onClicked: grinWalletController.setNodeUrl(root.nodeUrlDraft)
                                    }
                                    Button {
                                        text: root.tf("browser_wallet_reset_node", "Reset Node")
                                        enabled: grinWalletController.nodeUrl !== root.defaultNetworkNodeUrl()
                                        onClicked: grinWalletController.resetNodeUrl()
                                    }
                                    Button { text: root.tf("browser_wallet_refresh", "Refresh"); onClicked: grinWalletController.refreshNodeStatus() }
                                    Button { text: root.tf("browser_wallet_rescan", "Full Rescan"); enabled: root.nodeStatusMode() === "online"; onClicked: grinWalletController.rescanWallet() }
                                    Item { Layout.fillWidth: true }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 16
                            color: root.nodeStatusMode() === "offline" ? "#34191d"
                                 : (root.nodeStatusMode() === "connecting" ? "#2d2415" : "#132635")
                            border.color: root.recoveryBannerColor()
                            implicitHeight: settingsStatusColumn.implicitHeight + 20

                            ColumnLayout {
                                id: settingsStatusColumn
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 6

                                Label {
                                    Layout.fillWidth: true
                                    text: root.tf("browser_wallet_operational_status", "Operational Status") + ": " + grinWalletController.syncStatus
                                    color: "#ffffff"
                                    wrapMode: Text.WordWrap
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: root.recoveryBannerText().length > 0
                                          ? root.recoveryBannerText()
                                          : root.tf("browser_wallet_operational_ok", "Node is reachable and no pending recovery actions are currently flagged.")
                                    color: root.recoveryBannerText().length > 0 ? root.recoveryBannerColor() : "#8ff0c8"
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 16
                            color: "#132635"
                            border.color: "#2a4f64"
                            implicitHeight: balanceSettingsColumn.implicitHeight + 20

                            ColumnLayout {
                                id: balanceSettingsColumn
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 8

                                Label {
                                    Layout.fillWidth: true
                                    text: root.tf("browser_wallet_balances_title", "Wallet Balances")
                                    color: "#ffffff"
                                    wrapMode: Text.WordWrap
                                }

                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: 2
                                    rowSpacing: 10
                                    columnSpacing: 12

                                    Label { text: root.tf("browser_wallet_total", "Total"); color: "#8fb4c9" }
                                    Label { text: grinWalletController.totalBalance + " GRIN"; color: "#ffffff" }
                                    Label { text: root.tf("browser_wallet_spendable", "Spendable"); color: "#8fb4c9" }
                                    Label { text: grinWalletController.spendableBalance + " GRIN"; color: "#ffffff" }
                                    Label { text: root.tf("browser_wallet_locked", "Locked"); color: "#8fb4c9" }
                                    Label { text: grinWalletController.lockedBalance + " GRIN"; color: "#ffffff" }
                                    Label { text: root.tf("browser_wallet_immature", "Immature"); color: "#8fb4c9" }
                                    Label { text: grinWalletController.immatureBalance + " GRIN"; color: "#ffffff" }
                                    Label { text: root.tf("browser_wallet_awaiting_confirmation", "Awaiting Confirmation"); color: "#8fb4c9" }
                                    Label { text: grinWalletController.awaitingConfirmationBalance + " GRIN"; color: "#ffffff" }
                                    Label { text: root.tf("browser_wallet_awaiting_finalization", "Awaiting Finalization"); color: "#8fb4c9" }
                                    Label { text: grinWalletController.awaitingFinalizationBalance + " GRIN"; color: "#ffffff" }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 16
                            color: "#132635"
                            border.color: "#2a4f64"
                            implicitHeight: maintenanceSettingsColumn.implicitHeight + 20

                            ColumnLayout {
                                id: maintenanceSettingsColumn
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 8

                                Label {
                                    Layout.fillWidth: true
                                    text: root.tf("browser_wallet_maintenance_title", "Wallet Maintenance")
                                    color: "#ffffff"
                                    wrapMode: Text.WordWrap
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: root.tf("browser_wallet_maintenance_note", "Remove local (off-chain) UTXOs and cancelled transactions to clean up your wallet.")
                                    color: "#d7e9f4"
                                    wrapMode: Text.WordWrap
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: root.tf("browser_wallet_maintenance_warning", "This action is permanent. Confirmed UTXOs on the blockchain remain intact.")
                                    color: "#ffc8a8"
                                    wrapMode: Text.WordWrap
                                    font.pixelSize: 13
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Button {
                                        text: root.tf("browser_wallet_maintenance_cleanup", "Clean Up Now")
                                        enabled: grinWalletController.walletUnlocked
                                        onClicked: grinWalletController.cleanupLocalAndCancelledItems()
                                    }
                                    Item { Layout.fillWidth: true }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 16
                            color: "#34191d"
                            border.color: "#8b3c46"
                            implicitHeight: dangerSettingsColumn.implicitHeight + 20

                            ColumnLayout {
                                id: dangerSettingsColumn
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 8

                                Label {
                                    Layout.fillWidth: true
                                    text: root.tf("browser_wallet_danger_title", "Danger Zone")
                                    color: "#ffffff"
                                    wrapMode: Text.WordWrap
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: root.tf("browser_wallet_delete_note", "Delete the currently selected wallet only if you have verified your backup and seed phrase.")
                                    color: "#ffd6d6"
                                    wrapMode: Text.WordWrap
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Button { text: root.tf("browser_wallet_delete", "Delete Wallet"); onClicked: root.deleteConfirmOpen = true }
                                    Item { Layout.fillWidth: true }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: !grinWalletController.walletUnlocked
        color: "#05080dcc"
        z: 5

        MouseArea { anchors.fill: parent }

        Rectangle {
            width: Math.min(parent.width - 28, 640)
            anchors.centerIn: parent
            radius: 30
            color: "#0f1b26"
            border.color: "#2a4f64"
            implicitHeight: authColumn.implicitHeight + 38

            ColumnLayout {
                id: authColumn
                anchors.fill: parent
                anchors.margins: 20
                spacing: 14

                Label {
                    Layout.fillWidth: true
                    text: authMode === "unlock"
                          ? root.tf("browser_wallet_login_title", "Unlock Your Wallet")
                          : root.tf("browser_wallet_setup_title", "Set Up Your Wallet")
                    color: "#ffffff"
                    font.pixelSize: 30
                    font.weight: Font.Bold
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    text: authMode === "unlock"
                          ? root.tf("browser_wallet_login_note", "Enter your password to unlock the locally stored wallet for this browser session.")
                          : (authMode === "import_backup"
                             ? root.tf("browser_wallet_import_backup_note", "Paste a previously exported encrypted wallet backup JSON. The wallet stays locked after import until you unlock it with its password.")
                             : root.tf("browser_wallet_setup_note", "Create a new wallet with a fresh seed phrase or restore an existing wallet from its 24 words."))
                    color: "#d7e9f4"
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    visible: authMode !== "import_backup"
                    text: root.tf("browser_wallet_auth_network", "Active wallet network") + ": " + root.authNetworkDraft
                    color: "#8ff0c8"
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: authMode !== "import_backup"
                    spacing: 8

                    Button {
                        text: root.tf("browser_wallet_network_mainnet", "Mainnet")
                        highlighted: root.authNetworkDraft === "mainnet"
                        Layout.fillWidth: true
                        onClicked: {
                            root.authNetworkDraft = "mainnet"
                            grinWalletController.setSelectedNetwork("mainnet")
                        }
                    }

                    Button {
                        text: root.tf("browser_wallet_network_testnet", "Testnet")
                        highlighted: root.authNetworkDraft === "testnet"
                        Layout.fillWidth: true
                        onClicked: {
                            root.authNetworkDraft = "testnet"
                            grinWalletController.setSelectedNetwork("testnet")
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: !grinWalletController.walletExists
                    spacing: 8

                    Button {
                        text: root.tf("browser_wallet_create_mode", "New Wallet")
                        highlighted: authMode === "create"
                        Layout.fillWidth: true
                        onClicked: {
                            authMode = "create"
                            root.restoreMnemonicDraft = ""
                            root.backupImportDraft = ""
                        }
                    }

                    Button {
                        text: root.tf("browser_wallet_restore_mode", "Restore Seed")
                        highlighted: authMode === "restore"
                        Layout.fillWidth: true
                        onClicked: {
                            authMode = "restore"
                            root.backupImportDraft = ""
                        }
                    }

                    Button {
                        text: root.tf("browser_wallet_import_backup", "Import Backup")
                        highlighted: authMode === "import_backup"
                        Layout.fillWidth: true
                        onClicked: {
                            authMode = "import_backup"
                            root.restoreMnemonicDraft = ""
                        }
                    }
                }

                TextField {
                    id: walletNameField
                    Layout.fillWidth: true
                    visible: authMode !== "unlock" && authMode !== "import_backup"
                    text: walletPageSettings.walletNameDraft
                    placeholderText: root.tf("browser_wallet_name_placeholder", "Wallet name")
                    onActiveFocusChanged: root.syncBrowserShortcutContext(walletNameField)
                    onSelectedTextChanged: root.syncBrowserShortcutContext(walletNameField)
                    onTextChanged: {
                        walletPageSettings.walletNameDraft = text
                        root.syncBrowserShortcutContext(walletNameField)
                    }
                    Keys.onPressed: function(event) { root.handleTextControlKeyPress(walletNameField, event) }
                }

                TextArea {
                    id: restoreMnemonicArea
                    Layout.fillWidth: true
                    Layout.preferredHeight: authMode === "restore" ? 132 : 0
                    visible: authMode === "restore"
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    persistentSelection: true
                    activeFocusOnPress: true
                    text: root.restoreMnemonicDraft
                    placeholderText: root.tf("browser_wallet_mnemonic_placeholder", "24-word mnemonic for importing an existing wallet.")
                    onActiveFocusChanged: root.syncBrowserShortcutContext(restoreMnemonicArea)
                    onSelectedTextChanged: root.syncBrowserShortcutContext(restoreMnemonicArea)
                    onTextChanged: {
                        root.restoreMnemonicDraft = text
                        root.syncBrowserShortcutContext(restoreMnemonicArea)
                    }
                    Keys.onPressed: function(event) { root.handleTextControlKeyPress(restoreMnemonicArea, event) }
                }

                TextArea {
                    id: importBackupArea
                    Layout.fillWidth: true
                    Layout.preferredHeight: authMode === "import_backup" ? 176 : 0
                    visible: authMode === "import_backup"
                    wrapMode: TextEdit.WrapAnywhere
                    selectByMouse: true
                    persistentSelection: true
                    activeFocusOnPress: true
                    text: root.backupImportDraft
                    placeholderText: root.tf("browser_wallet_backup_import_placeholder", "Paste encrypted wallet backup JSON here.")
                    onActiveFocusChanged: root.syncBrowserShortcutContext(importBackupArea)
                    onSelectedTextChanged: root.syncBrowserShortcutContext(importBackupArea)
                    onTextChanged: {
                        root.backupImportDraft = text
                        root.syncBrowserShortcutContext(importBackupArea)
                    }
                    Keys.onPressed: function(event) { root.handleTextControlKeyPress(importBackupArea, event) }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: authMode === "restore"
                    Item { Layout.fillWidth: true }
                    Button {
                        text: root.tf("browser_wallet_paste", "Paste")
                        onClicked: {
                            root.openPasteDialog(
                                restoreMnemonicArea,
                                root.tf("browser_wallet_paste_restore_title", "Paste Seed Phrase"),
                                root.tf("browser_wallet_mnemonic_placeholder", "24-word mnemonic for importing an existing wallet."))
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: authMode === "import_backup"
                    Item { Layout.fillWidth: true }
                    Button {
                        text: root.tf("browser_wallet_paste", "Paste")
                        onClicked: {
                            root.openPasteDialog(
                                importBackupArea,
                                root.tf("browser_wallet_paste_backup_title", "Paste Encrypted Backup"),
                                root.tf("browser_wallet_backup_import_placeholder", "Paste encrypted wallet backup JSON here."))
                        }
                    }
                }

                TextField {
                    id: unlockPasswordField
                    Layout.fillWidth: true
                    visible: authMode === "unlock"
                    echoMode: TextInput.Password
                    text: root.unlockPasswordDraft
                    placeholderText: root.tf("browser_wallet_password_placeholder", "Encryption password")
                    onActiveFocusChanged: root.syncBrowserShortcutContext(unlockPasswordField)
                    onSelectedTextChanged: root.syncBrowserShortcutContext(unlockPasswordField)
                    onTextChanged: {
                        root.unlockPasswordDraft = text
                        root.syncBrowserShortcutContext(unlockPasswordField)
                    }
                    onAccepted: root.submitAuth()
                    Keys.onPressed: function(event) { root.handleTextControlKeyPress(unlockPasswordField, event) }
                }

                TextField {
                    id: createPasswordField
                    Layout.fillWidth: true
                    visible: authMode !== "unlock" && authMode !== "import_backup"
                    echoMode: TextInput.Password
                    text: root.passwordDraft
                    placeholderText: root.tf("browser_wallet_password_placeholder", "Encryption password")
                    onActiveFocusChanged: root.syncBrowserShortcutContext(createPasswordField)
                    onSelectedTextChanged: root.syncBrowserShortcutContext(createPasswordField)
                    onTextChanged: {
                        root.passwordDraft = text
                        root.syncBrowserShortcutContext(createPasswordField)
                    }
                    Keys.onPressed: function(event) { root.handleTextControlKeyPress(createPasswordField, event) }
                }

                TextField {
                    id: confirmPasswordField
                    Layout.fillWidth: true
                    visible: authMode !== "unlock" && authMode !== "import_backup"
                    echoMode: TextInput.Password
                    text: root.passwordConfirmDraft
                    placeholderText: root.tf("browser_wallet_confirm_password_placeholder", "Confirm password")
                    onActiveFocusChanged: root.syncBrowserShortcutContext(confirmPasswordField)
                    onSelectedTextChanged: root.syncBrowserShortcutContext(confirmPasswordField)
                    onTextChanged: {
                        root.passwordConfirmDraft = text
                        root.syncBrowserShortcutContext(confirmPasswordField)
                    }
                    onAccepted: root.submitAuth()
                    Keys.onPressed: function(event) { root.handleTextControlKeyPress(confirmPasswordField, event) }
                }

                Label {
                    Layout.fillWidth: true
                    visible: authMode !== "unlock" && authMode !== "import_backup" && root.passwordDraft.length > 0
                    text: root.passwordDraft === root.passwordConfirmDraft
                          ? root.tf("browser_wallet_password_match", "Passwords match.")
                          : root.tf("browser_wallet_password_no_match", "Passwords do not match yet.")
                    color: root.passwordDraft === root.passwordConfirmDraft ? "#8ff0c8" : "#ffb4b4"
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    visible: false
                    text: grinWalletController.lastError
                    color: "#ffb4b4"
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    visible: grinWalletController.lastInfo.length > 0
                    text: grinWalletController.lastInfo
                    color: "#8ff0c8"
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    Button {
                        visible: grinWalletController.walletExists
                        text: root.tf("browser_wallet_delete", "Delete Wallet")
                        onClicked: root.deleteConfirmOpen = true
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        text: authMode === "unlock"
                              ? root.tf("browser_wallet_unlock", "Unlock")
                              : (authMode === "create"
                                 ? root.tf("browser_wallet_create", "Create")
                                 : (authMode === "restore"
                                    ? root.tf("browser_wallet_restore", "Restore")
                                    : root.tf("browser_wallet_import_backup", "Import Backup")))
                        enabled: authMode === "unlock"
                                 ? root.unlockPasswordDraft.length > 0
                                 : (authMode === "import_backup"
                                    ? root.backupImportDraft.trim().length > 0
                                    : (walletPageSettings.walletNameDraft.trim().length > 0
                                       && root.passwordDraft.length > 0
                                       && root.passwordDraft === root.passwordConfirmDraft
                                       && (authMode !== "restore" || root.restoreMnemonicDraft.trim().length > 0)))
                        onClicked: root.submitAuth()
                    }
                }
            }
        }
    }

    Popup {
        id: pasteInputPopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: Math.min(root.width - 28, 760)
        padding: 0
        onClosed: {
            root.pasteTargetControl = null
            root.pasteDialogTitle = ""
            root.pasteDialogPlaceholder = ""
            root.pasteDialogText = ""
        }
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
                text: root.pasteDialogTitle.length > 0
                      ? root.pasteDialogTitle
                      : root.tf("browser_wallet_paste_generic_title", "Paste Text")
                color: "#ffffff"
                font.pixelSize: 28
                font.weight: Font.Bold
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                text: root.tf("browser_wallet_paste_generic_note", "Enter or paste the full content below. Multi-line input is supported.")
                color: "#d7e9f4"
                wrapMode: Text.WordWrap
            }

            TextArea {
                id: pasteInputArea
                Layout.fillWidth: true
                Layout.preferredHeight: 260
                wrapMode: TextEdit.WrapAnywhere
                selectByMouse: true
                persistentSelection: true
                activeFocusOnPress: true
                text: root.pasteDialogText
                placeholderText: root.pasteDialogPlaceholder
                onActiveFocusChanged: root.syncBrowserShortcutContext(pasteInputArea)
                onSelectedTextChanged: root.syncBrowserShortcutContext(pasteInputArea)
                onTextChanged: {
                    root.pasteDialogText = text
                    root.syncBrowserShortcutContext(pasteInputArea)
                }
                Keys.onPressed: function(event) { root.handleTextControlKeyPress(pasteInputArea, event) }
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: root.tf("browser_wallet_cancel", "Cancel")
                    onClicked: pasteInputPopup.close()
                }
                Button {
                    text: root.tf("browser_wallet_apply_paste", "Apply")
                    enabled: root.pasteDialogText.trim().length > 0
                    onClicked: root.applyPasteDialog()
                }
            }
        }
    }

    Popup {
        id: errorPopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: Math.min(root.width - 28, 620)
        padding: 0
        onClosed: grinWalletController.clearLastError()
        background: Rectangle {
            radius: 28
            color: "#221216"
            border.color: "#c65b5b"
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            Label {
                Layout.fillWidth: true
                text: root.tf("browser_wallet_error_title", "Wallet Error")
                color: "#ffffff"
                font.pixelSize: 28
                font.weight: Font.Bold
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                text: root.errorDialogText
                color: "#ffd7d7"
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: root.tf("browser_wallet_error_close", "Close")
                    onClicked: errorPopup.close()
                }
            }
        }
    }

    Popup {
        id: revealSeedPopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: Math.min(root.width - 28, 520)
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
                text: root.tf("browser_wallet_seed_prompt_title", "Reveal Seed Phrase")
                color: "#ffffff"
                font.pixelSize: 28
                font.weight: Font.Bold
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                text: root.tf("browser_wallet_seed_prompt_note", "Enter your wallet password to decrypt and show the seed phrase for the active network wallet.")
                color: "#d7e9f4"
                wrapMode: Text.WordWrap
            }

            TextField {
                id: revealSeedPasswordField
                Layout.fillWidth: true
                echoMode: TextInput.Password
                text: root.revealSeedPasswordDraft
                placeholderText: root.tf("browser_wallet_password_placeholder", "Encryption password")
                onActiveFocusChanged: root.syncBrowserShortcutContext(revealSeedPasswordField)
                onSelectedTextChanged: root.syncBrowserShortcutContext(revealSeedPasswordField)
                onTextChanged: {
                    root.revealSeedPasswordDraft = text
                    root.syncBrowserShortcutContext(revealSeedPasswordField)
                }
                onAccepted: {
                    if (grinWalletController.revealSeedPhrase(root.revealSeedPasswordDraft)) {
                        root.revealSeedPasswordDraft = ""
                        revealSeedPopup.close()
                    }
                }
                Keys.onPressed: function(event) { root.handleTextControlKeyPress(revealSeedPasswordField, event) }
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: root.tf("browser_wallet_cancel", "Cancel")
                    onClicked: {
                        root.revealSeedPasswordDraft = ""
                        revealSeedPopup.close()
                    }
                }
                Button {
                    text: root.tf("browser_wallet_seed_show", "Show Seed Phrase")
                    enabled: root.revealSeedPasswordDraft.length > 0
                    onClicked: {
                        if (grinWalletController.revealSeedPhrase(root.revealSeedPasswordDraft)) {
                            root.revealSeedPasswordDraft = ""
                            revealSeedPopup.close()
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: deleteWalletPopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        visible: root.deleteConfirmOpen
        onClosed: root.deleteConfirmOpen = false
        width: Math.min(root.width - 28, 560)
        padding: 0
        background: Rectangle {
            radius: 28
            color: "#0f1b26"
            border.color: "#8a3f3f"
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            Label {
                Layout.fillWidth: true
                text: root.tf("browser_wallet_delete_title", "Delete Local Wallet Configuration?")
                color: "#ffffff"
                font.pixelSize: 28
                font.weight: Font.Bold
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                text: root.tf("browser_wallet_delete_note", "This removes the encrypted wallet, local history, and sync state from this browser. Make sure you still have the seed phrase before continuing.")
                color: "#ffd3d3"
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true
                Button { text: root.tf("browser_wallet_cancel_delete", "Cancel"); onClicked: deleteWalletPopup.close() }
                Item { Layout.fillWidth: true }
                Button {
                    text: root.tf("browser_wallet_delete_and_restore", "Delete and Restore")
                    onClicked: {
                        deleteWalletPopup.close()
                        root.beginRestoreReset()
                    }
                }
            }
        }
    }

    Connections {
        target: grinWalletController

        function onWorkflowChanged() {
            root.syncWorkflowEditors()
        }

        function onWalletChanged() {
            root.syncAuthMode()
            root.syncNodeDraft()
            root.syncAuthNetworkDraft()
            if (grinWalletController.walletName.length > 0)
                walletPageSettings.walletNameDraft = grinWalletController.walletName
            else if (!grinWalletController.walletExists)
                walletPageSettings.walletNameDraft = ""

            if (grinWalletController.walletUnlocked) {
                root.restoreMnemonicDraft = ""
                root.backupImportDraft = ""
                root.clearPasswordDrafts()
            } else {
                root.unlockPasswordDraft = ""
            }

            if (!grinWalletController.walletExists && root.authMode === "unlock")
                root.authMode = "restore"
        }

        function onLastErrorChanged() {
            if (grinWalletController.lastError.length > 0)
                root.showErrorDialog(grinWalletController.lastError)
        }

        function onNodeConfigChanged() {
            root.syncNodeDraft()
            root.syncAuthNetworkDraft()
        }
    }

    Component.onCompleted: {
        grinWalletController.initialize()
        root.syncAuthMode()
        root.syncNodeDraft()
        root.syncAuthNetworkDraft()
        slatepackArea.text = ""
        decodedArea.text = ""
        root.updateSlatepackStatus("")
    }
}
