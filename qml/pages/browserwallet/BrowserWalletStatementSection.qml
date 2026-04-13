import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: statementSection
    property var walletRoot
    readonly property int statementDesktopBreakpoint: 860

    function amountStringToValue(amountText) {
        var parsed = Number(amountText ? amountText.toString().trim() : "")
        return isFinite(parsed) ? parsed : 0
    }

    function statementDateColumnWidth(layoutWidth) {
        return Math.max(160, Math.round(layoutWidth * 0.22))
    }

    function statementAmountColumnWidth(layoutWidth) {
        return Math.max(110, Math.round(layoutWidth * 0.14))
    }

    function statementReferenceColumnWidth(layoutWidth) {
        var fixedWidth = statementDateColumnWidth(layoutWidth)
            + statementAmountColumnWidth(layoutWidth)
            + (12 * 2)
        return Math.max(220, layoutWidth - fixedWidth)
    }

    function formatAmountValue(value) {
        var fixed = Math.abs(value).toFixed(9)
        return fixed.replace(/0+$/, "").replace(/\.$/, "")
    }

    function signedStatementAmount(modelData) {
        var amount = amountStringToValue(modelData.amount || "0")
        var fee = amountStringToValue(modelData.fee || "0")
        var mode = (modelData.mode || "").toLowerCase()
        if (mode === "send")
            return -(amount + fee)
        if (mode === "receive" || mode === "invoice")
            return amount
        return 0
    }

    function signedStatementText(modelData) {
        var signed = signedStatementAmount(modelData)
        var prefix = signed >= 0 ? "+" : "-"
        return prefix + formatAmountValue(signed) + " GRIN"
    }

    function signedStatementColor(modelData) {
        return signedStatementAmount(modelData) >= 0 ? "#FEF102" : "#dd6070"
    }

    function creditText(modelData) {
        var signed = signedStatementAmount(modelData)
        return signed > 0 ? "+" + formatAmountValue(signed) + " GRIN" : "-"
    }

    function debitText(modelData) {
        var signed = signedStatementAmount(modelData)
        return signed < 0 ? "-" + formatAmountValue(signed) + " GRIN" : "-"
    }

    function statementTimestamp(modelData) {
        var candidates = [
            modelData.timestamp,
            modelData.broadcast_at,
            modelData.last_broadcast_attempt,
            modelData.cancelled_at,
            modelData.last_node_check
        ]
        for (var i = 0; i < candidates.length; ++i) {
            var value = candidates[i]
            if (value && value.toString().trim().length > 0)
                return value
        }
        return "-"
    }

    function statementLabel(modelData) {
        var mode = (modelData.mode || "-").toUpperCase()
        var state = modelData.state || "-"
        return mode + " / " + state
    }

    function statementDetail(modelData) {
        var mode = (modelData.mode || "").toLowerCase()
        var amount = formatAmountValue(amountStringToValue(modelData.amount || "0")) + " GRIN"
        var fee = amountStringToValue(modelData.fee || "0")
        if (mode === "send")
            return walletRoot.tf("browser_wallet_statement_detail_send", "Outgoing payment") + ": " + amount
                + "  ·  " + walletRoot.tf("browser_wallet_statement_fee", "Fee") + ": " + formatAmountValue(fee) + " GRIN"
        if (mode === "receive" || mode === "invoice")
            return walletRoot.tf("browser_wallet_statement_detail_receive", "Incoming payment") + ": " + amount
        return amount
    }

    function confirmedStatementEntries() {
        var entries = []
        var list = grinWalletController.transactionHistory || []
        for (var i = 0; i < list.length; ++i) {
            var entry = list[i]
            var confirmations = entry.confirmations !== undefined ? Number(entry.confirmations) : 0
            if ((entry.status || "") === "confirmed" && isFinite(confirmations) && confirmations >= 10)
                entries.push(entry)
        }
        return entries
    }

    function statementExportText() {
        var lines = []
        lines.push(walletRoot.tf("browser_wallet_statement_title", "Statement of Account"))
        lines.push(walletRoot.tf("browser_wallet_statement_current_balance", "Current Balance") + ": " + grinWalletController.totalBalance + " GRIN")
        lines.push("")
        var list = confirmedStatementEntries()
        for (var i = 0; i < list.length; ++i) {
            var entry = list[i]
            lines.push(statementTimestamp(entry) + " | "
                       + statementLabel(entry) + " | "
                       + (entry.status || "-") + " | "
                       + signedStatementText(entry))
        }
        return lines.join("\n")
    }

    implicitHeight: statementCard.implicitHeight

    BrowserWalletSectionCard {
        id: statementCard
        width: parent ? parent.width : 0
        title: walletRoot.tf("browser_wallet_statement_title", "Statement of Account")
        fillColor: "#0d1c2a"
        strokeColor: "#182e40"

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12

            // ── Balance summary ───────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                radius: 14
                color: "#091825"
                border.color: "#132e42"
                border.width: 1
                implicitHeight: balanceSummaryCol.implicitHeight + 28

                ColumnLayout {
                    id: balanceSummaryCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 16
                    spacing: 12

                    // Total balance + export
                    RowLayout {
                        Layout.fillWidth: true

                        Column {
                            spacing: 3

                            Label {
                                text: walletRoot.tf("browser_wallet_statement_current_balance", "Current Balance")
                                color: "#2a5a72"
                                font.pixelSize: 11
                                font.letterSpacing: 0.8
                            }

                            Label {
                                text: grinWalletController.totalBalance + " GRIN"
                                color: "#c0e8ff"
                                font.pixelSize: walletRoot.veryPhoneMode ? 22 : (walletRoot.phoneMode ? 26 : 30)
                                font.weight: Font.Bold
                                wrapMode: Text.WordWrap
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            text: walletRoot.tf("browser_wallet_statement_export", "Export")
                            font.pixelSize: walletRoot.controlTextSize
                            background: Rectangle {
                                radius: 10; color: parent.down ? "#07111c" : (parent.hovered ? "#060e18" : "transparent")
                                border.color: parent.hovered ? "#1e3a52" : "#152a3c"; border.width: 1
                            }
                            contentItem: Label {
                                text: parent.text; font: parent.font; color: parent.hovered ? "#88b8d8" : "#5a8eac"
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: grinWalletController.downloadTextFile("statement-of-account.txt", statementSection.statementExportText())
                        }
                    }

                    // Sub-balance grid
                    GridLayout {
                        Layout.fillWidth: true
                        columns: width < 760 ? 2 : 4
                        rowSpacing: 8
                        columnSpacing: 14

                        Repeater {
                            model: [
                                { label: walletRoot.tf("browser_wallet_statement_spendable", "Spendable"), value: grinWalletController.spendableBalance + " GRIN" },
                                { label: walletRoot.tf("browser_wallet_statement_awaiting",  "Awaiting"),
                                  value: (statementSection.formatAmountValue(
                                              statementSection.amountStringToValue(grinWalletController.awaitingConfirmationBalance)
                                              + statementSection.amountStringToValue(grinWalletController.awaitingFinalizationBalance)) || "0") + " GRIN" },
                                { label: walletRoot.tf("browser_wallet_statement_locked",    "Locked"),   value: grinWalletController.lockedBalance + " GRIN" },
                                { label: walletRoot.tf("browser_wallet_history_confirmations","Confs"),   value: "≥ 10" }
                            ]

                            Column {
                                spacing: 2
                                Label { text: modelData.label; color: "#2a5a72"; font.pixelSize: 11; font.letterSpacing: 0.5 }
                                Label { text: modelData.value; color: "#5a9ab8"; font.pixelSize: 13; font.weight: Font.DemiBold }
                            }
                        }
                    }
                }
            }

            // ── Booking entries ───────────────────────────────────────────
            Column {
                Layout.fillWidth: true
                spacing: 0

                // Section header
                RowLayout {
                    width: parent.width
                    visible: statementSection.confirmedStatementEntries().length > 0 && width >= statementSection.statementDesktopBreakpoint

                    Label {
                        Layout.preferredWidth: statementSection.statementDateColumnWidth(parent.width)
                        text: walletRoot.tf("browser_wallet_statement_date", "Date / Time")
                        color: "#2a5a72"
                        font.pixelSize: walletRoot.bodyTextSize
                        font.weight: Font.DemiBold
                    }

                    Label {
                        Layout.fillWidth: true
                        text: walletRoot.tf("browser_wallet_statement_reference", "Reference")
                        color: "#2a5a72"
                        font.pixelSize: walletRoot.bodyTextSize
                        font.weight: Font.DemiBold
                    }

                    Label {
                        Layout.preferredWidth: statementSection.statementAmountColumnWidth(parent.width)
                        text: walletRoot.tf("browser_wallet_statement_amount", "Amount")
                        color: "#2a5a72"
                        font.pixelSize: walletRoot.bodyTextSize
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignRight
                    }
                }

                Item {
                    width: parent.width
                    height: statementSection.confirmedStatementEntries().length > 0 && parent.width >= statementSection.statementDesktopBreakpoint ? 6 : 0
                }
            }

            Repeater {
                model: statementSection.confirmedStatementEntries()

                Rectangle {
                    width: parent.width
                    radius: 12
                    color: index % 2 === 0 ? "#091825" : "#080f1c"
                    border.color: "#112030"
                    border.width: 1
                    implicitHeight: entryCol.implicitHeight + entryCol.anchors.topMargin + entryCol.anchors.bottomMargin
                    Layout.fillWidth: true

                    // Left accent bar for signed amount
                    Rectangle {
                        width: 3
                        height: Math.round(parent.height * 0.5)
                        radius: 2
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        color: statementSection.signedStatementColor(modelData)
                        opacity: 0.6
                    }

                    ColumnLayout {
                        id: entryCol
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 18
                        anchors.leftMargin: 22
                        anchors.topMargin: 22
                        anchors.bottomMargin: 30
                        spacing: 0

                        GridLayout {
                            id: entryGrid
                            Layout.fillWidth: true
                            columns: width < statementSection.statementDesktopBreakpoint ? 1 : 3
                            rowSpacing: 6
                            columnSpacing: 12

                            // Date / time
                            Label {
                                Layout.preferredWidth: parent.columns >= 3 ? statementSection.statementDateColumnWidth(parent.width) : -1
                                Layout.fillWidth: parent.columns < 3
                                text: statementSection.statementTimestamp(modelData)
                                color: "#4a7898"
                                font.pixelSize: walletRoot.bodyTextSize
                                wrapMode: Text.WordWrap
                            }

                            // Reference column
                            ColumnLayout {
                                Layout.preferredWidth: parent.columns >= 3 ? statementSection.statementReferenceColumnWidth(parent.width) : -1
                                Layout.fillWidth: true
                                spacing: 2

                                Label {
                                    Layout.fillWidth: true
                                    text: statementSection.statementLabel(modelData)
                                    color: "#c0d8f0"
                                    font.pixelSize: walletRoot.bodyTextSize
                                    font.weight: Font.DemiBold
                                    wrapMode: Text.WordWrap
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: statementSection.statementDetail(modelData)
                                    color: "#4a7898"
                                    font.pixelSize: walletRoot.bodyTextSize
                                    wrapMode: Text.WordWrap
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.workflow_id || ""
                                    color: "#1e3e52"
                                    font.pixelSize: walletRoot.compactTextSize - 1
                                    wrapMode: Text.WrapAnywhere
                                }
                            }

                            // Amount
                            Label {
                                Layout.preferredWidth: statementSection.statementAmountColumnWidth(parent.width)
                                visible: parent.columns >= 3
                                text: statementSection.signedStatementText(modelData)
                                color: statementSection.signedStatementColor(modelData)
                                font.pixelSize: walletRoot.bodyTextSize
                                font.weight: Font.Bold
                                horizontalAlignment: Text.AlignRight
                            }
                        }

                        // Mobile: show amount inline
                        RowLayout {
                            Layout.fillWidth: true
                            visible: entryGrid.width < statementSection.statementDesktopBreakpoint
                            Layout.topMargin: 4

                            Label {
                                text: statementSection.signedStatementText(modelData)
                                color: statementSection.signedStatementColor(modelData)
                                font.pixelSize: walletRoot.compactTextSize
                                font.weight: Font.Bold
                            }
                        }
                    }
                }
            }

            // Empty state
            Item {
                Layout.fillWidth: true
                visible: statementSection.confirmedStatementEntries().length === 0
                implicitHeight: noEntryLabel.implicitHeight + 20

                Label {
                    id: noEntryLabel
                    anchors.centerIn: parent
                    width: parent.width
                    text: walletRoot.tf("browser_wallet_statement_empty", "No statement entries are available yet. Complete or receive a transaction first.")
                    color: "#2a5060"
                    font.pixelSize: walletRoot.bodyTextSize
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }
}
