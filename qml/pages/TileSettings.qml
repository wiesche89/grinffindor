import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    property var i18n: null
    property var settingsStore: null
    signal backRequested()
    anchors.fill: parent

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
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#140716d9" }
            GradientStop { position: 0.45; color: "#1b0b1fcc" }
            GradientStop { position: 1.0; color: "#09040dcc" }
        }
        z: 1
    }

    Button {
        text: i18n ? i18n.tf("back", "Back") : "Back"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 18
        anchors.topMargin: 18
        onClicked: root.backRequested()
        z: 4
    }

    Flickable {
        id: scrollView
        anchors.fill: parent
        anchors.topMargin: 72
        clip: true
        contentWidth: width
        contentHeight: contentColumn.implicitHeight + 48
        z: 3

        Column {
            id: contentColumn
            width: Math.min(scrollView.width * 0.86, 980)
            x: Math.round((scrollView.width - width) / 2)
            y: 22
            spacing: 18

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

                        RowLayout {
                            id: languageRow
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 14

                            Label {
                                text: i18n ? i18n.tf("settings_language_label", "App language") : "App language"
                                color: "#f3f5ff"
                                font.pixelSize: 16
                                Layout.alignment: Qt.AlignVCenter
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
