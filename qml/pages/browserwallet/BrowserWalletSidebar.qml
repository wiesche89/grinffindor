import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Rectangle {
    id: sidebar
    property var walletRoot
    property bool compactMode: false

    signal sectionSelected(string section)

    color: compactMode ? "#0d1720" : "#0f1722"
    radius: compactMode ? 0 : 28
    border.color: compactMode ? "transparent" : "#26465b"

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
        anchors.margins: compactMode ? 20 : 22
        spacing: 16

        Rectangle {
            width: parent.width
            radius: 24
            color: "#12202c"
            border.color: "#24455a"
            implicitHeight: compactMode ? 128 : 142

            Row {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10

                Rectangle {
                    width: 44
                    height: 44
                    radius: 14
                    color: "#173247"
                    border.color: "#2f607a"

                    Label {
                        anchors.centerIn: parent
                        text: walletDisplayName().length > 0 ? walletDisplayName().charAt(0).toUpperCase() : "W"
                        color: "#d8f3ff"
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                    }
                }

                Label {
                    width: parent.width - 54
                    height: Math.max(parent.height, implicitHeight)
                    text: walletDisplayName()
                    color: "#ffffff"
                    wrapMode: Text.WordWrap
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: compactMode ? 28 : 30
                    font.weight: Font.Bold
                }
            }
        }

        Label {
            width: parent.width
            text: walletRoot.tf("browser_wallet_nav_section_label", "Navigation")
            color: "#7ea0b3"
            font.pixelSize: 12
            font.letterSpacing: 1.2
        }

        Button {
            width: parent.width
            text: walletRoot.tf("browser_wallet_nav_home", "Home")
            leftPadding: 18
            rightPadding: 18
            topPadding: 14
            bottomPadding: 14
            highlighted: walletRoot.activeSection === "home"
            background: Rectangle {
                radius: 16
                color: parent.highlighted ? "#173247" : "#111c26"
                border.color: parent.highlighted ? "#4d89aa" : "#223847"
            }
            contentItem: Label {
                text: parent.text
                color: parent.highlighted ? "#f7fbff" : "#b8cfdd"
                font.pixelSize: 16
                font.weight: parent.highlighted ? Font.DemiBold : Font.Medium
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: activateSection("home")
        }

        Button {
            width: parent.width
            text: walletRoot.tf("browser_wallet_nav_utxos", "UTXOs")
            leftPadding: 18
            rightPadding: 18
            topPadding: 14
            bottomPadding: 14
            highlighted: walletRoot.activeSection === "utxos"
            background: Rectangle {
                radius: 16
                color: parent.highlighted ? "#173247" : "#111c26"
                border.color: parent.highlighted ? "#4d89aa" : "#223847"
            }
            contentItem: Label {
                text: parent.text
                color: parent.highlighted ? "#f7fbff" : "#b8cfdd"
                font.pixelSize: 16
                font.weight: parent.highlighted ? Font.DemiBold : Font.Medium
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: activateSection("utxos")
        }

        Button {
            width: parent.width
            text: walletRoot.tf("browser_wallet_nav_statement", "Statement of Account")
            leftPadding: 18
            rightPadding: 18
            topPadding: 14
            bottomPadding: 14
            highlighted: walletRoot.activeSection === "statement"
            background: Rectangle {
                radius: 16
                color: parent.highlighted ? "#173247" : "#111c26"
                border.color: parent.highlighted ? "#4d89aa" : "#223847"
            }
            contentItem: Label {
                text: parent.text
                color: parent.highlighted ? "#f7fbff" : "#b8cfdd"
                font.pixelSize: 16
                font.weight: parent.highlighted ? Font.DemiBold : Font.Medium
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: activateSection("statement")
        }

        Button {
            width: parent.width
            text: walletRoot.tf("browser_wallet_nav_send_receive", "Send / Receive")
            leftPadding: 18
            rightPadding: 18
            topPadding: 14
            bottomPadding: 14
            highlighted: walletRoot.activeSection === "slatepack"
            background: Rectangle {
                radius: 16
                color: parent.highlighted ? "#173247" : "#111c26"
                border.color: parent.highlighted ? "#4d89aa" : "#223847"
            }
            contentItem: Label {
                text: parent.text
                color: parent.highlighted ? "#f7fbff" : "#b8cfdd"
                font.pixelSize: 16
                font.weight: parent.highlighted ? Font.DemiBold : Font.Medium
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: activateSection("slatepack")
        }

        Button {
            width: parent.width
            text: walletRoot.tf("browser_wallet_nav_settings", "Settings")
            leftPadding: 18
            rightPadding: 18
            topPadding: 14
            bottomPadding: 14
            highlighted: walletRoot.activeSection === "settings"
            background: Rectangle {
                radius: 16
                color: parent.highlighted ? "#173247" : "#111c26"
                border.color: parent.highlighted ? "#4d89aa" : "#223847"
            }
            contentItem: Label {
                text: parent.text
                color: parent.highlighted ? "#f7fbff" : "#b8cfdd"
                font.pixelSize: 16
                font.weight: parent.highlighted ? Font.DemiBold : Font.Medium
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: activateSection("settings")
        }

        Item {
            width: 1
            height: 1
            Layout.fillHeight: true
        }

        Button {
            width: parent.width
            text: walletRoot.tf("browser_wallet_back_lock", "Back (Lock)")
            leftPadding: 18
            rightPadding: 18
            topPadding: 14
            bottomPadding: 14
            background: Rectangle {
                radius: 16
                color: "#111c26"
                border.color: "#223847"
            }
            contentItem: Label {
                text: parent.text
                color: "#b8cfdd"
                font.pixelSize: 16
                font.weight: Font.Medium
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: handleBackLock()
        }
    }
}