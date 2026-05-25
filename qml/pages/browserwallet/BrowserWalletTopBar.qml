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

    function statusDotColor() {
        if (walletRoot.nodeStatusMode() === "offline")    return "#dd4050"
        if (walletRoot.nodeStatusMode() === "connecting") return "#e0a840"
        return "#FEF102"
    }

    signal menuRequested()
    signal backRequested()

    height: walletRoot.veryPhoneMode ? 50 : (walletRoot.phoneMode ? 56 : 64)
    color: "#070e18"
    border.color: "#10253a"
    border.width: 1
    z: 2

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: walletRoot.pageMargin
        anchors.rightMargin: walletRoot.pageMargin
        spacing: walletRoot.veryPhoneMode ? 8 : 12

        Rectangle {
            implicitWidth: walletRoot.veryPhoneMode ? 76 : (walletRoot.phoneMode ? 88 : 108)
            implicitHeight: walletRoot.veryPhoneMode ? 32 : (walletRoot.phoneMode ? 36 : 42)
            Layout.alignment: Qt.AlignVCenter
            radius: walletRoot.veryPhoneMode ? 12 : 14
            color: backMa.pressed ? "#0e2030" : (backMa.containsMouse ? "#0a1a28" : "#0dffffff")
            border.color: backMa.containsMouse ? "#1e3a50" : "#102030"
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: walletRoot.tf("back", "Back")
                color: "#f4f7ff"
                font.pixelSize: walletRoot.veryPhoneMode ? 13 : 15
                font.weight: Font.DemiBold
            }

            MouseArea {
                id: backMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: topBar.backRequested()
            }
        }

        // Hamburger menu (compact / mobile)
        Item {
            visible: walletRoot.compactNavigation && grinWalletController.walletUnlocked
            width: menuBtn.implicitWidth
            height: parent.height

            Rectangle {
                id: menuBtn
                anchors.verticalCenter: parent.verticalCenter
                implicitWidth: walletRoot.veryPhoneMode ? 34 : 38
                implicitHeight: walletRoot.veryPhoneMode ? 30 : 34
                radius: 8
                color: menuMa.pressed ? "#0e2030" : (menuMa.containsMouse ? "#0a1a28" : "transparent")
                border.color: menuMa.containsMouse ? "#1e3a50" : "#102030"

                Column {
                    anchors.centerIn: parent
                    spacing: 4

                    Repeater {
                        model: 3
                        Rectangle {
                            width: walletRoot.veryPhoneMode ? 14 : 16
                            height: 2
                            radius: 1
                            color: menuMa.containsMouse ? "#88c0d8" : "#4a7898"
                        }
                    }
                }

                MouseArea {
                    id: menuMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: topBar.menuRequested()
                }
            }
        }

        Item { Layout.fillWidth: true }

        // Sync status chip
        Row {
            spacing: 7
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

            // Status dot
            Rectangle {
                width: 7
                height: 7
                radius: 4
                anchors.verticalCenter: parent.verticalCenter
                color: topBar.statusDotColor()
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: topBar.statusText()
                color: walletRoot.nodeStatusMode() === "offline"    ? "#dd6070"
                     : walletRoot.nodeStatusMode() === "connecting" ? "#c89038"
                     : "#4a8ea8"
                font.pixelSize: walletRoot.compactTextSize
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }
    }
}
