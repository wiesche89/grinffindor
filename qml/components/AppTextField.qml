import QtQuick
import QtQuick.Controls

FocusScope {
    id: root

    property alias text: control.text
    property bool readOnly: false
    property alias placeholderText: control.placeholderText
    property alias echoMode: control.echoMode
    property alias validator: control.validator
    property alias inputMethodHints: control.inputMethodHints
    property alias font: control.font
    property alias color: control.color
    property alias selectionColor: control.selectionColor
    property alias selectedTextColor: control.selectedTextColor
    property alias selectByMouse: control.selectByMouse
    property alias persistentSelection: control.persistentSelection
    property alias activeFocusOnPress: control.activeFocusOnPress
    property alias leftPadding: control.leftPadding
    property alias rightPadding: control.rightPadding
    property alias topPadding: control.topPadding
    property alias bottomPadding: control.bottomPadding
    property alias background: control.background
    property alias cursorPosition: control.cursorPosition
    readonly property string selectedText: control.selectedText
    readonly property int selectionStart: control.selectionStart
    readonly property int selectionEnd: control.selectionEnd
    readonly property bool usingPlatformEditor: enabled && PlatformBridge.supportsNativeTouchSelection()
    property string bridgeId: objectName.length > 0 ? objectName : ("app_text_field_" + Math.random().toString(36).slice(2))
    property string editorTitle: placeholderText
    property string inputMode: ""
    property string inputType: ""
    property string autoCapitalize: "sentences"
    property bool autoCorrect: true
    property bool spellcheck: true
    property bool selectAllOnOpen: true

    signal accepted()
    signal editingFinished()

    implicitWidth: control.implicitWidth
    implicitHeight: control.implicitHeight

    function syncShortcutContext() {
        if (typeof grinWalletController === "undefined")
            return
        grinWalletController.updateBrowserShortcutContext(
            control.text !== undefined && control.text !== null ? control.text : "",
            control.selectedText !== undefined && control.selectedText !== null ? control.selectedText : "",
            control.activeFocus && !usingPlatformEditor)
    }

    function openEditor(selectAllRequested) {
        if (!usingPlatformEditor) {
            control.forceActiveFocus()
            return false
        }

        return PlatformBridge.openTextEditor(editorTitle && editorTitle.length > 0 ? editorTitle : placeholderText,
                                             text,
                                             {
                                                 id: bridgeId,
                                                 multiline: false,
                                                 readOnly: readOnly,
                                                 password: echoMode === TextInput.Password || echoMode === TextInput.PasswordEchoOnEdit,
                                                 placeholder: placeholderText,
                                                 inputMode: inputMode,
                                                 inputType: inputType,
                                                 autoCapitalize: autoCapitalize,
                                                 autoCorrect: autoCorrect,
                                                 spellcheck: spellcheck,
                                                 selectAll: selectAllRequested === undefined ? selectAllOnOpen : !!selectAllRequested
                                             })
    }

    function selectAll() {
        if (usingPlatformEditor) {
            openEditor(true)
            return
        }
        control.selectAll()
        syncShortcutContext()
    }

    function copy() {
        var value = selectedText.length > 0 ? selectedText : text
        return value.length > 0 ? grinWalletController.copyTextToClipboard(value) : false
    }

    function remove(start, end) {
        var currentText = text || ""
        var safeStart = Math.max(0, Math.min(start, currentText.length))
        var safeEnd = Math.max(safeStart, Math.min(end, currentText.length))
        text = currentText.slice(0, safeStart) + currentText.slice(safeEnd)
        control.cursorPosition = safeStart
        syncShortcutContext()
    }

    function insert(index, value) {
        var currentText = text || ""
        var insertion = value || ""
        var safeIndex = Math.max(0, Math.min(index, currentText.length))
        text = currentText.slice(0, safeIndex) + insertion + currentText.slice(safeIndex)
        control.cursorPosition = safeIndex + insertion.length
        syncShortcutContext()
    }

    function cut() {
        if (readOnly)
            return false
        var value = selectedText.length > 0 ? selectedText : text
        if (value.length === 0 || !grinWalletController.copyTextToClipboard(value))
            return false
        if (selectionEnd > selectionStart)
            remove(selectionStart, selectionEnd)
        else
            text = ""
        return true
    }

    function paste() {
        if (readOnly)
            return false
        var pastedText = PlatformBridge.readClipboard()
        if (!pastedText || pastedText.length === 0)
            return false
        if (selectionEnd > selectionStart)
            remove(selectionStart, selectionEnd)
        insert(control.cursorPosition, pastedText)
        return true
    }

    TextField {
        id: control

        anchors.fill: parent
        readOnly: root.readOnly || root.usingPlatformEditor

        leftPadding: 12
        rightPadding: 12
        topPadding: 9
        bottomPadding: 9

        color: root.color && root.color.toString() !== "#000000" ? root.color : "#c8e4f8"
        placeholderTextColor: "#2a5068"
        selectionColor: root.selectionColor && root.selectionColor.toString() !== "#000000"
                        ? root.selectionColor : "#1e5080"
        selectedTextColor: root.selectedTextColor && root.selectedTextColor.toString() !== "#ffffff"
                           ? root.selectedTextColor : "#ddeeff"

        background: Rectangle {
            radius: 10
            color: control.activeFocus ? "#06111a" : "#05101a"
            border.color: control.activeFocus ? "#2a6a88"
                        : (control.hovered ? "#1e4058" : "#0e2a3a")
            border.width: 1
        }

        onActiveFocusChanged: root.syncShortcutContext()
        onSelectedTextChanged: root.syncShortcutContext()
        onTextChanged: root.syncShortcutContext()
        onAccepted: root.accepted()
        onEditingFinished: root.editingFinished()

        Keys.onPressed: function(event) {
            event.accepted = false
        }

        TapHandler {
            enabled: root.usingPlatformEditor
            onTapped: root.openEditor(false)
        }
    }

    Connections {
        target: PlatformBridge

        function onFocusRequested(id) {
            if (id !== root.bridgeId)
                return
            if (root.usingPlatformEditor)
                root.openEditor(false)
            else
                control.forceActiveFocus()
        }

        function onTextEditorTextChanged(id, value) {
            if (id !== root.bridgeId)
                return
            if (root.text !== value)
                root.text = value
        }

        function onTextEditorAccepted(id, value) {
            if (id !== root.bridgeId)
                return
            if (root.text !== value)
                root.text = value
            root.accepted()
            root.editingFinished()
        }

        function onTextEditorClosed(id, value, accepted) {
            if (id !== root.bridgeId)
                return
            if (accepted && root.text !== value)
                root.text = value
        }
    }
}