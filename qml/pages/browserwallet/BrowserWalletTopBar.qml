import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: topBar
    property var walletRoot

    signal menuRequested()

    height: 72
    color: "#101822"
    border.color: "#234b63"
    z: 2

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        spacing: 14

        ToolButton {
            visible: walletRoot.compactNavigation && grinWalletController.walletUnlocked
            text: walletRoot.tf("browser_wallet_menu", "Menu")
            onClicked: topBar.menuRequested()
        }

        Item { Layout.fillWidth: true }

        Rectangle {
            visible: !walletRoot.compactNavigation || topBar.width >= 760
            radius: 14
            color: "#122231"
            border.color: walletRoot.nodeStatusMode() === "offline" ? "#8a3f3f"
                         : (walletRoot.nodeStatusMode() === "connecting" ? "#8a7440" : "#2f607a")
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