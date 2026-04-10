import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: topBar
    property var walletRoot
    readonly property bool compactStatusChip: walletRoot.compactNavigation && topBar.width < 760

    function statusText() {
        var text = grinWalletController.syncStatus || ""
        if (!compactStatusChip)
            return text
        if (text.indexOf("Seed scan") === 0)
            return text
        if (text === "Scanning wallet outputs...")
            return "Scanning..."
        if (walletRoot.nodeStatusMode() === "offline")
            return "Offline"
        if (walletRoot.nodeStatusMode() === "connecting")
            return "Syncing..."
        if (text === "Connected to external node" || text === "Wallet outputs synced" || text === "Idle")
            return "Synced"
        return text
    }

    signal menuRequested()

    height: walletRoot.veryPhoneMode ? 54 : (walletRoot.phoneMode ? 60 : 72)
    color: "#101822"
    border.color: "#234b63"
    z: 2

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: walletRoot.pageMargin
        anchors.rightMargin: walletRoot.pageMargin
        spacing: walletRoot.veryPhoneMode ? 8 : (walletRoot.phoneMode ? 10 : 14)

        ToolButton {
            id: menuButton
            visible: walletRoot.compactNavigation && grinWalletController.walletUnlocked
            text: walletRoot.tf("browser_wallet_menu", "Menu")
            font.pixelSize: walletRoot.controlTextSize
            padding: walletRoot.veryPhoneMode ? 6 : (walletRoot.phoneMode ? 8 : 10)

            background: Rectangle {
                radius: walletRoot.veryPhoneMode ? 8 : (walletRoot.phoneMode ? 10 : 12)
                color: menuButton.down ? "#1a3448" : (menuButton.hovered ? "#173042" : "#122231")
                border.color: "#2f607a"
                border.width: 1
            }

            contentItem: Text {
                text: menuButton.text
                font: menuButton.font
                color: "#d8f3ff"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            onClicked: topBar.menuRequested()
        }

        Item { Layout.fillWidth: true }

        Rectangle {
            visible: true
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            Layout.preferredWidth: Math.min(statusLabel.implicitWidth + 24,
                                            Math.max(112, topBar.width - (menuButton.visible ? menuButton.implicitWidth + 68 : 36)))
            Layout.maximumWidth: Math.max(112, topBar.width - (menuButton.visible ? menuButton.implicitWidth + 68 : 36))
            radius: walletRoot.veryPhoneMode ? 10 : (walletRoot.phoneMode ? 12 : 14)
            color: "#122231"
            border.color: walletRoot.nodeStatusMode() === "offline" ? "#8a3f3f"
                         : (walletRoot.nodeStatusMode() === "connecting" ? "#8a7440" : "#2f607a")
            implicitHeight: walletRoot.veryPhoneMode ? 28 : (walletRoot.phoneMode ? 30 : 34)

            Label {
                id: statusLabel
                anchors.fill: parent
                anchors.leftMargin: walletRoot.veryPhoneMode ? 10 : 12
                anchors.rightMargin: walletRoot.veryPhoneMode ? 10 : 12
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                text: topBar.statusText()
                color: "#d8f3ff"
                font.pixelSize: walletRoot.compactTextSize
            }
        }
    }
}