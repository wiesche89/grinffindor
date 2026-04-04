import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: overviewCard
    property var walletRoot

    width: parent ? parent.width : 0
    radius: 28
    color: "#102131"
    border.color: "#29516a"
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

                    Label { text: walletRoot.tf("browser_wallet_metric_network", "Network"); color: "#8fb4c9"; font.pixelSize: 13 }
                    Label {
                        width: parent.width
                        text: grinWalletController.selectedNetwork
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                        font.pixelSize: 22
                        font.weight: Font.Bold
                    }
                }
            }

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

                    Label { text: walletRoot.tf("browser_wallet_metric_chain", "Chain Height"); color: "#8fb4c9"; font.pixelSize: 13 }
                    Label {
                        width: parent.width
                        text: "" + grinWalletController.chainHeight
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                        font.pixelSize: 22
                        font.weight: Font.Bold
                    }
                }
            }

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

                    Label { text: walletRoot.tf("browser_wallet_metric_scan", "Scan Height"); color: "#8fb4c9"; font.pixelSize: 13 }
                    Label {
                        width: parent.width
                        text: "" + grinWalletController.scanHeight
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                        font.pixelSize: 22
                        font.weight: Font.Bold
                    }
                }
            }

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

                    Label { text: walletRoot.tf("browser_wallet_metric_balance", "Spendable"); color: "#8fb4c9"; font.pixelSize: 13 }
                    Label {
                        width: parent.width
                        text: grinWalletController.spendableBalance + " GRIN"
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                        font.pixelSize: 22
                        font.weight: Font.Bold
                    }
                }
            }

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

                    Label { text: walletRoot.tf("browser_wallet_awaiting_confirmation", "Awaiting Confirmation"); color: "#8fb4c9"; font.pixelSize: 13 }
                    Label {
                        width: parent.width
                        text: grinWalletController.awaitingConfirmationBalance + " GRIN"
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                        font.pixelSize: 22
                        font.weight: Font.Bold
                    }
                }
            }

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

                    Label { text: walletRoot.tf("browser_wallet_locked", "Locked"); color: "#8fb4c9"; font.pixelSize: 13 }
                    Label {
                        width: parent.width
                        text: grinWalletController.lockedBalance + " GRIN"
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