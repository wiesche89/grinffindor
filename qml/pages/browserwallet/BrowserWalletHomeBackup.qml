import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../components" as AppComponents

BrowserWalletSectionCard {
    id: backupCard
    property var walletRoot

    width: parent ? parent.width : 0
    fillColor: "#181200"
    strokeColor: "#806800"
    titleColor: "#FEF102"
    subtitleColor: "#A09000"
    title: walletRoot.tf("browser_wallet_backup_title", "Write Down Your Seed Phrase Now")
    subtitle: walletRoot.tf("browser_wallet_backup_note", "This is the only time the seed phrase is shown automatically. Store it offline and keep it away from screenshots, chat logs, and cloud notes.")
    visible: grinWalletController.mnemonicPreview.length > 0

    ColumnLayout {
        id: backupColumn
        width: parent.width
        spacing: 12

        AppComponents.AppTextArea {
            id: mnemonicPreviewArea
            Layout.fillWidth: true
            Layout.preferredHeight: 112
            editorTitle: walletRoot.tf("browser_wallet_backup_title", "Write Down Your Seed Phrase Now")
            readOnly: true
            font.pixelSize: walletRoot.controlTextSize
            wrapMode: TextEdit.Wrap
            textFormat: TextEdit.PlainText
            selectByMouse: true
            persistentSelection: true
            activeFocusOnPress: true
            text: grinWalletController.mnemonicPreview
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }

            Button {
                text: walletRoot.tf("browser_wallet_backup_done", "I saved it")
                font.pixelSize: walletRoot.controlTextSize
                background: Rectangle {
                    radius: 10
                    color: !parent.enabled ? "#181200" : (parent.down ? "#201800" : (parent.hovered ? "#1a1500" : "#181200"))
                    border.color: !parent.enabled ? "#181000" : (parent.down ? "#A08800" : (parent.hovered ? "#C09800" : "#806800"))
                    border.width: 1
                }
                contentItem: Label {
                    text: parent.text; font: parent.font
                    color: parent.down ? "#FEF102" : (parent.hovered ? "#FEF102" : "#FEF102")
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                onClicked: grinWalletController.dismissMnemonicPreview()
            }
        }
    }
}
