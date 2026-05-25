import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    property var i18n: null
    signal backRequested()

    function assetPath(path) {
        return (typeof assetBaseUrl === "string" ? assetBaseUrl : "qrc:/res/") + path
    }

    anchors.fill: parent
    readonly property bool compact: width < 760
    readonly property bool singleColumn: width < 980
    readonly property int pageGutter: compact ? 16 : 28
    readonly property color glassBorder: "#1fd8e1f0"

    Rectangle { anchors.fill: parent; color: "#05070b" }

    Image {
        anchors.fill: parent
        source: root.assetPath("media/images/image_wallpaper_tile.png")
        fillMode: Image.PreserveAspectCrop
        smooth: true
        asynchronous: true
    }

    Rectangle { anchors.fill: parent; color: "#730a0e14" }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#a00b1017" }
            GradientStop { position: 0.42; color: "#780e141d" }
            GradientStop { position: 1.0; color: "#b0090d14" }
        }
    }

    Rectangle {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: compact ? 64 : 76
        color: "#d80b1018"
        border.color: glassBorder
        z: 10

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: pageGutter
            anchors.rightMargin: pageGutter
            spacing: 14

            Rectangle {
                implicitWidth: compact ? 88 : 108
                implicitHeight: compact ? 38 : 42
                radius: 14
                color: backMouse.containsMouse ? "#14ffffff" : "#0dffffff"
                border.color: glassBorder

                Text {
                    anchors.centerIn: parent
                    text: i18n ? i18n.tf("back", "Back") : "Back"
                    color: "#f4f7ff"
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }

                MouseArea {
                    id: backMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.backRequested()
                    cursorShape: Qt.PointingHandCursor
                }
            }

            Item { Layout.fillWidth: true }
        }
    }

    Flickable {
        id: scrollView
        anchors.fill: parent
        anchors.topMargin: topBar.height
        clip: true
        contentWidth: width
        contentHeight: contentColumn.implicitHeight + 56

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Column {
            id: contentColumn
            width: Math.min(scrollView.width - (compact ? 20 : 48), 1120)
            x: Math.round((scrollView.width - width) / 2)
            y: compact ? 16 : 28
            spacing: compact ? 20 : 28

            Rectangle {
                width: parent.width
                radius: 30
                color: "#111318b8"
                border.color: "#6b738066"
                border.width: 1
                implicitHeight: heroColumn.implicitHeight + 34

                Column {
                    id: heroColumn
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 12

                    Label {
                        text: i18n ? i18n.tf("wallets_page_eyebrow", "Wallets and node software") : "Wallets and node software"
                        color: "#b8becf"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("wallets_page_title", "Wallets") : "Wallets"
                        font.pixelSize: scrollView.width < 700 ? 30 : 42
                        font.bold: true
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width * 0.9
                        text: i18n ? i18n.tf("wallets_page_intro", "Choose the official Grin command-line tools or a community GUI wallet. The official grin and grin-wallet downloads do not include a graphical interface.") : "Choose the official Grin command-line tools or a community GUI wallet. The official grin and grin-wallet downloads do not include a graphical interface."
                        color: "#e8ebff"
                        font.pixelSize: 18
                        wrapMode: Text.WordWrap
                    }
                }
            }

            GridLayout {
                width: parent.width
                columns: root.width < 620 ? 1 : (root.singleColumn ? 2 : 3)
                columnSpacing: 16
                rowSpacing: 16

                Repeater {
                    model: [
                        {
                            title: i18n ? i18n.tf("wallets_rust_title", "Grin Rust Node + CLI Wallet") : "Grin Rust Node + CLI Wallet",
                            url: "https://grin.mw/download",
                            body: i18n ? i18n.tf("wallets_rust_body", "Official Grin downloads for the Rust node and command-line grin-wallet. Use this when you want the reference tools and are comfortable with terminal-based setup.") : "Official Grin downloads for the Rust node and command-line grin-wallet. Use this when you want the reference tools and are comfortable with terminal-based setup.",
                            platforms: i18n ? i18n.tf("wallets_rust_platforms", "Windows, macOS, Linux. Homebrew and Snap options are listed on the official download page.") : "Windows, macOS, Linux. Homebrew and Snap options are listed on the official download page."
                        },
                        {
                            title: "Grim",
                            url: "https://getgrin.github.io",
                            body: i18n ? i18n.tf("wallets_grim_body", "Community GUI node and wallet for Grin. It is listed by the Grin project for users who want a graphical wallet and node experience.") : "Community GUI node and wallet for Grin. It is listed by the Grin project for users who want a graphical wallet and node experience.",
                            platforms: i18n ? i18n.tf("wallets_grim_platforms", "Windows, Linux, macOS, Android, and iOS.") : "Windows, Linux, macOS, Android, and iOS."
                        },
                        {
                            title: "Grin++",
                            url: "https://grinplusplus.github.io",
                            body: i18n ? i18n.tf("wallets_grinpp_body", "Community Grin GUI node and wallet written in C++. It is listed as a community project on the official Grin download page.") : "Community Grin GUI node and wallet written in C++. It is listed as a community project on the official Grin download page.",
                            platforms: i18n ? i18n.tf("wallets_grinpp_platforms", "Windows, Linux, macOS, and Android.") : "Windows, Linux, macOS, and Android."
                        }
                    ]

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: walletCardColumn.implicitHeight + 36
                        radius: 24
                        color: "#111318b0"
                        border.color: "#6b738066"
                        border.width: 1

                        Column {
                            id: walletCardColumn
                            anchors.fill: parent
                            anchors.margins: 18
                            spacing: 10

                            Label {
                                width: parent.width
                                text: modelData.title
                                color: "#ffffff"
                                font.pixelSize: 21
                                font.bold: true
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                width: parent.width
                                text: "<a href=\"" + modelData.url + "\">" + modelData.url + "</a>"
                                color: "#dfe5ff"
                                font.pixelSize: 16
                                linkColor: "#dfe5ff"
                                wrapMode: Text.WrapAnywhere
                                onLinkActivated: function(link) { Qt.openUrlExternally(link) }
                            }

                            Label {
                                width: parent.width
                                text: modelData.body
                                color: "#d8ddff"
                                font.pixelSize: 15
                                wrapMode: Text.WordWrap
                            }

                            Label {
                                width: parent.width
                                text: modelData.platforms
                                color: "#bfc8dd"
                                font.pixelSize: 14
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                radius: 26
                color: "#101014b0"
                border.color: "#6b738066"
                border.width: 1
                implicitHeight: noteColumn.implicitHeight + 34

                Column {
                    id: noteColumn
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 12

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("wallets_note_title", "Use Carefully") : "Use Carefully"
                        color: "#ffffff"
                        font.pixelSize: 28
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("wallets_note_body", "The Grin website lists community projects with a use-at-your-own-risk note. Always verify downloads, keep backups of your recovery phrase, and prefer official project links.") : "The Grin website lists community projects with a use-at-your-own-risk note. Always verify downloads, keep backups of your recovery phrase, and prefer official project links."
                        color: "#e8ebff"
                        font.pixelSize: 16
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
