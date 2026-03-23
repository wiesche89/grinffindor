import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    property var i18n: null
    property var settingsStore: null
    signal backRequested()
    anchors.fill: parent
    readonly property bool compact: width < 760
    readonly property bool singleColumn: width < 980
    readonly property int pageGutter: compact ? 16 : 28
    readonly property color glassPanel: "#d9101722"
    readonly property color glassPanelSoft: "#cc131c28"
    readonly property color glassBorder: "#1fd8e1f0"

    function initLanguageFromStore() {
        if (!languageCombo)
            return

        var code = "en"
        if (settingsStore && settingsStore.languageCode)
            code = settingsStore.languageCode
        else if (i18n && i18n.language)
            code = i18n.language

        for (var i = 0; i < languageCombo.model.length; ++i) {
            if (languageCombo.model[i].code === code) {
                languageCombo.currentIndex = i
                break
            }
        }
    }

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
                        text: i18n ? i18n.tf("settings_eyebrow", "Application Setup") : "Application Setup"
                        color: "#b8becf"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("settings_title", "Settings") : "Settings"
                        font.pixelSize: scrollView.width < 700 ? 30 : 42
                        font.bold: true
                        color: "#ffffff"
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width * 0.9
                        text: i18n ? i18n.tf("settings_intro", "Adjust the app configuration to match your preferred language and usage.") : "Adjust the app configuration to match your preferred language and usage."
                        color: "#e8ebff"
                        font.pixelSize: 18
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                width: parent.width
                radius: 26
                color: "#101014b0"
                border.color: "#6b738066"
                border.width: 1
                implicitHeight: settingsColumn.implicitHeight + 34

                Column {
                    id: settingsColumn
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 16

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("settings_language_title", "Language") : "Language"
                        color: "#ffffff"
                        font.pixelSize: 28
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        width: parent.width
                        text: i18n ? i18n.tf("settings_language_note", "Choose the language used across the interface.") : "Choose the language used across the interface."
                        color: "#d8ddff"
                        font.pixelSize: 16
                        wrapMode: Text.WordWrap
                    }

                    Rectangle {
                        width: parent.width
                        radius: 22
                        color: "#111318b0"
                        border.color: "#6b738066"
                        border.width: 1
                        implicitHeight: languageRow.implicitHeight + 28

                        GridLayout {
                            id: languageRow
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 14
                            columns: root.compact ? 1 : 2
                            rowSpacing: 12
                            columnSpacing: 14

                            Label {
                                text: i18n ? i18n.tf("settings_language_label", "App language") : "App language"
                                color: "#f3f5ff"
                                font.pixelSize: 16
                                Layout.alignment: Qt.AlignVCenter
                                Layout.fillWidth: root.compact
                            }

                            ComboBox {
                                id: languageCombo
                                Layout.fillWidth: true
                                model: [
                                    { code: "en", label: i18n ? i18n.tf("settings_language_en", "English") : "English" },
                                    { code: "de", label: i18n ? i18n.tf("settings_language_de", "Deutsch") : "Deutsch" }
                                ]
                                textRole: "label"
                                implicitHeight: 44

                                Component.onCompleted: root.initLanguageFromStore()

                                contentItem: Text {
                                    leftPadding: 14
                                    rightPadding: 38
                                    text: languageCombo.displayText
                                    font.pixelSize: 15
                                    color: "#f3f5ff"
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }

                                background: Rectangle {
                                    radius: 14
                                    color: languageCombo.pressed ? "#1b2029" : "#14181f"
                                    border.color: languageCombo.visualFocus ? "#9ba7c0" : "#5d667566"
                                    border.width: 1
                                }

                                indicator: Canvas {
                                    x: languageCombo.width - width - 14
                                    y: (languageCombo.height - height) / 2
                                    width: 12
                                    height: 8
                                    contextType: "2d"

                                    onPaint: {
                                        context.reset()
                                        context.moveTo(0, 0)
                                        context.lineTo(width, 0)
                                        context.lineTo(width / 2, height)
                                        context.closePath()
                                        context.fillStyle = "#dfe5ff"
                                        context.fill()
                                    }
                                }

                                delegate: ItemDelegate {
                                    width: languageCombo.width
                                    contentItem: Text {
                                        text: modelData.label
                                        color: "#f3f5ff"
                                        font.pixelSize: 15
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle {
                                        color: highlighted ? "#232934" : "#14181f"
                                    }
                                }

                                popup: Popup {
                                    y: languageCombo.height + 6
                                    width: languageCombo.width
                                    implicitHeight: contentItem.implicitHeight
                                    padding: 1

                                    contentItem: ListView {
                                        clip: true
                                        implicitHeight: contentHeight
                                        model: languageCombo.popup.visible ? languageCombo.delegateModel : null
                                        currentIndex: languageCombo.highlightedIndex
                                    }

                                    background: Rectangle {
                                        radius: 14
                                        color: "#14181f"
                                        border.color: "#5d667566"
                                        border.width: 1
                                    }
                                }

                                onCurrentIndexChanged: {
                                    if (currentIndex < 0 || currentIndex >= model.length)
                                        return

                                    var code = model[currentIndex].code
                                    if (i18n)
                                        i18n.language = code
                                    if (settingsStore)
                                        settingsStore.languageCode = code
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
