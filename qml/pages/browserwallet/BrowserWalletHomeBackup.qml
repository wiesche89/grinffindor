import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: backupCard
    property var walletRoot

    width: parent ? parent.width : 0
    radius: 26
    color: "#143326"
    border.color: "#2d7055"
    visible: grinWalletController.mnemonicPreview.length > 0
    implicitHeight: backupColumn.implicitHeight + 34

    ColumnLayout {
        id: backupColumn
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        Label {
            Layout.fillWidth: true
            text: walletRoot.tf("browser_wallet_backup_title", "Write Down Your Seed Phrase Now")
            color: "#ffffff"
            font.pixelSize: 26
            font.weight: Font.Bold
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            text: walletRoot.tf("browser_wallet_backup_note", "This is the only time the seed phrase is shown automatically. Store it offline and keep it away from screenshots, chat logs, and cloud notes.")
            color: "#d9f8e9"
            wrapMode: Text.WordWrap
        }

        TextArea {
            id: mnemonicPreviewArea
            Layout.fillWidth: true
            Layout.preferredHeight: 116
            readOnly: true
            wrapMode: TextEdit.Wrap
            textFormat: TextEdit.PlainText
            selectByMouse: true
            persistentSelection: true
            activeFocusOnPress: true
            text: grinWalletController.mnemonicPreview
            onActiveFocusChanged: walletRoot.syncBrowserShortcutContext(mnemonicPreviewArea)
            onSelectedTextChanged: walletRoot.syncBrowserShortcutContext(mnemonicPreviewArea)
            onTextChanged: walletRoot.syncBrowserShortcutContext(mnemonicPreviewArea)
            Keys.onPressed: function(event) { walletRoot.handleTextControlKeyPress(mnemonicPreviewArea, event) }
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }

            Button {
                text: walletRoot.tf("browser_wallet_backup_done", "I saved it")
                onClicked: grinWalletController.dismissMnemonicPreview()
            }
        }
    }
}