import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: tile
    signal activated()

    property string titleText: ""
    property string subtitleText: ""
    property string buttonText: ""
    property url imageSource: ""

    // Außenabstand / Zoom wie gehabt
    property int  safePad: 0
    property real zoom: 1.10

    // Positionen relativ zur painted area
    property real titleY: 0.40
    property real btnY:   0.725
    property real btnX:   0.18
    property real btnW:   0.76
    property real btnH:   0.095

    // Rundung einstellen
    property real cornerRadius: 26

    // 1) Inhalt (Bild + Overlays) rendern wir in eine Texture
    Item {
        id: content
        anchors.fill: parent

        Image {
            id: bg
            anchors.fill: parent
            anchors.margins: tile.safePad
            source: tile.imageSource
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
            asynchronous: true
            cache: true

            // optional: wenn du wirklich zoomen willst, hier anwenden:
            // scale: tile.zoom
            // transformOrigin: Item.Center
        }

        // Helper: Koordinaten der painted area
        function pxX(rel) { return bg.x + rel * bg.paintedWidth }
        function pxY(rel) { return bg.y + rel * bg.paintedHeight }
        function pxW(rel) { return rel * bg.paintedWidth }
        function pxH(rel) { return rel * bg.paintedHeight }

        Text {
            visible: bg.paintedWidth > 0
            text: tile.titleText
            color: "#ffffff"
            font.pixelSize: 22
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            width: bg.paintedWidth * 0.92
            wrapMode: Text.NoWrap

            x: bg.x + (bg.paintedWidth - width) / 2
            y: content.pxY(tile.titleY) - height / 2
        }

        Item {
            id: btnArea
            visible: bg.paintedWidth > 0
            x: content.pxX(tile.btnX)
            y: content.pxY(tile.btnY)
            width: content.pxW(tile.btnW)
            height: content.pxH(tile.btnH)

            property bool hovered: false

            Text {
                anchors.centerIn: parent
                text: tile.buttonText
                font.pixelSize: 16
                font.bold: true
                color: btnArea.hovered ? "#e6dcff" : "#ffffff"
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onEntered: btnArea.hovered = true
                onExited: btnArea.hovered = false
                onClicked: {
                    console.log(tile.titleText + " clicked")
                    tile.activated()
                }
            }
        }
    }

    // 2) Runde Maske (Anti-Aliased)
    Rectangle {
        id: mask
        anchors.fill: parent
        radius: tile.cornerRadius
        color: "white"
        antialiasing: true
        visible: false
    }
}
