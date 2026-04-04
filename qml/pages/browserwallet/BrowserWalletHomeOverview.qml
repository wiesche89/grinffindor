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

            Repeater {
                model: [
                    { title: walletRoot.tf("browser_wallet_metric_network", "Network"), value: grinWalletController.selectedNetwork },
                    { title: walletRoot.tf("browser_wallet_metric_chain", "Chain Height"), value: "" + grinWalletController.chainHeight },
                    { title: walletRoot.tf("browser_wallet_metric_scan", "Scan Height"), value: "" + grinWalletController.scanHeight },
                    { title: walletRoot.tf("browser_wallet_metric_balance", "Spendable"), value: grinWalletController.spendableBalance + " GRIN" },
                    { title: walletRoot.tf("browser_wallet_awaiting_confirmation", "Awaiting Confirmation"), value: grinWalletController.awaitingConfirmationBalance + " GRIN" },
                    { title: walletRoot.tf("browser_wallet_locked", "Locked"), value: grinWalletController.lockedBalance + " GRIN" }
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