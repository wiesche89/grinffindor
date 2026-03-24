import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore

Item {
    id: root
    property var i18n: null
    signal backRequested()

    anchors.fill: parent

    function tf(key, fallback) {
        return i18n ? i18n.tf(key, fallback) : fallback
    }

    Settings {
        id: walletPageSettings
        category: "browserWalletPage"
        property string walletNameDraft: ""
        property string mnemonicDraft: ""
        property string amountDraft: ""
        property string noteDraft: ""
    }

    Rectangle {
        anchors.fill: parent
        color: "#091018"
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#10273a" }
            GradientStop { position: 0.45; color: "#091018" }
            GradientStop { position: 1.0; color: "#05080d" }
        }
    }

    Rectangle {
        width: parent.width * 0.48
        height: width
        radius: width / 2
        x: parent.width * 0.58
        y: -height * 0.16
        color: "#2ad4ff"
        opacity: 0.05
    }

    Rectangle {
        width: parent.width * 0.42
        height: width
        radius: width / 2
        x: -width * 0.2
        y: parent.height * 0.44
        color: "#4ade80"
        opacity: 0.04
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
                text: root.tf("back", "Back")
                onClicked: root.backRequested()
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

        Column {
            width: Math.min(parent.width - 140, 780)
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 44
            spacing: 4

            Label {
                width: parent.width
                visible: grinWalletController.lastInfo.length > 0
                text: grinWalletController.lastInfo
                wrapMode: Text.WordWrap
                color: "#8ff0c8"
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                width: parent.width
                visible: grinWalletController.lastError.length > 0
                text: grinWalletController.lastError
                wrapMode: Text.WordWrap
                color: "#ffb4b4"
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    Flickable {
        anchors.fill: parent
        anchors.topMargin: topBar.height
        contentWidth: width
        contentHeight: contentColumn.implicitHeight + 40
        clip: true

        ScrollBar.vertical: ScrollBar {}

        Column {
            id: contentColumn
            width: Math.min(parent.width - 32, 1120)
            x: Math.round((parent.width - width) / 2)
            y: 18
            spacing: 18

            Rectangle {
                width: parent.width
                radius: 28
                color: "#102131"
                border.color: "#29516a"
                implicitHeight: heroColumn.implicitHeight + 40

                Column {
                    id: heroColumn
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12

                    Label {
                        width: parent.width
                        text: root.tf("browser_wallet_intro", "A self-custodial Grin wallet shell for Qt WASM. Seed and slate handling stay local in the browser runtime, while chain data comes from an external node.")
                        wrapMode: Text.WordWrap
                        color: "#d7e9f4"
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
                                radius: 22
                                color: "#0e1b27"
                                border.color: "#26465b"
                                implicitHeight: 88

                                Column {
                                    anchors.fill: parent
                                    anchors.margins: 16
                                    spacing: 4

                                    Label {
                                        text: modelData.title
                                        color: "#8fb4c9"
                                        font.pixelSize: 13
                                    }

                                    Label {
                                        width: parent.width
                                        text: modelData.value
                                        wrapMode: Text.WordWrap
                                        color: "#ffffff"
                                        font.pixelSize: 22
                                        font.weight: Font.Bold
                                    }
                                }
                            }
                        }
                    }
                }
            }

            GridLayout {
                width: parent.width
                columns: width < 920 ? 1 : 2
                rowSpacing: 18
                columnSpacing: 18

                Rectangle {
                    Layout.fillWidth: true
                    radius: 26
                    color: "#0f1b26"
                    border.color: "#26465b"
                    implicitHeight: createColumn.implicitHeight + 34

                    ColumnLayout {
                        id: createColumn
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12

                        Label {
                            text: root.tf("browser_wallet_seed_title", "Wallet Seed")
                            color: "#ffffff"
                            font.pixelSize: 26
                            font.weight: Font.Bold
                        }

                        TextField {
                            id: walletNameField
                            Layout.fillWidth: true
                            text: walletPageSettings.walletNameDraft
                            placeholderText: root.tf("browser_wallet_name_placeholder", "Wallet name")
                            onTextChanged: walletPageSettings.walletNameDraft = text
                        }

                        TextArea {
                            id: mnemonicArea
                            Layout.fillWidth: true
                            Layout.preferredHeight: 116
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true
                            persistentSelection: true
                            activeFocusOnPress: true
                            text: walletPageSettings.mnemonicDraft
                            placeholderText: root.tf("browser_wallet_mnemonic_placeholder", "24-word mnemonic for importing an existing wallet.")
                            onTextChanged: walletPageSettings.mnemonicDraft = text
                        }

                        Shortcut {
                            sequences: [StandardKey.Paste, "Ctrl+V", "Shift+Insert"]
                            onActivated: {
                                if (mnemonicArea.activeFocus) {
                                    var pastedText = grinWalletController.requestPasteText()
                                    if (pastedText.length > 0) {
                                        mnemonicArea.text = pastedText
                                    }
                                }
                            }
                        }

                        TextField {
                            id: passwordField
                            Layout.fillWidth: true
                            echoMode: TextInput.Password
                            placeholderText: root.tf("browser_wallet_password_placeholder", "Encryption password")
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Button {
                                text: root.tf("browser_wallet_paste", "Paste")
                                onClicked: {
                                    mnemonicArea.forceActiveFocus()
                                    var pastedText = grinWalletController.requestPasteText()
                                    if (pastedText.length > 0) {
                                        mnemonicArea.text = pastedText
                                    }
                                }
                            }

                            Button {
                                text: root.tf("browser_wallet_create", "Create")
                                onClicked: grinWalletController.createWallet(walletNameField.text, passwordField.text)
                            }

                            Button {
                                text: root.tf("browser_wallet_import", "Import")
                                onClicked: grinWalletController.importWallet(walletNameField.text, mnemonicArea.text, passwordField.text)
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Button {
                                text: root.tf("browser_wallet_unlock", "Unlock")
                                onClicked: grinWalletController.unlockWallet(passwordField.text)
                            }

                            Button {
                                text: root.tf("browser_wallet_lock", "Lock")
                                onClicked: grinWalletController.lockWallet()
                            }
                        }

                        Label {
                            visible: grinWalletController.walletUnlocked
                            width: parent.width
                            text: root.tf("browser_wallet_fingerprint", "Seed fingerprint") + ": " + grinWalletController.seedFingerprint
                            color: "#8ff0c8"
                            wrapMode: Text.WordWrap
                        }

                        TextArea {
                            visible: grinWalletController.walletUnlocked
                            Layout.fillWidth: true
                            Layout.preferredHeight: 116
                            readOnly: true
                            wrapMode: TextEdit.Wrap
                            text: grinWalletController.mnemonicPreview
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: 26
                    color: "#0f1b26"
                    border.color: "#26465b"
                    implicitHeight: nodeColumn.implicitHeight + 34

                    ColumnLayout {
                        id: nodeColumn
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12

                        Label {
                            text: root.tf("browser_wallet_node_title", "External Node")
                            color: "#ffffff"
                            font.pixelSize: 26
                            font.weight: Font.Bold
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 16
                            color: "#132635"
                            border.color: "#2a4f64"
                            implicitHeight: 56

                            Label {
                                anchors.fill: parent
                                anchors.margins: 14
                                verticalAlignment: Text.AlignVCenter
                                text: grinWalletController.nodeUrl
                                color: "#e2f4ff"
                                wrapMode: Text.WrapAnywhere
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Button {
                                text: root.tf("browser_wallet_refresh", "Refresh")
                                onClicked: grinWalletController.refreshNodeStatus()
                            }

                            Button {
                                text: root.tf("browser_wallet_sync", "Sync Wallet")
                                onClicked: grinWalletController.syncWallet()
                            }

                            Button {
                                text: root.tf("browser_wallet_rescan", "Full Rescan")
                                onClicked: grinWalletController.rescanWallet()
                            }
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

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: root.tf("browser_wallet_node_note", "The wallet uses the fixed external Grinffindor mainnet node for chain access. Scanner, output ownership detection, and tx building live in the next wallet-core phases.")
                            color: "#cbdbe4"
                        }
                    }
                }
            }

            Rectangle {
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
                        text: root.tf("browser_wallet_slatepack_title", "Slatepack Workbench")
                        color: "#ffffff"
                        font.pixelSize: 26
                        font.weight: Font.Bold
                    }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: root.tf("browser_wallet_slatepack_note", "Start a SEND flow at S1 or a RECEIVE invoice flow at I1. Paste the counterparty Slatepack into the single exchange field and advance it locally until S3 or I3.")
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
                                if (pastedSlatepack.length > 0) {
                                    slatepackArea.text = pastedSlatepack
                                }
                            }
                        }

                        Button {
                            text: root.tf("browser_wallet_send", "Send")
                            onClicked: {
                                grinWalletController.startSendWorkflow(amountField.text, noteField.text)
                                slatepackArea.text = grinWalletController.workflowSlatepack
                                decodedArea.text = grinWalletController.workflowDecoded
                            }
                        }

                        Button {
                            text: root.tf("browser_wallet_receive", "Receive")
                            onClicked: {
                                grinWalletController.startReceiveWorkflow(amountField.text, noteField.text)
                                slatepackArea.text = grinWalletController.workflowSlatepack
                                decodedArea.text = grinWalletController.workflowDecoded
                            }
                        }

                        Button {
                            text: root.tf("browser_wallet_process", "Process")
                            onClicked: {
                                grinWalletController.processWorkflowSlatepack(slatepackArea.text)
                                slatepackArea.text = grinWalletController.workflowSlatepack
                                decodedArea.text = grinWalletController.workflowDecoded
                            }
                        }

                        Button {
                            text: root.tf("browser_wallet_clear", "Clear")
                            onClicked: {
                                grinWalletController.clearWorkflow()
                                slatepackArea.text = ""
                                decodedArea.text = ""
                            }
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
                                if (pastedSlatepack.length > 0) {
                                    slatepackArea.text = pastedSlatepack
                                }
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
                implicitHeight: historyColumn.implicitHeight + 34

                ColumnLayout {
                    id: historyColumn
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Label {
                        text: root.tf("browser_wallet_history_title", "Transaction History")
                        color: "#ffffff"
                        font.pixelSize: 26
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
                                    text: (modelData.mode || "-") + " / "
                                          + (modelData.state || "-") + " / "
                                          + (modelData.status || "-")
                                    color: "#8ff0c8"
                                    wrapMode: Text.WordWrap
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: (modelData.amount || "0") + " GRIN"
                                          + "  fee " + (modelData.fee || "0")
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
                                    visible: modelData.payment_proof_status !== undefined
                                    text: "Payment proof: " + (modelData.payment_proof_status || "-")
                                    color: "#d7e9f4"
                                    wrapMode: Text.WordWrap
                                    font.pixelSize: 12
                                }

                                Label {
                                    Layout.fillWidth: true
                                    visible: modelData.broadcasted === true || modelData.confirmations !== undefined
                                    text: (modelData.confirmed_height !== undefined
                                               ? "Confirmed at height " + (modelData.confirmed_height || "-") + "  "
                                               : "")
                                          + "Confirmations " + (modelData.confirmations || 0)
                                    color: "#d7e9f4"
                                    wrapMode: Text.WordWrap
                                    font.pixelSize: 12
                                }

                                Label {
                                    Layout.fillWidth: true
                                    visible: modelData.broadcast_error !== undefined
                                    text: modelData.broadcast_error || ""
                                    color: "#ffb4b4"
                                    wrapMode: Text.WordWrap
                                    font.pixelSize: 12
                                }

                                Label {
                                    Layout.fillWidth: true
                                    visible: modelData.broadcast_at !== undefined || modelData.last_node_check !== undefined
                                    text: (modelData.broadcast_at !== undefined
                                               ? "Broadcast: " + modelData.broadcast_at
                                               : "")
                                          + (modelData.last_node_check !== undefined
                                               ? ((modelData.broadcast_at !== undefined ? "  " : "") + "Checked: " + modelData.last_node_check)
                                               : "")
                                    color: "#8fb4c9"
                                    wrapMode: Text.WordWrap
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

        }
    }

    Connections {
        target: grinWalletController

        function onWorkflowChanged() {
            if (grinWalletController.workflowSlatepack.length > 0) {
                slatepackArea.text = grinWalletController.workflowSlatepack
            }
            if (grinWalletController.workflowDecoded.length > 0) {
                decodedArea.text = grinWalletController.workflowDecoded
            }
        }

        function onWalletChanged() {
            if (grinWalletController.walletName.length > 0) {
                walletPageSettings.walletNameDraft = grinWalletController.walletName
            }
            if (grinWalletController.walletUnlocked && grinWalletController.mnemonicPreview.length > 0) {
                walletPageSettings.mnemonicDraft = grinWalletController.mnemonicPreview
                mnemonicArea.text = grinWalletController.mnemonicPreview
            }
        }
    }

    Component.onCompleted: {
        grinWalletController.initialize()
        slatepackArea.text = ""
        decodedArea.text = ""
    }
}
