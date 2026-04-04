import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

BrowserWalletSectionCard {
    id: backupCard
    property var walletRoot

    width: parent ? parent.width : 0
    fillColor: "#143326"
    strokeColor: "#2d7055"
    title: walletRoot.tf("browser_wallet_backup_title", "Write Down Your Seed Phrase Now")
    subtitle: walletRoot.tf("browser_wallet_backup_note", "This is the only time the seed phrase is shown automatically. Store it offline and keep it away from screenshots, chat logs, and cloud notes.")
    subtitleColor: "#d9f8e9"
    visible: grinWalletController.mnemonicPreview.length > 0

    ColumnLayout {
        id: backupColumn
        width: parent.width
        spacing: 12

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