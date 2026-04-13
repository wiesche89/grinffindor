import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: errorPopup
    property var walletRoot

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: Math.min(walletRoot.width - 28, 580)
    padding: 0

    onClosed: grinWalletController.clearLastError()

    background: Rectangle {
        radius: 22
        color: "#14090e"
        border.color: "#7a2030"
        border.width: 1
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 22
        spacing: 14

        // Title row with icon
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                width: 28; height: 28; radius: 8
                color: "#2a0e14"
                border.color: "#7a2030"
                border.width: 1

                Label {
                    anchors.centerIn: parent
                    text: "!"
                    color: "#dd4050"
                    font.pixelSize: 16
                    font.weight: Font.Bold
                }
            }

            Label {
                Layout.fillWidth: true
                text: walletRoot.tf("browser_wallet_error_title", "Wallet Error")
                color: "#ddeeff"
                font.pixelSize: 22
                font.weight: Font.Bold
                wrapMode: Text.WordWrap
            }
        }

        Label {
            Layout.fillWidth: true
            text: walletRoot.errorDialogText
            color: "#c08090"
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }

            Button {
                text: walletRoot.tf("browser_wallet_error_close", "Close")
                font.pixelSize: 13
                background: Rectangle {
                    radius: 10; color: parent.down ? "#07111c" : (parent.hovered ? "#060e18" : "transparent")
                    border.color: parent.hovered ? "#1e3a52" : "#152a3c"; border.width: 1
                }
                contentItem: Label {
                    text: parent.text; font: parent.font; color: parent.hovered ? "#88b8d8" : "#5a8eac"
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                onClicked: errorPopup.close()
            }
        }
    }
}
