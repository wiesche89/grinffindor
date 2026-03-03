import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root
    property string pageTitle: ""
    signal backRequested()
    anchors.fill: parent

    Image {
        id: background
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
        id: backButton
        text: qsTr("Zurück")
        anchors {
            left: parent.left
            top: parent.top
            leftMargin: 18
            topMargin: 18
        }
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
            text: qsTr("Hier bleibt Platz für zusätzliche Informationen zum Thema %1.").arg(root.pageTitle)
            font.pixelSize: 16
            color: "#dfe3ff"
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            width: Math.min(root.width * 0.75, 720)
        }
    }
}
