// Main.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

ApplicationWindow {
    id: root
    width: 960
    height: 640
    visible: true
    title: "Grinffindor"
    property string activeTilePage: ""

    function showTilePage(pageFile) {
        if (!pageFile)
            return
        activeTilePage = pageFile
    }

    function returnToMain() {
        activeTilePage = ""
    }

    // --- Tile sizing (only tune these) ---
    readonly property int tileW: 300             // etwas breiter -> mehr "Card"-Look
    readonly property real tileAspect: 1030/770  // dein PNG: 768x1024
    readonly property int tileH: Math.ceil(tileW * tileAspect)

    readonly property int rowPadding: 18
    readonly property int tileSpacing: 10
    readonly property int scrollBarH: 8

    ListModel { id: tileModel }

    Component.onCompleted: {
        tileModel.clear()

        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("API"),
            imageSource: "qrc:/res/media/tiles/tile_api.PNG",
            pageFile: "TileAPI.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("ATOMIC SWAPS"),
            imageSource: "qrc:/res/media/tiles/tile_atomic_swaps.PNG",
            pageFile: "TileAtomicSwaps.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("BOOK"),
            imageSource: "qrc:/res/media/tiles/tile_book.PNG",
            pageFile: "TileBook.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("BOUNTIE"),
            imageSource: "qrc:/res/media/tiles/tile_bountie.PNG",
            pageFile: "TileBountie.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("COCKATOO"),
            imageSource: "qrc:/res/media/tiles/tile_cockatoo.PNG",
            pageFile: "TileCockatoo.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("CONTRACTS"),
            imageSource: "qrc:/res/media/tiles/tile_contracts.PNG",
            pageFile: "TileContracts.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("DEVELOPMENT"),
            imageSource: "qrc:/res/media/tiles/tile_development.PNG",
            pageFile: "TileDevelopment.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("EMISSION"),
            imageSource: "qrc:/res/media/tiles/tile_emission.PNG",
            pageFile: "TileEmission.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("EXCHANGE"),
            imageSource: "qrc:/res/media/tiles/tile_exchange.PNG",
            pageFile: "TileExchange.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("EXPLORER"),
            imageSource: "qrc:/res/media/tiles/tile_explorer.PNG",
            pageFile: "TileExplorer.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("GOVERNANCE"),
            imageSource: "qrc:/res/media/tiles/tile_governance.PNG",
            pageFile: "TileGovernance.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("GRINFFINDOR"),
            imageSource: "qrc:/res/media/tiles/tile_grinffindor.PNG",
            pageFile: "TileGrinffindor.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("GRIN NODE"),
            imageSource: "qrc:/res/media/tiles/tile_grin_node.PNG",
            pageFile: "TileGrinNode.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("GRYFFINDOR"),
            imageSource: "qrc:/res/media/tiles/tile_gryffindor.PNG",
            pageFile: "TileGryffindor.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("HARDWARE WALLET"),
            imageSource: "qrc:/res/media/tiles/tile_hardware_wallet.PNG",
            pageFile: "TileHardwareWallet.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("IGNOTUS"),
            imageSource: "qrc:/res/media/tiles/tile_ignotus.PNG",
            pageFile: "TileIgnotus.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("INFLATION"),
            imageSource: "qrc:/res/media/tiles/tile_inflation.PNG",
            pageFile: "TileInflation.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("LOGIN"),
            imageSource: "qrc:/res/media/tiles/tile_login.PNG",
            pageFile: "TileLogin.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("MAINNET FAUCET"),
            imageSource: "qrc:/res/media/tiles/tile_mainnet_faucet.PNG",
            pageFile: "TileMainnetFaucet.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("MINING"),
            imageSource: "qrc:/res/media/tiles/tile_mining.PNG",
            pageFile: "TileMining.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("MIXER"),
            imageSource: "qrc:/res/media/tiles/tile_mixer.PNG",
            pageFile: "TileMixer.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("MULTISIG"),
            imageSource: "qrc:/res/media/tiles/tile_multisig.PNG",
            pageFile: "TileMultisig.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("MULTISIG COIN"),
            imageSource: "qrc:/res/media/tiles/tile_multisig_coin.PNG",
            pageFile: "TileMultisigCoin.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("MWIXNET"),
            imageSource: "qrc:/res/media/tiles/tile_mwixnet.PNG",
            pageFile: "TileMwixnet.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("MW BOOK"),
            imageSource: "qrc:/res/media/tiles/tile_mw_book.PNG",
            pageFile: "TileMwBook.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("POOL"),
            imageSource: "qrc:/res/media/tiles/tile_pool.PNG",
            pageFile: "TilePool.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("SERVER"),
            imageSource: "qrc:/res/media/tiles/tile_server.PNG",
            pageFile: "TileServer.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("SETTINGS"),
            imageSource: "qrc:/res/media/tiles/tile_settings.PNG",
            pageFile: "TileSettings.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("SHOP"),
            imageSource: "qrc:/res/media/tiles/tile_shop.PNG",
            pageFile: "TileShop.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("SUPPORT"),
            imageSource: "qrc:/res/media/tiles/tile_support.PNG",
            pageFile: "TileSupport.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("SWAP"),
            imageSource: "qrc:/res/media/tiles/tile_swap.PNG",
            pageFile: "TileSwap.qml"
        })
        tileModel.append({
            titleText: qsTr(""),
            subtitleText: qsTr(""),
            buttonText: qsTr("TESTNET FAUCET"),
            imageSource: "qrc:/res/media/tiles/tile_testnet_faucet.PNG",
            pageFile: "TileTestnetFaucet.qml"
        })
    }

    Item {
        anchors.fill: parent

        Image {
            anchors.fill: parent
            source: "qrc:/res/media/images/image_wallpaper.png"
            fillMode: Image.Stretch
            smooth: true
            z: -10
            visible: activeTilePage === ""
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            visible: activeTilePage === ""

            // Header
            Item {
                Layout.alignment: Qt.AlignHCenter
                width: Math.min(root.width * 0.85, 1200)
                height: headerLoader.item ? headerLoader.item.height : 78

                Loader {
                    id: headerLoader
                    anchors.fill: parent
                    source: "qrc:/qml/qml/Header.qml"
                    onLoaded: if (item) item.window = root
                }
            }

            // Content
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(parent.width * 0.92, 1120)
                    spacing: 18

                    // Hero (kleiner, Text zentriert)
                    Rectangle {
                        width: parent.width
                        height: 150
                        radius: 24
                        color: "#05050588"
                        border.color: "#05050588"
                        border.width: 1

                        ColumnLayout {
                            anchors.centerIn: parent
                            width: parent.width * 0.78
                            spacing: 8

                            Label {
                                text: qsTr("Grinffindor supports the Grin community with open tools and infrastructure. Explore testnet and mainnet faucets, run your own Grin node on UmbrelOS, and find everything you need to use, test, and build with Grin.")
                                font.pixelSize: 14
                                color: "#cfd6ff"
                                wrapMode: Text.WordWrap
                                horizontalAlignment: Text.AlignHCenter
                                Layout.fillWidth: true
                            }
                        }
                    }

                    // Tiles row (hoch genug -> kein Clip)
                    Item {
                        width: parent.width
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
                        }
                    }
                }
            }

            // Footer
            Item {
                Layout.alignment: Qt.AlignHCenter
                width: Math.min(root.width * 0.85, 1200)
                height: footerLoader.item ? footerLoader.item.height : 64

                Loader {
                    id: footerLoader
                    anchors.fill: parent
                    source: "qrc:/qml/qml/Footer.qml"
                    onLoaded: if (item) item.window = root
                }
            }
        }

        Loader {
            id: tilePageLoader
            anchors.fill: parent
            visible: activeTilePage !== ""
            source: activeTilePage ? "qrc:/qml/qml/pages/" + activeTilePage : ""
            asynchronous: true
            z: 10

            Connections {
                target: item
                onBackRequested: returnToMain()
            }
        }
    }
}
