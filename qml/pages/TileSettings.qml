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

    Rectangle {
        width: Math.min(parent.width * 0.82, 720)
        implicitHeight: contentColumn.implicitHeight + 32
        anchors.centerIn: parent
        radius: 24
        color: "#050505aa"
        border.color: "#ffffff22"
        border.width: 1
        z: 2

        ColumnLayout {
            id: contentColumn
            anchors.fill: parent
            anchors.margins: 16
            spacing: 14

            Label {
                text: i18n ? i18n.tf("settings_title", "Settings") : "Settings"
                font.pixelSize: 28
                font.bold: true
                color: "#ffffff"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                text: i18n ? i18n.tf("settings_language_title", "Language") : "Language"
                font.pixelSize: 18
                font.bold: true
                color: "#f0f0f0"
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Label {
                    text: i18n ? i18n.tf("settings_language_label", "App language") : "App language"
                    color: "#dddddd"
                    font.pixelSize: 14
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

                    Component.onCompleted: root.initLanguageFromStore()

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
