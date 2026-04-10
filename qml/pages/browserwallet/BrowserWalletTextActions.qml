import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Flow {
    id: textActions

    property var walletRoot
    property var targetControl
    property bool allowCopy: true
    property bool allowPaste: true
    property bool allowCut: true

    width: parent ? parent.width : implicitWidth
    spacing: 8
    visible: !!walletRoot && !!targetControl

    Button {
        text: walletRoot ? walletRoot.tf("browser_wallet_select_all", "Select All") : "Select All"
        font.pixelSize: walletRoot ? walletRoot.compactTextSize : 12
        enabled: targetControl && walletRoot && walletRoot.textControlHasText(targetControl)
        onClicked: walletRoot.selectAllTextControl(targetControl)
    }

    Button {
        text: walletRoot ? walletRoot.tf("browser_wallet_copy", "Copy") : "Copy"
        font.pixelSize: walletRoot ? walletRoot.compactTextSize : 12
        visible: allowCopy
        enabled: targetControl && walletRoot && walletRoot.textControlHasText(targetControl)
        onClicked: walletRoot.copyTextControl(targetControl)
    }

    Button {
        text: walletRoot ? walletRoot.tf("browser_wallet_cut", "Cut") : "Cut"
        font.pixelSize: walletRoot ? walletRoot.compactTextSize : 12
        visible: allowCut
        enabled: targetControl && walletRoot
                 && !walletRoot.textControlIsReadOnly(targetControl)
                 && walletRoot.textControlHasText(targetControl)
        onClicked: walletRoot.cutTextControl(targetControl)
    }

    Button {
        text: walletRoot ? walletRoot.tf("browser_wallet_paste", "Paste") : "Paste"
        font.pixelSize: walletRoot ? walletRoot.compactTextSize : 12
        visible: allowPaste
        enabled: targetControl && walletRoot && !walletRoot.textControlIsReadOnly(targetControl)
        onClicked: walletRoot.pasteIntoTextControl(targetControl)
    }
}