// Main.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtCore

ApplicationWindow {
    id: root
    readonly property bool isWasm: Qt.platform.os === "wasm"
    readonly property bool isIos: Qt.platform.os === "ios"
    readonly property bool isNativeMobile: Qt.platform.os === "android" || isIos
    readonly property bool isMobileRuntime: isWasm || isNativeMobile || (typeof PlatformBridge !== "undefined" && PlatformBridge.mobile)

    // Safe area insets for iPhone notch / home indicator (Qt 6.7+)
    readonly property real iosSafeTop: isIos ? safeAreaInsets.top : 0
    readonly property real iosSafeBottom: isIos ? safeAreaInsets.bottom : 0
    function assetPath(path) {
        return (typeof assetBaseUrl === "string" ? assetBaseUrl : "qrc:/res/") + path
    }
    width: isMobileRuntime ? Screen.width : 960
    height: isMobileRuntime ? Screen.height : 640
    visible: true
    // iOS: Maximized keeps the system status bar (clock/battery/wifi) visible.
    // FullScreen would hide it. Android keeps FullScreen.
    visibility: isIos ? Window.Maximized
              : (isNativeMobile ? Window.FullScreen
              : (isWasm ? Window.Windowed : Window.Maximized))
    flags: isWasm ? (Qt.Window | Qt.FramelessWindowHint) : Qt.Window
    title: i18n ? i18n.tf("app_title", "Grinffindor") : "Grinffindor"
    property string activeTilePage: ""

    function showTilePage(pageFile) {
        if (!pageFile)
            return
        activeTilePage = pageFile
    }

    function returnToMain() {
        activeTilePage = ""
    }

    function populateTiles() {
        tileModel.clear()

        tileModel.append({
            titleText: "",
            subtitleText: "",
            buttonText: i18n.tf("tile_grinffindor", "Grinffindor"),
            imageSource: root.assetPath("media/tiles/tile_grinffindor.PNG"),
            pageFile: "TileGrinffindor.qml"
        })
        tileModel.append({
            titleText: "",
            subtitleText: "",
            buttonText: i18n.tf("tile_browser_wallet", "Browser Wallet"),
            imageSource: root.assetPath("media/tiles/tile_hardware_wallet.PNG"),
            pageFile: "TileBrowserWallet.qml"
        })
        tileModel.append({
            titleText: "",
            subtitleText: "",
            buttonText: i18n.tf("tile_wallets", "Wallets"),
            imageSource: root.assetPath("media/tiles/tile_login.PNG"),
            pageFile: "TileWallets.qml"
        })
        tileModel.append({
            titleText: "",
            subtitleText: "",
            buttonText: i18n.tf("tile_grin_node", "UmbrelOS"),
            imageSource: root.assetPath("media/tiles/tile_grin_node.PNG"),
            pageFile: "TileGrinNode.qml"
        })
        tileModel.append({
            titleText: "",
            subtitleText: "",
            buttonText: i18n.tf("tile_runtime_monitor", "Runtime Monitor"),
            imageSource: root.assetPath("media/tiles/tile_server.PNG"),
            pageFile: "TileRuntimeMonitor.qml"
        })
        tileModel.append({
            titleText: "",
            subtitleText: "",
            buttonText: i18n.tf("tile_mainnet_faucet", "Mainnet Bot"),
            imageSource: root.assetPath("media/tiles/tile_mainnet_faucet.PNG"),
            pageFile: "TileMainnetFaucet.qml"
        })
        tileModel.append({
            titleText: "",
            subtitleText: "",
            buttonText: i18n.tf("tile_testnet_faucet", "Testnet Bot"),
            imageSource: root.assetPath("media/tiles/tile_testnet_faucet.PNG"),
            pageFile: "TileTestnetFaucet.qml"
        })
        tileModel.append({
            titleText: "",
            subtitleText: "",
            buttonText: i18n.tf("tile_support", "Support"),
            imageSource: root.assetPath("media/tiles/tile_support.PNG"),
            pageFile: "TileSupport.qml"
        })
        tileModel.append({
            titleText: "",
            subtitleText: "",
            buttonText: i18n.tf("tile_exchange", "Exchange"),
            imageSource: root.assetPath("media/tiles/tile_exchange.PNG"),
            pageFile: "TileExchange.qml"
        })
        tileModel.append({
            titleText: "",
            subtitleText: "",
            buttonText: i18n.tf("tile_explorer", "Explorer"),
            imageSource: root.assetPath("media/tiles/tile_explorer.PNG"),
            pageFile: "TileExplorer.qml"
        })
        tileModel.append({
            titleText: "",
            subtitleText: "",
            buttonText: i18n.tf("tile_settings", "Settings"),
            imageSource: root.assetPath("media/tiles/tile_settings.PNG"),
            pageFile: "TileSettings.qml"
        })
    }

    readonly property int tileW: Math.max(190, Math.min(300, Math.floor(root.width * (root.width < 700 ? 0.62 : 0.31))))
    readonly property real tileAspect: 1030 / 770
    readonly property int tileH: Math.ceil(tileW * tileAspect)

    readonly property int rowPadding: 18
    readonly property int tileSpacing: 10
    readonly property int scrollBarH: 8

    Settings {
        id: appSettings
        category: "grinffindor"

        property string languageCode: "en"
        property string runtimePrometheusUrl: "https://prometheus.grinffindor.org"
    }

    I18n {
        id: i18n
        settingsStore: appSettings
    }

    ListModel { id: tileModel }

    Component.onCompleted: {
        if (isWasm || isIos) {
            root.x = 0
            root.y = 0
        }
        populateTiles()
    }

    Connections {
        target: i18n
        function onLoadedChanged() {
            if (i18n.loaded)
                root.populateTiles()
        }
    }

    // Wallpaper covers the full window including iOS safe areas (status bar + home indicator)
    background: Rectangle {
        color: "#080f18"
        Image {
            anchors.fill: parent
            source: root.assetPath("media/images/image_wallpaper_tile.png")
            fillMode: Image.Stretch
            smooth: true
        }
    }

    Item {
        anchors.fill: parent

        Item {
            id: headerHost
            width: root.width
            height: (headerLoader.item ? headerLoader.item.height : 78) + root.iosSafeTop
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter

            Loader {
                id: headerLoader
                anchors.fill: parent
                anchors.topMargin: root.iosSafeTop
                source: "qrc:/qml/qml/Header.qml"
                onLoaded: {
                    if (!item)
                        return
                    item.window = root
                    item.i18n = i18n
                }
            }

            Connections {
                target: headerLoader.item
                ignoreUnknownSignals: true
                function onSettingsRequested() {
                    root.showTilePage("TileSettings.qml")
                }
            }
        }

        Item {
            id: contentHost
            anchors.top: headerHost.bottom
            anchors.topMargin: Math.max(6, Math.round(root.height * 0.015))
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: footerHost.top
            anchors.bottomMargin: Math.max(10, Math.round(root.height * 0.02))

            Flickable {
                id: contentFlick
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                clip: true
                contentWidth: width
                contentHeight: contentInner.height + Math.max(12, Math.round(root.height * 0.015))
                flickableDirection: Flickable.VerticalFlick

                WheelHandler {
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                    onWheel: function(event) {
                        contentFlick.contentY = Math.max(0,
                            Math.min(contentFlick.contentHeight - contentFlick.height,
                                     contentFlick.contentY - event.angleDelta.y))
                    }
                }

                Item {
                id: contentInner
                width: Math.min(contentFlick.width * 0.92, 1120)
                height: heroCard.height + tilesRow.height + 10
                x: Math.round((contentFlick.width - width) / 2)
                y: Math.max(6, Math.round(root.height * 0.01))

                Rectangle {
                    id: heroCard
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width
                    height: root.width < 700 ? 138 : 118
                    radius: 24
                    color: "#05050588"
                    border.color: "#05050588"
                    border.width: 1

                    Label {
                        text: i18n.tf("hero_text", "Grinffindor supports the Grin community with open tools and infrastructure. Explore testnet and mainnet faucets, run your own Grin node on UmbrelOS, and find everything you need to use, test, and build with Grin.")
                        anchors.centerIn: parent
                        width: parent.width * (root.width < 700 ? 0.88 : 0.74)
                        font.pixelSize: root.width < 700 ? 13 : 14
                        color: "#cfd6ff"
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                Item {
                    id: tilesRow
                    anchors.top: heroCard.bottom
                    anchors.topMargin: 10
                    x: -contentInner.x
                    width: contentFlick.width
                    height: root.tileH + root.scrollBarH + root.rowPadding * 2 + 10

                    ListView {
                        id: tileList
                        anchors.fill: parent
                        anchors.margins: root.rowPadding
                        orientation: ListView.Horizontal
                        spacing: root.tileSpacing
                        clip: true

                        model: tileModel
                        boundsBehavior: Flickable.StopAtBounds
                        flickableDirection: Flickable.HorizontalFlick

                        delegate: Tile {
                            width: root.tileW
                            height: root.tileH
                            titleText: model.titleText
                            subtitleText: model.subtitleText
                            buttonText: model.buttonText
                            imageSource: model.imageSource
                            onActivated: root.showTilePage(model.pageFile)
                        }

                        ScrollBar.horizontal: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: root.scrollBarH
                        }

                        WheelHandler {
                            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                            onWheel: function(event) {
                                var delta = event.angleDelta.x !== 0 ? -event.angleDelta.x : -event.angleDelta.y
                                tileList.contentX = Math.max(0,
                                    Math.min(tileList.contentWidth - tileList.width,
                                             tileList.contentX + delta))
                            }
                        }
                    }
                }
            }
            }
        }

        Item {
            id: footerHost
            width: root.width
            height: footerLoader.item ? footerLoader.item.height : 64
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Math.max(10, Math.round(root.height * 0.025)) + root.iosSafeBottom
            anchors.horizontalCenter: parent.horizontalCenter

            Loader {
                id: footerLoader
                anchors.fill: parent
                source: "qrc:/qml/qml/Footer.qml"
                onLoaded: {
                    if (!item)
                        return
                    item.window = root
                    item.i18n = i18n
                    item.nodeStatus = nodeFooterStatus
                }
            }
        }

        Loader {
            id: tilePageLoader
            anchors.fill: parent
            active: activeTilePage !== ""
            visible: active
            source: active ? "qrc:/qml/qml/pages/" + activeTilePage : ""
            asynchronous: false
            z: 10

            onLoaded: {
                if (!item)
                    return
                if (item.i18n !== undefined)
                    item.i18n = i18n
                if (item.settingsStore !== undefined)
                    item.settingsStore = appSettings
            }

            Connections {
                target: tilePageLoader.item
                ignoreUnknownSignals: true
                function onBackRequested() {
                    root.returnToMain()
                }
            }
        }
    }
}
