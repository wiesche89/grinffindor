import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root
    property string pageTitle: ""
    property url backgroundSource: "qrc:/res/media/images/image_wallpaper_tile.png"
    property var i18n: null
    signal backRequested()
    anchors.fill: parent

    Image {
        anchors.fill: parent
        source: "qrc:/res/media/images/image_wallpaper_tile.png"
        fillMode: Image.PreserveAspectCrop
        smooth: true
        asynchronous: true
        z: 0
    }

    Rectangle {
        anchors.fill: parent
        color: "#00000088"
        z: 1
    }

    Button {
        text: i18n ? i18n.tf("back", "Back") : "Back"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 18
        anchors.topMargin: 18
        onClicked: root.backRequested()
        z: 2
    }

    Column {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(parent.width * 0.8, 760)
        spacing: 14
        z: 2

        Label {
            text: root.pageTitle
            font.pixelSize: 42
            font.bold: true
            color: "#ffffff"
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Label {
            text: (i18n ? i18n.tf("tile_page_placeholder", "There is room here for additional information about %1.") : "There is room here for additional information about %1.").arg(root.pageTitle)
            font.pixelSize: 16
            color: "#dfe3ff"
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            width: Math.min(root.width * 0.75, 720)
        }
    }
}
