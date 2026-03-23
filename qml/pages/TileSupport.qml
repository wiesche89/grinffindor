import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    property var i18n: null
    signal backRequested()
    anchors.fill: parent
    readonly property bool compact: width < 760
    readonly property bool singleColumn: width < 980
    readonly property int pageGutter: compact ? 16 : 28
    readonly property color glassPanel: "#d9101722"
    readonly property color glassPanelSoft: "#cc131c28"
    readonly property color glassBorder: "#1fd8e1f0"

    Rectangle {
        anchors.fill: parent
        color: "#05070b"
    }

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
        color: "#660a0e14"
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#a00b1017" }
            GradientStop { position: 0.32; color: "#780e141d" }
            GradientStop { position: 0.68; color: "#880b1017" }
            GradientStop { position: 1.0; color: "#b0090d14" }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"

        Rectangle {
            width: parent.width * 0.56
            height: width
            radius: width / 2
            x: parent.width - width * 0.72
            y: -height * 0.24
            color: "#5b7cff"
            opacity: 0.035
        }

        Rectangle {
            width: parent.width * 0.42
            height: width
            radius: width / 2
            x: -width * 0.18
            y: parent.height * 0.30
            color: "#55d6ff"
            opacity: 0.03
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

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

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
                        text: i18n ? i18n.tf("support_page_eyebrow", "Direct Help") : "Direct Help"
                        color: "#b8becf"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("support_page_title", "Support") : "Support"
                        font.pixelSize: scrollView.width < 700 ? 30 : 42
                        font.bold: true
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width * 0.9
                        text: i18n ? i18n.tf("support_page_intro", "Do you have questions, need help, or ran into a problem? No problem - we are here for you.") : "Do you have questions, need help, or ran into a problem? No problem - we are here for you."
                        color: "#e8ebff"
                        font.pixelSize: 18
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width * 0.9
                        text: i18n ? i18n.tf("support_page_telegram_intro", "For fast and direct support and for reporting bugs, please use our Telegram community:") : "For fast and direct support and for reporting bugs, please use our Telegram community:"
                        color: "#d8ddff"
                        font.pixelSize: 16
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                width: parent.width
                radius: 24
                color: "#101014b0"
                border.color: "#6b738066"
                border.width: 1
                implicitHeight: telegramColumn.implicitHeight + 30

                Column {
                    id: telegramColumn
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 10

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("support_telegram_title", "Telegram Community") : "Telegram Community"
                        color: "#ffffff"
                        font.pixelSize: 22
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        width: parent.width
                        text: "<a href=\"" + (i18n ? i18n.tf("support_telegram_url", "https://t.me/Grinffindor") : "https://t.me/Grinffindor") + "\">"
                              + (i18n ? i18n.tf("support_telegram_label", "https://t.me/Grinffindor") : "https://t.me/Grinffindor")
                              + "</a>"
                        color: "#dfe5ff"
                        font.pixelSize: 18
                        font.bold: true
                        linkColor: "#dfe5ff"
                        wrapMode: Text.WordWrap
                        onLinkActivated: function(link) { Qt.openUrlExternally(link) }
                    }
                }
            }

            GridLayout {
                width: parent.width
                columns: root.singleColumn ? 1 : 2
                columnSpacing: 16
                rowSpacing: 16

                Repeater {
                    model: [
                        {
                            title: i18n ? i18n.tf("support_topic_bug_title", "Report Errors and Bugs") : "Report Errors and Bugs",
                            body: i18n ? i18n.tf("support_topic_bug_body", "Let us know when something is broken, behaves unexpectedly, or produces an error so problems can be tracked and fixed quickly.") : "Let us know when something is broken, behaves unexpectedly, or produces an error so problems can be tracked and fixed quickly."
                        },
                        {
                            title: i18n ? i18n.tf("support_topic_help_title", "Get Help with Wallet, Node, or Bot") : "Get Help with Wallet, Node, or Bot",
                            body: i18n ? i18n.tf("support_topic_help_body", "Ask for direct support if you need help using the wallet, setting up a node, or working with the bots and related tools.") : "Ask for direct support if you need help using the wallet, setting up a node, or working with the bots and related tools."
                        },
                        {
                            title: i18n ? i18n.tf("support_topic_feedback_title", "Share Feedback and Suggestions") : "Share Feedback and Suggestions",
                            body: i18n ? i18n.tf("support_topic_feedback_body", "Bring in feedback, ideas for improvements, and practical suggestions that can make the platform better for everyone.") : "Bring in feedback, ideas for improvements, and practical suggestions that can make the platform better for everyone."
                        },
                        {
                            title: i18n ? i18n.tf("support_topic_exchange_title", "Talk with Users and Developers") : "Talk with Users and Developers",
                            body: i18n ? i18n.tf("support_topic_exchange_body", "Use the group to exchange with other users and developers, compare setups, and resolve issues together.") : "Use the group to exchange with other users and developers, compare setups, and resolve issues together."
                        }
                    ]

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: topicColumn.implicitHeight + 36
                        radius: 24
                        color: "#111318b0"
                        border.color: "#6b738066"
                        border.width: 1

                        Column {
                            id: topicColumn
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

                            Label {
                                width: parent.width
                                text: modelData.body
                                color: "#d8ddff"
                                font.pixelSize: 15
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
                implicitHeight: goalColumn.implicitHeight + 34

                Column {
                    id: goalColumn
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 12

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("support_goal_title", "Our Goal") : "Our Goal"
                        color: "#ffffff"
                        font.pixelSize: 28
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("support_goal_body", "Our goal is to identify problems quickly and find solutions together.") : "Our goal is to identify problems quickly and find solutions together."
                        color: "#e8ebff"
                        font.pixelSize: 16
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                width: parent.width
                radius: 26
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#17191eb8" }
                    GradientStop { position: 1.0; color: "#0e1014b8" }
                }
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
                        text: i18n ? i18n.tf("support_note_title", "Helpful Bug Reports") : "Helpful Bug Reports"
                        color: "#ffffff"
                        font.pixelSize: 28
                        font.bold: true
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("support_note_body", "The more precise your description is, including logs, screenshots, or error messages, the faster we can help.") : "The more precise your description is, including logs, screenshots, or error messages, the faster we can help."
                        color: "#f3f5ff"
                        font.pixelSize: 18
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
