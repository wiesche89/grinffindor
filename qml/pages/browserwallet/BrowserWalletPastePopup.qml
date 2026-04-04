import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: pasteInputPopup
    property var walletRoot

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: Math.min(walletRoot.width - 28, 760)
    padding: 0

    onClosed: {
        walletRoot.pasteTargetControl = null
        walletRoot.pasteDialogTitle = ""
        walletRoot.pasteDialogPlaceholder = ""
        walletRoot.pasteDialogText = ""
    }

    background: Rectangle {
        radius: 28
        color: "#0f1b26"
        border.color: "#2a4f64"
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        Label {
            Layout.fillWidth: true
            text: walletRoot.pasteDialogTitle.length > 0
                  ? walletRoot.pasteDialogTitle
                  : walletRoot.tf("browser_wallet_paste_generic_title", "Paste Text")
            color: "#ffffff"
            font.pixelSize: 28
            font.weight: Font.Bold
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            text: walletRoot.tf("browser_wallet_paste_generic_note", "Enter or paste the full content below. Multi-line input is supported.")
            color: "#d7e9f4"
            wrapMode: Text.WordWrap
        }

        TextArea {
            id: pasteInputArea
            Layout.fillWidth: true
            Layout.preferredHeight: 260
            wrapMode: TextEdit.WrapAnywhere
            selectByMouse: true
            persistentSelection: true
            activeFocusOnPress: true
            text: walletRoot.pasteDialogText
            placeholderText: walletRoot.pasteDialogPlaceholder
            onActiveFocusChanged: walletRoot.syncBrowserShortcutContext(pasteInputArea)
            onSelectedTextChanged: walletRoot.syncBrowserShortcutContext(pasteInputArea)
            onTextChanged: {
                walletRoot.pasteDialogText = text
                walletRoot.syncBrowserShortcutContext(pasteInputArea)
            }
            Keys.onPressed: function(event) { walletRoot.handleTextControlKeyPress(pasteInputArea, event) }
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }

            Button {
                text: walletRoot.tf("browser_wallet_cancel", "Cancel")
                onClicked: pasteInputPopup.close()
            }

            Button {
                text: walletRoot.tf("browser_wallet_apply_paste", "Apply")
                enabled: walletRoot.pasteDialogText.trim().length > 0
                onClicked: walletRoot.applyPasteDialog()
            }
        }
    }
}