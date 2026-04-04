import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import "browserwallet" as BrowserWalletParts

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
    property bool compactNavigation: width < 920
    property color slatepackStatusColor: "#8fb4c9"
    property var slatepackEditor: null
    property var decodedEditor: null

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
            syncBrowserShortcutContext(pasteTargetControl)
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
            return tf("browser_wallet_storage_persistent", "Browser storage is persistent.")
        if (grinWalletController.storagePersistenceState === "best-effort")
            return tf("browser_wallet_storage_best_effort", "Browser storage is best-effort only. Export a backup and request persistent storage on this device.")
        if (grinWalletController.storagePersistenceState === "native")
            return tf("browser_wallet_storage_native", "Desktop storage is handled by the local app data directory.")
        return tf("browser_wallet_storage_unknown", "Browser storage status has not been confirmed yet.")
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
        if (nodeStatusMode() === "offline")
            return tf("browser_wallet_recovery_offline", "The wallet cannot currently reach the external node. Balances and broadcast recovery may be stale until the node responds again.")
        if (nodeStatusMode() === "connecting")
            return tf("browser_wallet_recovery_connecting", "The wallet is still querying the node. Wait for a tip update before starting sends or trusting the displayed chain state.")
        if (count > 0)
            return tf("browser_wallet_recovery_pending", "Recovery check active for ") + count + tf("browser_wallet_recovery_pending_suffix", " transaction(s). Refresh the node after reconnecting so broadcast and mempool status can be reconciled.")
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
            return tf("browser_wallet_history_recovery_pending", "Broadcast was submitted locally and is waiting for node confirmation. Use Refresh after reconnecting to reconcile it.")
        if (status === "broadcast_failed")
            return tf("browser_wallet_history_recovery_failed", "Node broadcast failed. Check node connectivity, then retry broadcast for this workflow.")
        if (status === "in_mempool")
            return tf("browser_wallet_history_recovery_mempool", "Transaction is seen in the mempool but not confirmed yet. Keep the node online until kernel confirmation arrives.")
        if (status === "broadcasted")
            return tf("browser_wallet_history_recovery_broadcasted", "Transaction was broadcast and is awaiting mempool or kernel confirmation. A refresh or reload will continue status recovery.")
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
            setSlatepackStatus(tf("browser_wallet_slatepack_status_idle", "Paste a Slatepack, decode it, or start a SEND/RECEIVE workflow."), "#8fb4c9")
            return
        }

        try {
            var parsed = JSON.parse(trimmed)
            if (parsed.encrypted_slatepack) {
                setSlatepackStatus(parsed.note || tf("browser_wallet_slatepack_status_encrypted", "Encrypted Slatepack detected. Unlock the matching wallet to decrypt it."), "#ffd280")
                return
            }
            if (parsed.external_slatepack) {
                setSlatepackStatus(parsed.note || tf("browser_wallet_slatepack_status_invalid", "Slatepack payload could not be parsed."), "#ffb4b4")
                return
            }

            var state = parsed.state || parsed.sta || "-"
            var workflowId = parsed.id || parsed.workflow_id || "-"
            var mode = parsed.body_type || parsed.mode || ((state && state.indexOf("I") === 0) ? "invoice" : ((state && state.indexOf("S") === 0) ? "send" : "-"))
            setSlatepackStatus(tf("browser_wallet_slatepack_status_ready", "Decoded Slatepack ready") + ": " + mode + " / " + state + " / " + workflowId, "#8ff0c8")
            return
        } catch (error) {
        }

        setSlatepackStatus(tf("browser_wallet_slatepack_status_preview", "Decoded text preview loaded."), "#8fb4c9")
    }
    function syncWorkflowEditors() {
        if (slatepackEditor)
            slatepackEditor.text = grinWalletController.workflowSlatepack
        if (decodedEditor)
            decodedEditor.text = grinWalletController.workflowDecoded
        updateSlatepackStatus(decodedEditor ? decodedEditor.text : grinWalletController.workflowDecoded)
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
        if (walletName.trim().length === 0)
            return
        if (createPassword.length === 0 || createPassword !== passwordConfirmDraft)
            return
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

    BrowserWalletParts.BrowserWalletTopBar {
        id: topBar
        walletRoot: root
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        onMenuRequested: mobileSidebarDrawer.open()
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

        BrowserWalletParts.BrowserWalletSidebar {
            walletRoot: root
            visible: !root.compactNavigation
            Layout.preferredWidth: Math.min(300, root.width * 0.28)
            Layout.fillHeight: true
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

                BrowserWalletParts.BrowserWalletHomeSection {
                    width: parent.width
                    visible: root.activeSection === "home"
                    walletRoot: root
                }

                BrowserWalletParts.BrowserWalletUtxoSection {
                    width: parent.width
                    visible: root.activeSection === "utxos"
                    walletRoot: root
                }

                BrowserWalletParts.BrowserWalletSlatepackSection {
                    width: parent.width
                    visible: root.activeSection === "slatepack"
                    walletRoot: root
                    walletPageSettings: walletPageSettings
                }

                BrowserWalletParts.BrowserWalletSettingsSection {
                    width: parent.width
                    visible: root.activeSection === "settings"
                    walletRoot: root
                }
            }
        }
    }

    Drawer {
        id: mobileSidebarDrawer
        edge: Qt.LeftEdge
        modal: true
        interactive: root.compactNavigation
        enabled: root.compactNavigation && grinWalletController.walletUnlocked
        width: Math.min(root.width * 0.82, 340)
        height: root.height
        topMargin: topBar.height
        background: Rectangle {
            color: "#09121a"
        }

        BrowserWalletParts.BrowserWalletSidebar {
            anchors.fill: parent
            walletRoot: root
            compactMode: true
            onSectionSelected: mobileSidebarDrawer.close()
        }
    }

    BrowserWalletParts.BrowserWalletAuthPanel {
        anchors.fill: parent
        visible: !grinWalletController.walletUnlocked
        z: 5
        walletRoot: root
        walletPageSettings: walletPageSettings
    }

    BrowserWalletParts.BrowserWalletPastePopup {
        id: pasteInputPopup
        walletRoot: root
    }

    BrowserWalletParts.BrowserWalletErrorPopup {
        id: errorPopup
        walletRoot: root
    }

    BrowserWalletParts.BrowserWalletRevealSeedPopup {
        id: revealSeedPopup
        walletRoot: root
    }

    BrowserWalletParts.BrowserWalletDeletePopup {
        id: deleteWalletPopup
        walletRoot: root
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
                root.syncWorkflowEditors()
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
        root.updateSlatepackStatus("")
    }

    onCompactNavigationChanged: {
        if (!compactNavigation)
            mobileSidebarDrawer.close()
    }
}