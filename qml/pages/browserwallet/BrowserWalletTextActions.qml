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
    spacing: 6
    visible: !!walletRoot && !!targetControl

    Repeater {
        model: [
            {
                label:   walletRoot ? walletRoot.tf("browser_wallet_select_all", "Select All") : "Select All",
                visible: true,
                enabled: targetControl && walletRoot && walletRoot.textControlHasText(targetControl),
                action:  function() { walletRoot.selectAllTextControl(targetControl) }
            },
            {
                label:   walletRoot ? walletRoot.tf("browser_wallet_copy", "Copy") : "Copy",
                visible: allowCopy,
                enabled: targetControl && walletRoot && walletRoot.textControlHasText(targetControl),
                action:  function() { walletRoot.copyTextControl(targetControl) }
            },
            {
                label:   walletRoot ? walletRoot.tf("browser_wallet_cut", "Cut") : "Cut",
                visible: allowCut,
                enabled: targetControl && walletRoot
                         && !walletRoot.textControlIsReadOnly(targetControl)
                         && walletRoot.textControlHasText(targetControl),
                action:  function() { walletRoot.cutTextControl(targetControl) }
            },
            {
                label:   walletRoot ? walletRoot.tf("browser_wallet_paste", "Paste") : "Paste",
                visible: allowPaste,
                enabled: targetControl && walletRoot && !walletRoot.textControlIsReadOnly(targetControl),
                action:  function() { walletRoot.pasteIntoTextControl(targetControl) }
            }
        ]

        Button {
            text: modelData.label
            font.pixelSize: walletRoot ? walletRoot.compactTextSize : 12
            visible: modelData.visible
            enabled: modelData.enabled
            background: Rectangle {
                radius: 8
                color: parent.down ? "#07111c" : (parent.hovered ? "#060e18" : "transparent")
                border.color: parent.hovered ? "#1e3a52" : "#152a3c"
                border.width: 1
            }
            contentItem: Label {
                text: parent.text; font: parent.font
                color: !parent.enabled ? "#283c50" : (parent.hovered ? "#88b8d8" : "#5a8eac")
                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            }
            onClicked: modelData.action()
        }
    }
}
