import QtQuick
import QtQuick.Controls

Rectangle {
    id: sidebar
    property var walletRoot

    radius: 28
    color: "#0f1722"
    border.color: "#26465b"

    Column {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        Label {
            width: parent.width
            text: walletRoot.tf("browser_wallet_sidebar_title", "Wallet")
            color: "#ffffff"
            font.pixelSize: 28
            font.weight: Font.Bold
        }

        Label {
            width: parent.width
            text: grinWalletController.walletName.length > 0
                  ? grinWalletController.walletName
                  : walletRoot.tf("browser_wallet_metric_empty", "Not created")
            color: "#8ff0c8"
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: [
                { key: "home", title: walletRoot.tf("browser_wallet_nav_home", "Home") },
                { key: "utxos", title: walletRoot.tf("browser_wallet_nav_utxos", "UTXOs") },
                { key: "slatepack", title: walletRoot.tf("browser_wallet_nav_slatepack", "Slatepack") },
                { key: "settings", title: walletRoot.tf("browser_wallet_nav_settings", "Settings") }
            ]

            Column {
                width: parent.width
                spacing: 6

                Button {
                    width: parent.width
                    text: modelData.title
                    flat: true
                    highlighted: walletRoot.activeSection === modelData.key
                    onClicked: walletRoot.activeSection = modelData.key
                }
            }
        }
    }
}