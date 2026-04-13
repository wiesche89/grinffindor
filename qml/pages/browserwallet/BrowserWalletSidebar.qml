import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../components" as AppComponents

Rectangle {
    id: sidebar
    property var walletRoot
    property bool compactMode: false
    readonly property bool narrow: walletRoot && walletRoot.phoneMode
    readonly property bool veryNarrow: walletRoot && walletRoot.veryPhoneMode
    readonly property real scaleFactor: walletRoot ? walletRoot.mobileScale : 1.0

    signal sectionSelected(string section)

    color: compactMode ? "#070e18" : "#080f1a"
    radius: compactMode ? 0 : 20
    border.color: compactMode ? "transparent" : "#12263a"
    border.width: 1

    function walletDisplayName() {
        return grinWalletController.walletName.length > 0
            ? grinWalletController.walletName
            : walletRoot.tf("browser_wallet_metric_empty", "Not created")
    }

    function activateSection(section) {
        walletRoot.activeSection = section
        sectionSelected(section)
    }

    function handleBackLock() {
        if (grinWalletController.walletUnlocked)
            grinWalletController.lockWallet()
        walletRoot.backRequested()
        sectionSelected("back")
    }

    Column {
        anchors.fill: parent
        anchors.margins: compactMode ? 16 : 18
        spacing: 4

        // Wallet header
        Item {
            width: parent.width
            height: veryNarrow ? 70 : (narrow ? 80 : (compactMode ? 96 : 108))

            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                Label {
                    width: parent.width
                    text: walletDisplayName()
                    color: "#c8e4f8"
                    font.pixelSize: veryNarrow ? 18 : (narrow ? 20 : (compactMode ? 22 : 26))
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }

                Label {
                    visible: grinWalletController.selectedNetwork.length > 0
                    text: grinWalletController.selectedNetwork
                    color: "#FEF102"
                    font.pixelSize: veryNarrow ? 11 : 12
                    font.letterSpacing: 0.5
                }
            }
        }

        // Thin divider
        Rectangle {
            width: parent.width
            height: 1
            color: "#10273a"
        }

        Item { height: veryNarrow ? 6 : 10; width: 1 }

        // Navigation label
        Label {
            width: parent.width
            text: walletRoot.tf("browser_wallet_nav_section_label", "Navigation")
            color: "#2a5068"
            font.pixelSize: 10
            font.letterSpacing: 1.4
            font.weight: Font.Medium
        }

        Item { height: 4; width: 1 }

        // Nav items
        AppComponents.AppNavItem {
            width: parent.width
            text: walletRoot.tf("browser_wallet_nav_home", "Home")
            active: walletRoot.activeSection === "home"
            fontSize: walletRoot.controlTextSize
            onClicked: activateSection("home")
        }

        AppComponents.AppNavItem {
            width: parent.width
            text: walletRoot.tf("browser_wallet_nav_utxos", "UTXOs")
            active: walletRoot.activeSection === "utxos"
            fontSize: walletRoot.controlTextSize
            onClicked: activateSection("utxos")
        }

        AppComponents.AppNavItem {
            width: parent.width
            text: walletRoot.tf("browser_wallet_nav_statement", "Statement of Account")
            active: walletRoot.activeSection === "statement"
            fontSize: walletRoot.controlTextSize
            onClicked: activateSection("statement")
        }

        AppComponents.AppNavItem {
            width: parent.width
            text: walletRoot.tf("browser_wallet_nav_send_receive", "Send / Receive")
            active: walletRoot.activeSection === "slatepack"
            fontSize: walletRoot.controlTextSize
            onClicked: activateSection("slatepack")
        }

        AppComponents.AppNavItem {
            width: parent.width
            text: walletRoot.tf("browser_wallet_nav_settings", "Settings")
            active: walletRoot.activeSection === "settings"
            fontSize: walletRoot.controlTextSize
            onClicked: activateSection("settings")
        }

        // Push lock to bottom
        Item { height: veryNarrow ? 16 : 20; width: 1 }

        // Thin divider
        Rectangle {
            width: parent.width
            height: 1
            color: "#10273a"
        }

        Item { height: 8; width: 1 }

        // Back / Lock
        Item {
            width: parent.width
            height: 38

            Rectangle {
                anchors.fill: parent
                radius: 9
                color: lockMa.pressed ? "#1a0a10" : (lockMa.containsMouse ? "#120810" : "transparent")
                border.color: lockMa.containsMouse ? "#5a2030" : "transparent"

                Label {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 18
                    anchors.verticalCenter: parent.verticalCenter
                    text: walletRoot.tf("browser_wallet_back_lock", "Back (Lock)")
                    color: lockMa.pressed ? "#cc3050" : (lockMa.containsMouse ? "#aa2840" : "#3a5060")
                    font.pixelSize: walletRoot.controlTextSize
                    elide: Text.ElideRight
                }

                MouseArea {
                    id: lockMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: sidebar.handleBackLock()
                }
            }
        }
    }
}
