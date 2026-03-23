import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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
    readonly property bool handset: width < 520
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
        source: root.assetPath("media/images/image_wallpaper_tile.png")
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

        Rectangle {
            width: parent.width * 0.28
            height: width
            radius: width / 2
            x: parent.width * 0.38
            y: parent.height * 0.62
            color: "#7b88ff"
            opacity: 0.025
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
                id: backButton
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

            Item {
                Layout.fillWidth: true
            }

            Label {
                text: i18n ? i18n.tf("grinffindor_page_eyebrow", "Gateway to Grin") : "Gateway to Grin"
                color: "#d9e1f1"
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }
        }
    }

    Flickable {
        id: flick
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
            width: Math.min(flick.width - (compact ? 20 : 48), 1120)
            x: Math.round((flick.width - width) / 2)
            y: compact ? 16 : 28
            spacing: compact ? 20 : 28

            Rectangle {
                width: parent.width
                radius: 34
                color: glassPanel
                border.color: glassBorder
                implicitHeight: heroLayout.implicitHeight + 56

                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#104f6fff" }
                        GradientStop { position: 0.5; color: "#0c55d6ff" }
                        GradientStop { position: 1.0; color: "#00000000" }
                    }
                }

                GridLayout {
                    id: heroLayout
                    anchors.fill: parent
                    anchors.margins: compact ? 18 : 30
                    columns: root.singleColumn ? 1 : 2
                    columnSpacing: compact ? 18 : 28
                    rowSpacing: compact ? 16 : 22

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        spacing: 18

                        Rectangle {
                            radius: 16
                            color: "#12ffffff"
                            border.color: glassBorder
                            implicitWidth: heroBadge.implicitWidth + 22
                            implicitHeight: 34

                            Label {
                                id: heroBadge
                                anchors.centerIn: parent
                                text: i18n ? i18n.tf("grinffindor_hero_badge", "Privacy • Infrastructure • Community") : "Privacy • Infrastructure • Community"
                                color: "#e4ebfb"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: i18n ? i18n.tf("grinffindor_page_title", "Welcome to Grinffindor") : "Welcome to Grinffindor"
                            color: "#ffffff"
                            font.pixelSize: root.compact ? 34 : 58
                            font.weight: Font.Bold
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            Layout.preferredWidth: Math.min(parent.width, 650)
                            text: i18n ? i18n.tf("grinffindor_page_intro", "Grinffindor is a modern gateway into the world of Grin and Mimblewimble — built to make privacy-focused cryptocurrency more accessible, more practical, and more visual.")
                                       : "Grinffindor is a modern gateway into the world of Grin and Mimblewimble — built to make privacy-focused cryptocurrency more accessible, more practical, and more visual."
                            color: "#d0d9ea"
                            font.pixelSize: root.compact ? 17 : 20
                            wrapMode: Text.WordWrap
                        }

                        Flow {
                            Layout.fillWidth: true
                            width: parent.width
                            spacing: 12

                            Rectangle {
                                id: primaryCta
                                implicitWidth: 176
                                implicitHeight: 46
                                radius: 15
                                color: primaryMouse.containsMouse ? "#18ffffff" : "#10ffffff"
                                border.color: glassBorder

                                Text {
                                    anchors.centerIn: parent
                                    text: i18n ? i18n.tf("grinffindor_cta_primary", "Explore Ecosystem") : "Explore Ecosystem"
                                    color: "#ffffff"
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                }

                                MouseArea {
                                    id: primaryMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                }
                            }

                            Rectangle {
                                id: secondaryCta
                                implicitWidth: 136
                                implicitHeight: 46
                                radius: 15
                                color: "#00000000"
                                border.color: secondaryMouse.containsMouse ? "#34ffffff" : glassBorder

                                Text {
                                    anchors.centerIn: parent
                                    text: i18n ? i18n.tf("grinffindor_cta_secondary", "Run a Node") : "Run a Node"
                                    color: "#d7deee"
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                }

                                MouseArea {
                                    id: secondaryMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                }
                            }
                        }
                    }

                    Rectangle {
                        visible: !root.singleColumn
                        Layout.fillWidth: false
                        Layout.preferredWidth: Math.min(420, heroLayout.width * 0.36)
                        Layout.preferredHeight: 290
                        Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                        radius: 28
                        color: glassPanelSoft
                        border.color: glassBorder

                        Column {
                            anchors.fill: parent
                            anchors.margins: 24
                            spacing: 16

                            Label {
                                text: i18n ? i18n.tf("grinffindor_sidecard_title", "Why Grinffindor?") : "Why Grinffindor?"
                                color: "#ffffff"
                                font.pixelSize: 25
                                font.weight: Font.Bold
                            }

                            Repeater {
                                model: [
                                    i18n ? i18n.tf("grinffindor_side_1", "Seed nodes for Mainnet and Testnet") : "Seed nodes for Mainnet and Testnet",
                                    i18n ? i18n.tf("grinffindor_side_2", "Guides for self-hosting with UmbrelOS") : "Guides for self-hosting with UmbrelOS",
                                    i18n ? i18n.tf("grinffindor_side_3", "Faucet and tipping tools for onboarding") : "Faucet and tipping tools for onboarding",
                                    i18n ? i18n.tf("grinffindor_side_4", "Infrastructure designed for builders and community") : "Infrastructure designed for builders and community"
                                ]

                                Row {
                                    width: parent.width
                                    spacing: 10

                                    Rectangle {
                                        width: 7
                                        height: 7
                                        radius: 3.5
                                        y: 8
                                        color: "#8b5cf6"
                                    }

                                    Label {
                                        width: sideCardTextColumn.width
                                        text: modelData
                                        color: "#d4ddef"
                                        font.pixelSize: 15
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }

                        Item {
                            id: sideCardTextColumn
                            width: parent.width - 58
                            height: 1
                            visible: false
                        }
                    }
                }
            }

            GridLayout {
                width: parent.width
                columns: handset ? 1 : (root.compact ? 2 : 4)
                columnSpacing: 16
                rowSpacing: 16

                Repeater {
                    model: [
                        {
                            value: "24/7",
                            label: i18n ? i18n.tf("grinffindor_stat_1", "Infrastructure") : "Infrastructure"
                        },
                        {
                            value: "Mainnet",
                            label: i18n ? i18n.tf("grinffindor_stat_2", "Live Support") : "Live Support"
                        },
                        {
                            value: "Testnet",
                            label: i18n ? i18n.tf("grinffindor_stat_3", "Experimentation") : "Experimentation"
                        },
                        {
                            value: "Community",
                            label: i18n ? i18n.tf("grinffindor_stat_4", "Driven") : "Driven"
                        }
                    ]

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: statColumn.implicitHeight + 30
                        radius: 24
                        color: glassPanelSoft
                        border.color: glassBorder

                        Column {
                            id: statColumn
                            anchors.centerIn: parent
                            spacing: 7

                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.value
                                color: "#ffffff"
                                font.pixelSize: 29
                                font.weight: Font.Bold
                            }

                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.label
                                color: "#bcc7da"
                                font.pixelSize: 14
                            }
                        }
                    }
                }
            }

            Column {
                width: parent.width
                spacing: 12

                Label {
                    text: i18n ? i18n.tf("grinffindor_features_title", "Core Features") : "Core Features"
                    color: "#f4f7ff"
                    font.pixelSize: 36
                    font.weight: Font.Bold
                }

                Label {
                    width: parent.width
                    text: i18n ? i18n.tf("grinffindor_features_subtitle", "A cleaner, easier and more practical way to discover and use the Grin ecosystem.")
                               : "A cleaner, easier and more practical way to discover and use the Grin ecosystem."
                    color: "#c9d3e4"
                    font.pixelSize: 17
                    wrapMode: Text.WordWrap
                }
            }

            GridLayout {
                width: parent.width
                columns: root.width < 620 ? 1 : (root.singleColumn ? 2 : 3)
                columnSpacing: 18
                rowSpacing: 18

                Repeater {
                    model: [
                        {
                            accent: "#8b5cf6",
                            title: i18n ? i18n.tf("grinffindor_feature_seed_title", "Reliable Seed Nodes") : "Reliable Seed Nodes",
                            body: i18n ? i18n.tf("grinffindor_feature_seed_body", "Stable Mainnet and Testnet infrastructure that helps keep the network connected and reachable.") : "Stable Mainnet and Testnet infrastructure that helps keep the network connected and reachable."
                        },
                        {
                            accent: "#22c55e",
                            title: i18n ? i18n.tf("grinffindor_feature_umbrel_title", "UmbrelOS Guides") : "UmbrelOS Guides",
                            body: i18n ? i18n.tf("grinffindor_feature_umbrel_body", "Learn how to run your own node with a clean, self-hosted setup and practical documentation.") : "Learn how to run your own node with a clean, self-hosted setup and practical documentation."
                        },
                        {
                            accent: "#06b6d4",
                            title: i18n ? i18n.tf("grinffindor_feature_testnet_title", "Testnet Faucet") : "Testnet Faucet",
                            body: i18n ? i18n.tf("grinffindor_feature_testnet_body", "Experiment with Grin safely, test wallet flows, and explore the ecosystem without risk.") : "Experiment with Grin safely, test wallet flows, and explore the ecosystem without risk."
                        },
                        {
                            accent: "#f59e0b",
                            title: i18n ? i18n.tf("grinffindor_feature_mainnet_title", "Mainnet Tools") : "Mainnet Tools",
                            body: i18n ? i18n.tf("grinffindor_feature_mainnet_body", "Bring Grin into real interactions through practical faucet and tipping experiences.") : "Bring Grin into real interactions through practical faucet and tipping experiences."
                        },
                        {
                            accent: "#ef4444",
                            title: i18n ? i18n.tf("grinffindor_feature_shop_title", "Upcoming Shop") : "Upcoming Shop",
                            body: i18n ? i18n.tf("grinffindor_feature_shop_body", "A future place for curated tools, hardware and community items built around Grin.") : "A future place for curated tools, hardware and community items built around Grin."
                        },
                        {
                            accent: "#a855f7",
                            title: i18n ? i18n.tf("grinffindor_feature_ecosystem_title", "Built for Builders") : "Built for Builders",
                            body: i18n ? i18n.tf("grinffindor_feature_ecosystem_body", "Whether you are testing, running infrastructure or building software, Grinffindor supports the whole journey.") : "Whether you are testing, running infrastructure or building software, Grinffindor supports the whole journey."
                        }
                    ]

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: featureColumn.implicitHeight + 44
                        radius: 28
                        color: glassPanelSoft
                        border.color: glassBorder

                        Rectangle {
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.right: parent.right
                            height: 3
                            radius: 2
                            color: modelData.accent
                            opacity: 0.95
                        }

                        Column {
                            id: featureColumn
                            anchors.fill: parent
                            anchors.margins: 22
                            spacing: 14

                            Rectangle {
                                width: 54
                                height: 54
                                radius: 17
                                color: "#10ffffff"
                                border.color: glassBorder

                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 14
                                    height: 14
                                    radius: 7
                                    color: modelData.accent
                                }
                            }

                            Label {
                                width: parent.width
                                text: modelData.title
                                color: "#ffffff"
                                font.pixelSize: 23
                                font.weight: Font.Bold
                                wrapMode: Text.WordWrap
                            }

                            Label {
                                width: parent.width
                                text: modelData.body
                                color: "#d4ddef"
                                font.pixelSize: 16
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                radius: 32
                color: glassPanel
                border.color: glassBorder
                implicitHeight: journeyColumn.implicitHeight + 46

                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#03ffffff" }
                        GradientStop { position: 1.0; color: "#084f6fff" }
                    }
                }

                Column {
                    id: journeyColumn
                    anchors.fill: parent
                    anchors.margins: compact ? 18 : 24
                    spacing: 18

                    Label {
                        text: i18n ? i18n.tf("grinffindor_journey_title", "Start Your Journey") : "Start Your Journey"
                        color: "#ffffff"
                        font.pixelSize: 33
                        font.weight: Font.Bold
                    }

                    GridLayout {
                        width: parent.width
                        columns: root.width < 620 ? 1 : (root.singleColumn ? 2 : 4)
                        columnSpacing: 16
                        rowSpacing: 16

                        Repeater {
                            model: [
                                { step: "01", text: i18n ? i18n.tf("grinffindor_journey_step_1", "Run your node") : "Run your node" },
                                { step: "02", text: i18n ? i18n.tf("grinffindor_journey_step_2", "Test your first transaction") : "Test your first transaction" },
                                { step: "03", text: i18n ? i18n.tf("grinffindor_journey_step_3", "Tip someone") : "Tip someone" },
                                { step: "04", text: i18n ? i18n.tf("grinffindor_journey_step_4", "Become part of the network") : "Become part of the network" }
                            ]

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: journeyStepColumn.implicitHeight + 36
                                radius: 24
                                color: glassPanelSoft
                                border.color: glassBorder

                                Column {
                                    id: journeyStepColumn
                                    anchors.fill: parent
                                    anchors.margins: 18
                                    spacing: 10

                                    Label {
                                        text: modelData.step
                                        color: "#8b5cf6"
                                        font.pixelSize: 26
                                        font.weight: Font.Bold
                                    }

                                    Label {
                                        width: parent.width
                                        text: modelData.text
                                        color: "#ffffff"
                                        font.pixelSize: 20
                                        font.weight: Font.Bold
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                radius: 32
                color: glassPanel
                border.color: glassBorder
                implicitHeight: footerColumn.implicitHeight + 46

                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#0855d6ff" }
                        GradientStop { position: 1.0; color: "#0a4f6fff" }
                    }
                }

                Column {
                    id: footerColumn
                    anchors.fill: parent
                    anchors.margins: compact ? 18 : 24
                    spacing: 14

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("grinffindor_community_title", "Built for the Community") : "Built for the Community"
                        color: "#ffffff"
                        font.pixelSize: 31
                        font.weight: Font.Bold
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("grinffindor_community_body", "Grinffindor connects infrastructure, guides and practical tools in one place — focused on privacy, simplicity and decentralization.")
                                   : "Grinffindor connects infrastructure, guides and practical tools in one place — focused on privacy, simplicity and decentralization."
                        color: "#d2dbec"
                        font.pixelSize: 17
                        wrapMode: Text.WordWrap
                    }

                    Flow {
                        width: parent.width
                        spacing: 12

                        Rectangle {
                            implicitWidth: 176
                            implicitHeight: 46
                            radius: 15
                            color: footerPrimaryMouse.containsMouse ? "#18ffffff" : "#10ffffff"
                            border.color: glassBorder

                            Text {
                                anchors.centerIn: parent
                                text: i18n ? i18n.tf("grinffindor_footer_btn_1", "Join the Ecosystem") : "Join the Ecosystem"
                                color: "#ffffff"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }

                            MouseArea {
                                id: footerPrimaryMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                            }
                        }

                        Rectangle {
                            implicitWidth: 130
                            implicitHeight: 46
                            radius: 15
                            color: "#00000000"
                            border.color: footerSecondaryMouse.containsMouse ? "#34ffffff" : glassBorder

                            Text {
                                anchors.centerIn: parent
                                text: i18n ? i18n.tf("grinffindor_footer_btn_2", "Learn More") : "Learn More"
                                color: "#d7deee"
                                font.pixelSize: 14
                                font.weight: Font.Medium
                            }

                            MouseArea {
                                id: footerSecondaryMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                            }
                        }
                    }
                }
            }

            Item {
                width: 1
                height: 20
            }
        }
    }
}
