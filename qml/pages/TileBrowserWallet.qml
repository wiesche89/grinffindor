import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore

Item {
    id: root
    property var i18n: null
    property string authMode: "unlock"
    property string activeSection: "home"
    property bool deleteConfirmOpen: false
    property string restoreMnemonicDraft: ""
    property string unlockPasswordDraft: ""
    property string passwordDraft: ""
    property string passwordConfirmDraft: ""
    property string nodeUrlDraft: ""
    property string slatepackStatusText: ""
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
    function setSlatepackStatus(text, color) {
        slatepackStatusText = text
        slatepackStatusColor = color || "#8fb4c9"
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
        clearPasswordDrafts()
    }
    function submitAuth() {
        if (authMode === "unlock") {
            grinWalletController.unlockWallet(unlockPasswordDraft)
            return
        }
        if (walletPageSettings.walletNameDraft.trim().length === 0) return
        if (passwordDraft.length === 0 || passwordDraft !== passwordConfirmDraft) return
        if (authMode === "create") {
            grinWalletController.createWallet(walletPageSettings.walletNameDraft, passwordDraft)
            return
        }
        grinWalletController.restoreWallet(walletPageSettings.walletNameDraft, restoreMnemonicDraft, passwordDraft)
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
                text: root.tf("browser_wallet_title", "Grin Browser Wallet")
                color: "#f7fbff"
                font.pixelSize: 28
                font.weight: Font.Bold
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                radius: 14
                color: "#122231"
                border.color: "#2f607a"
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
                    radius: 28
                    color: "#102131"
                    border.color: "#29516a"
                    visible: root.activeSection === "home"
                    implicitHeight: overviewColumn.implicitHeight + 36

                    Column {
                        id: overviewColumn
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12

                        Label {
                            width: parent.width
                            text: root.tf("browser_wallet_overview_title", "Overview")
                            color: "#ffffff"
                            font.pixelSize: 30
                            font.weight: Font.Bold
                        }

                        Label {
                            width: parent.width
                            text: root.tf("browser_wallet_intro", "A self-custodial Grin wallet shell for Qt WASM. Seed and slate handling stay local in the browser runtime, while chain data comes from an external node.")
                            color: "#d7e9f4"
                            wrapMode: Text.WordWrap
                            font.pixelSize: 17
                        }

                        GridLayout {
                            width: parent.width
                            columns: width < 760 ? 1 : 4
                            rowSpacing: 12
                            columnSpacing: 12

                            Repeater {
                                model: [
                                    { title: root.tf("browser_wallet_metric_wallet", "Wallet"), value: grinWalletController.walletExists ? grinWalletController.walletName : root.tf("browser_wallet_metric_empty", "Not created") },
                                    { title: root.tf("browser_wallet_metric_chain", "Chain Height"), value: "" + grinWalletController.chainHeight },
                                    { title: root.tf("browser_wallet_metric_balance", "Spendable"), value: grinWalletController.spendableBalance + " GRIN" },
                                    { title: root.tf("browser_wallet_metric_scan", "Scan Height"), value: "" + grinWalletController.scanHeight }
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
                            Layout.fillWidth: true
                            Layout.preferredHeight: 116
                            readOnly: true
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true
                            persistentSelection: true
                            text: grinWalletController.mnemonicPreview
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
                                        color: "#8ff0c8"
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

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Button {
                                            text: root.tf("browser_wallet_history_broadcast", "Broadcast")
                                            enabled: modelData.tx_ready === true && modelData.broadcasted !== true && modelData.status !== "cancelled"
                                            onClicked: grinWalletController.broadcastTransaction(modelData.workflow_id)
                                        }
                                        Button {
                                            text: root.tf("browser_wallet_history_cancel", "Cancel")
                                            enabled: modelData.status !== "cancelled"
                                                     && modelData.status !== "confirmed"
                                                     && (modelData.confirmations === undefined || modelData.confirmations < 1)
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

                        GridLayout {
                            Layout.fillWidth: true
                            columns: width < 760 ? 1 : 2
                            rowSpacing: 12
                            columnSpacing: 12

                            TextField {
                                id: amountField
                                Layout.fillWidth: true
                                text: walletPageSettings.amountDraft
                                placeholderText: root.tf("browser_wallet_amount_placeholder", "Amount, e.g. 1.000000000")
                                onTextChanged: walletPageSettings.amountDraft = text
                            }

                            TextField {
                                id: noteField
                                Layout.fillWidth: true
                                text: walletPageSettings.noteDraft
                                placeholderText: root.tf("browser_wallet_note_placeholder", "Optional note")
                                onTextChanged: walletPageSettings.noteDraft = text
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Button {
                                text: root.tf("browser_wallet_paste_slatepack", "Paste Slatepack")
                                onClicked: {
                                    slatepackArea.forceActiveFocus()
                                    var pastedSlatepack = grinWalletController.requestPasteText()
                                    if (pastedSlatepack.length > 0) slatepackArea.text = pastedSlatepack
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

                        ScrollView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 320
                            clip: true

                            TextArea {
                                id: slatepackArea
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
                            }
                        }

                        Shortcut {
                            sequences: [StandardKey.Paste, "Ctrl+V", "Shift+Insert"]
                            onActivated: {
                                if (slatepackArea.activeFocus) {
                                    var pastedSlatepack = grinWalletController.requestPasteText()
                                    if (pastedSlatepack.length > 0) slatepackArea.text = pastedSlatepack
                                }
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
                                text: ""
                                placeholderText: root.tf("browser_wallet_slatepack_preview", "Decoded Slatepack preview")
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

                        TextField {
                            Layout.fillWidth: true
                            text: root.nodeUrlDraft
                            placeholderText: root.tf("browser_wallet_node_input", "https://your-node.example/v2/foreign")
                            onTextChanged: root.nodeUrlDraft = text
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.tf("browser_wallet_node_title", "External Node") + ": " + grinWalletController.nodeUrl
                            color: "#d7e9f4"
                            wrapMode: Text.WrapAnywhere
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.tf("browser_wallet_node_note", "Use a Grin foreign API endpoint here. The wallet reconnects immediately after saving the URL.")
                            color: "#8fb4c9"
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
                                enabled: grinWalletController.nodeUrl !== "https://mainnet.grinffindor.org/v2/foreign"
                                onClicked: grinWalletController.resetNodeUrl()
                            }
                            Button { text: root.tf("browser_wallet_refresh", "Refresh"); onClicked: grinWalletController.refreshNodeStatus() }
                            Button { text: root.tf("browser_wallet_rescan", "Full Rescan"); onClicked: grinWalletController.rescanWallet() }
                            Item { Layout.fillWidth: true }
                            Button { text: root.tf("browser_wallet_delete", "Delete Wallet"); onClicked: root.deleteConfirmOpen = true }
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
                          : root.tf("browser_wallet_setup_note", "Create a new wallet with a fresh seed phrase or restore an existing wallet from its 24 words.")
                    color: "#d7e9f4"
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: !grinWalletController.walletExists
                    spacing: 8

                    Button {
                        text: root.tf("browser_wallet_create", "Create")
                        highlighted: authMode === "create"
                        Layout.fillWidth: true
                        onClicked: {
                            authMode = "create"
                            root.restoreMnemonicDraft = ""
                        }
                    }

                    Button {
                        text: root.tf("browser_wallet_restore", "Restore")
                        highlighted: authMode === "restore"
                        Layout.fillWidth: true
                        onClicked: authMode = "restore"
                    }
                }

                TextField {
                    Layout.fillWidth: true
                    visible: authMode !== "unlock"
                    text: walletPageSettings.walletNameDraft
                    placeholderText: root.tf("browser_wallet_name_placeholder", "Wallet name")
                    onTextChanged: walletPageSettings.walletNameDraft = text
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
                    onTextChanged: root.restoreMnemonicDraft = text
                }

                Shortcut {
                    sequences: [StandardKey.Paste, "Ctrl+V", "Shift+Insert"]
                    onActivated: {
                        if (restoreMnemonicArea.activeFocus) {
                            var pastedText = grinWalletController.requestPasteText()
                            if (pastedText.length > 0) restoreMnemonicArea.text = pastedText
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: authMode === "restore"
                    Item { Layout.fillWidth: true }
                    Button {
                        text: root.tf("browser_wallet_paste", "Paste")
                        onClicked: {
                            restoreMnemonicArea.forceActiveFocus()
                            var pastedText = grinWalletController.requestPasteText()
                            if (pastedText.length > 0) restoreMnemonicArea.text = pastedText
                        }
                    }
                }

                TextField {
                    Layout.fillWidth: true
                    visible: authMode === "unlock"
                    echoMode: TextInput.Password
                    text: root.unlockPasswordDraft
                    placeholderText: root.tf("browser_wallet_password_placeholder", "Encryption password")
                    onTextChanged: root.unlockPasswordDraft = text
                    onAccepted: root.submitAuth()
                }

                TextField {
                    Layout.fillWidth: true
                    visible: authMode !== "unlock"
                    echoMode: TextInput.Password
                    text: root.passwordDraft
                    placeholderText: root.tf("browser_wallet_password_placeholder", "Encryption password")
                    onTextChanged: root.passwordDraft = text
                }

                TextField {
                    Layout.fillWidth: true
                    visible: authMode !== "unlock"
                    echoMode: TextInput.Password
                    text: root.passwordConfirmDraft
                    placeholderText: root.tf("browser_wallet_confirm_password_placeholder", "Confirm password")
                    onTextChanged: root.passwordConfirmDraft = text
                    onAccepted: root.submitAuth()
                }

                Label {
                    Layout.fillWidth: true
                    visible: authMode !== "unlock" && root.passwordDraft.length > 0
                    text: root.passwordDraft === root.passwordConfirmDraft
                          ? root.tf("browser_wallet_password_match", "Passwords match.")
                          : root.tf("browser_wallet_password_no_match", "Passwords do not match yet.")
                    color: root.passwordDraft === root.passwordConfirmDraft ? "#8ff0c8" : "#ffb4b4"
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    visible: grinWalletController.lastError.length > 0
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
                              : (authMode === "create" ? root.tf("browser_wallet_create", "Create") : root.tf("browser_wallet_restore", "Restore"))
                        enabled: authMode === "unlock"
                                 ? root.unlockPasswordDraft.length > 0
                                 : (walletPageSettings.walletNameDraft.trim().length > 0
                                    && root.passwordDraft.length > 0
                                    && root.passwordDraft === root.passwordConfirmDraft
                                    && (authMode !== "restore" || root.restoreMnemonicDraft.trim().length > 0))
                        onClicked: root.submitAuth()
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
            if (grinWalletController.walletName.length > 0)
                walletPageSettings.walletNameDraft = grinWalletController.walletName
            else if (!grinWalletController.walletExists)
                walletPageSettings.walletNameDraft = ""

            if (grinWalletController.walletUnlocked) {
                root.restoreMnemonicDraft = ""
                root.clearPasswordDrafts()
            } else {
                root.unlockPasswordDraft = ""
            }

            if (!grinWalletController.walletExists && root.authMode === "unlock")
                root.authMode = "restore"
        }

        function onNodeConfigChanged() {
            root.syncNodeDraft()
        }
    }

    Component.onCompleted: {
        grinWalletController.initialize()
        root.syncAuthMode()
        root.syncNodeDraft()
        slatepackArea.text = ""
        decodedArea.text = ""
        root.updateSlatepackStatus("")
    }
}
