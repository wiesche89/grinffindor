import QtQuick 2.15
import QtQml 2.15

QtObject {
    id: i18n

    property var settingsStore: null
    property string language: "en"
    property var dict: ({})
    property bool loaded: false

    function loadLanguage(lang) {
        if (!lang || typeof lang !== "string")
            lang = "en"

        loaded = false
        dict = ({})

        var url = Qt.resolvedUrl("translation/" + lang + ".json")
        var xhr = new XMLHttpRequest()
        xhr.open("GET", url)

        xhr.onreadystatechange = function() {
            if (xhr.readyState !== XMLHttpRequest.DONE)
                return

            if (xhr.status === 200 || xhr.status === 0) {
                try {
                    dict = JSON.parse(xhr.responseText)
                    loaded = true
                } catch (e) {
                    console.error("I18n parse error for", lang, "from", url, e)
                    dict = ({})
                    loaded = false
                }
            } else {
                console.warn("I18n load failed:", url, "status:", xhr.status)
                dict = ({})
                loaded = false
                if (lang !== "en")
                    loadLanguage("en")
            }
        }

        xhr.send()
    }

    Component.onCompleted: {
        if (settingsStore && settingsStore.languageCode)
            language = settingsStore.languageCode
        loadLanguage(language)
    }

    onLanguageChanged: {
        if (settingsStore)
            settingsStore.languageCode = language
        loadLanguage(language)
    }

    function t(key) {
        var _ = loaded

        if (!key || typeof key !== "string")
            return ""

        if (!loaded)
            return key

        if (dict && dict.hasOwnProperty(key) && dict[key] !== undefined)
            return String(dict[key])

        return key
    }

    function tf(key, fallback) {
        var value = t(key)
        if (value === key && fallback !== undefined && fallback !== null)
            return String(fallback)
        return value
    }
}
