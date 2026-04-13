import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: homeSection
    property var walletRoot

    implicitHeight: homeColumn.implicitHeight

    Column {
        id: homeColumn
        width: parent.width
        spacing: 18

        BrowserWalletHomeRecovery {
            width: parent.width
            walletRoot: homeSection.walletRoot
        }

        BrowserWalletHomeOverview {
            width: parent.width
            walletRoot: homeSection.walletRoot
        }

        BrowserWalletHomeHistory {
            width: parent.width
            walletRoot: homeSection.walletRoot
        }
    }
}
