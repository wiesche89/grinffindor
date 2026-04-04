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
        return Math.max(170, Math.round(layoutWidth * 0.22))
    }

    function statementAmountColumnWidth(layoutWidth) {
        return Math.max(120, Math.round(layoutWidth * 0.14))
    }

    function statementReferenceColumnWidth(layoutWidth) {
        var fixedWidth = statementDateColumnWidth(layoutWidth)
            + statementAmountColumnWidth(layoutWidth)
            + (12 * 2)
        return Math.max(240, layoutWidth - fixedWidth)
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
        return signedStatementAmount(modelData) >= 0 ? "#8ff0c8" : "#ffb4b4"
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
                + "  |  " + walletRoot.tf("browser_wallet_statement_fee", "Fee") + ": " + formatAmountValue(fee) + " GRIN"
        if (mode === "receive" || mode === "invoice")
            return walletRoot.tf("browser_wallet_statement_detail_receive", "Incoming payment") + ": " + amount
        return amount
    }

    function confirmedStatementEntries() {
        var entries = []
        var list = grinWalletController.transactionHistory || []
        for (var i = 0; i < list.length; ++i) {
            var entry = list[i]
            if ((entry.status || "") === "confirmed")
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
        fillColor: "#0f1b26"
        strokeColor: "#29516a"

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 14

            BrowserWalletPanel {
                Layout.fillWidth: true
                fillColor: "#102737"
                strokeColor: "#35617d"

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Label {
                                Layout.fillWidth: true
                                text: walletRoot.tf("browser_wallet_statement_current_balance", "Current Balance")
                                color: "#8fb4c9"
                                font.pixelSize: 13
                                font.letterSpacing: 1.1
                            }

                            Label {
                                Layout.fillWidth: true
                                text: grinWalletController.totalBalance + " GRIN"
                                color: "#ffffff"
                                font.pixelSize: 30
                                font.weight: Font.Bold
                                wrapMode: Text.WordWrap
                            }
                        }

                        Button {
                            text: walletRoot.tf("browser_wallet_statement_export", "Export")
                            onClicked: grinWalletController.downloadTextFile("statement-of-account.txt", statementSection.statementExportText())
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: width < 760 ? 2 : 4
                        rowSpacing: 10
                        columnSpacing: 12

                        Label { text: walletRoot.tf("browser_wallet_statement_spendable", "Spendable"); color: "#8fb4c9" }
                        Label { text: grinWalletController.spendableBalance + " GRIN"; color: "#d7e9f4" }
                        Label { text: walletRoot.tf("browser_wallet_statement_awaiting", "Awaiting"); color: "#8fb4c9" }
                        Label {
                            text: (statementSection.formatAmountValue(
                                       statementSection.amountStringToValue(grinWalletController.awaitingConfirmationBalance)
                                       + statementSection.amountStringToValue(grinWalletController.awaitingFinalizationBalance)) || "0") + " GRIN"
                            color: "#d7e9f4"
                        }
                        Label { text: walletRoot.tf("browser_wallet_statement_locked", "Locked"); color: "#8fb4c9" }
                        Label { text: grinWalletController.lockedBalance + " GRIN"; color: "#d7e9f4" }
                        Label { text: walletRoot.tf("browser_wallet_history_confirmations", "Confirmations"); color: "#8fb4c9" }
                        Label { text: "≥ 10"; color: "#d7e9f4" }
                    }
                }
            }

            BrowserWalletPanel {
                Layout.fillWidth: true
                title: walletRoot.tf("browser_wallet_statement_entries", "Bookings")
                description: walletRoot.tf("browser_wallet_statement_entries_note", "Statement entries are shown with signed amount, timestamp and workflow reference.")

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Rectangle {
                        Layout.fillWidth: true
                        visible: statementSection.confirmedStatementEntries().length > 0
                        radius: 14
                        color: "#101b25"
                        border.color: "#223847"
                        implicitHeight: statementHeader.implicitHeight + 18

                        GridLayout {
                            id: statementHeader
                            anchors.fill: parent
                            anchors.margins: 10
                            columns: width < statementSection.statementDesktopBreakpoint ? 2 : 3
                            rowSpacing: 8
                            columnSpacing: 12

                            Label {
                                Layout.preferredWidth: parent.columns >= 3 ? statementSection.statementDateColumnWidth(parent.width) : -1
                                text: walletRoot.tf("browser_wallet_statement_date", "Date / Time")
                                color: "#7ea0b3"
                                font.weight: Font.DemiBold
                            }
                            Label {
                                Layout.preferredWidth: parent.columns >= 3 ? statementSection.statementReferenceColumnWidth(parent.width) : -1
                                text: walletRoot.tf("browser_wallet_statement_reference", "Reference")
                                color: "#7ea0b3"
                                font.weight: Font.DemiBold
                            }
                            Label {
                                Layout.preferredWidth: statementSection.statementAmountColumnWidth(parent.width)
                                visible: parent.columns >= 3
                                text: walletRoot.tf("browser_wallet_statement_amount", "Amount")
                                color: "#7ea0b3"
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                    }

                    Repeater {
                        model: statementSection.confirmedStatementEntries()

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 16
                            color: index % 2 === 0 ? "#122231" : "#0f1b26"
                            border.color: "#21384a"
                            implicitHeight: entryColumn.implicitHeight + 22

                            ColumnLayout {
                                id: entryColumn
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 8

                                GridLayout {
                                    id: entryGrid
                                    Layout.fillWidth: true
                                    columns: width < statementSection.statementDesktopBreakpoint ? 1 : 3
                                    rowSpacing: 8
                                    columnSpacing: 12

                                    Label {
                                        Layout.preferredWidth: parent.columns >= 3 ? statementSection.statementDateColumnWidth(parent.width) : -1
                                        Layout.fillWidth: parent.columns < 3
                                        text: statementSection.statementTimestamp(modelData)
                                        color: "#d7e9f4"
                                        wrapMode: Text.WordWrap
                                    }

                                    ColumnLayout {
                                        Layout.preferredWidth: parent.columns >= 3 ? statementSection.statementReferenceColumnWidth(parent.width) : -1
                                        Layout.fillWidth: true
                                        spacing: 3

                                        Label {
                                            Layout.fillWidth: true
                                            text: statementSection.statementLabel(modelData)
                                            color: "#ffffff"
                                            font.weight: Font.DemiBold
                                            wrapMode: Text.WordWrap
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            text: statementSection.statementDetail(modelData)
                                            color: "#8fb4c9"
                                            wrapMode: Text.WordWrap
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            text: (modelData.workflow_id || "")
                                            color: "#6f94a8"
                                            font.pixelSize: 12
                                            wrapMode: Text.WrapAnywhere
                                        }
                                    }

                                    Label {
                                        Layout.preferredWidth: statementSection.statementAmountColumnWidth(parent.width)
                                        visible: parent.columns >= 3
                                        text: statementSection.signedStatementText(modelData)
                                        color: statementSection.signedStatementColor(modelData)
                                        horizontalAlignment: Text.AlignRight
                                        font.weight: Font.DemiBold
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    visible: entryGrid.width < statementSection.statementDesktopBreakpoint

                                    Label {
                                        text: walletRoot.tf("browser_wallet_statement_amount", "Amount") + ": " + statementSection.signedStatementText(modelData)
                                        color: statementSection.signedStatementColor(modelData)
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                    }
                                }
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: statementSection.confirmedStatementEntries().length === 0
                        text: walletRoot.tf("browser_wallet_statement_empty", "No statement entries are available yet. Complete or receive a transaction first.")
                        color: "#8fb4c9"
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}