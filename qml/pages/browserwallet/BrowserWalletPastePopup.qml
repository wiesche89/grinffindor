import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../components" as AppComponents

Popup {
    id: pasteInputPopup
    property var walletRoot

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: Math.min(walletRoot.width - 28, 740)
    padding: 0

    onOpened: {
        if (pasteInputArea.bridgeId !== undefined)
            PlatformBridge.requestFocus(pasteInputArea.bridgeId)
    }

    onClosed: {
        walletRoot.pasteTargetControl = null
        walletRoot.pasteDialogTitle = ""
        walletRoot.pasteDialogPlaceholder = ""
        walletRoot.pasteDialogText = ""
    }

    background: Rectangle {
        radius: 22
        color: "#0d1c2a"
        border.color: "#1e3d55"
        border.width: 1
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 22
        spacing: 14

        Label {
            Layout.fillWidth: true
            text: walletRoot.pasteDialogTitle.length > 0
                  ? walletRoot.pasteDialogTitle
                  : walletRoot.tf("browser_wallet_paste_generic_title", "Paste Text")
            color: "#ddeeff"
            font.pixelSize: 24
            font.weight: Font.Bold
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            text: walletRoot.tf("browser_wallet_paste_generic_note", "Enter or paste the full content below. Multi-line input is supported.")
            color: "#3a6a84"
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }

        AppComponents.AppTextArea {
            id: pasteInputArea
            Layout.fillWidth: true
            Layout.preferredHeight: 240
            editorTitle: walletRoot.pasteDialogTitle.length > 0
                         ? walletRoot.pasteDialogTitle
                         : walletRoot.tf("browser_wallet_paste_generic_title", "Paste Text")
            wrapMode: TextEdit.WrapAnywhere
            selectByMouse: true
            persistentSelection: true
            activeFocusOnPress: true
            text: walletRoot.pasteDialogText
            placeholderText: walletRoot.pasteDialogPlaceholder
            onTextChanged: walletRoot.pasteDialogText = text
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }

            Button {
                text: walletRoot.tf("browser_wallet_cancel", "Cancel")
                font.pixelSize: 13
                background: Rectangle {
                    radius: 10; color: parent.down ? "#07111c" : (parent.hovered ? "#060e18" : "transparent")
                    border.color: parent.hovered ? "#1e3a52" : "#152a3c"; border.width: 1
                }
                contentItem: Label {
                    text: parent.text; font: parent.font; color: parent.hovered ? "#88b8d8" : "#5a8eac"
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                onClicked: pasteInputPopup.close()
            }

            Button {
                text: walletRoot.tf("browser_wallet_apply_paste", "Apply")
                font.pixelSize: 13
                font.weight: Font.DemiBold
                enabled: walletRoot.pasteDialogText.trim().length > 0
                background: Rectangle {
                    radius: 10
                    color: !parent.enabled ? "#181200" : (parent.down ? "#201800" : (parent.hovered ? "#1a1500" : "#181200"))
                    border.color: !parent.enabled ? "#181000" : (parent.down ? "#A08800" : (parent.hovered ? "#C09800" : "#806800"))
                    border.width: 1
                }
                contentItem: Label {
                    text: parent.text; font: parent.font
                    color: !parent.enabled ? "#605000" : (parent.down ? "#FEF102" : (parent.hovered ? "#FEF102" : "#FEF102"))
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                onClicked: walletRoot.applyPasteDialog()
            }
        }
    }
}
