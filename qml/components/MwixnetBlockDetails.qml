import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ColumnLayout {
    id: root
    required property string kernel
    property var provider: null
    property double expectedHeight: -1
    property double checkedAtHeight: -1
    property var block: null
    property var selectedKernel: null
    property string error: ""
    property bool loading: false
    property bool expanded: false
    property int generation: 0
    property bool ready: false
    spacing: 12

    function cancel() {
        if (provider) provider.cancel(generation)
        generation = 0
        loading = false
    }

    function acceptBlock(value, height) {
        if (!value || !value.header || Number(value.header.height) !== height
                || typeof value.header.hash !== "string" || !/^[0-9a-f]{64}$/.test(value.header.hash)
                || typeof value.header.timestamp !== "string"
                || !Array.isArray(value.kernels) || !Array.isArray(value.inputs)
                || !Array.isArray(value.outputs)) return false
        var found = value.kernels.filter(function(item) { return item && item.excess === kernel })
        if (found.length !== 1) return false
        block = value
        selectedKernel = found[0]
        return true
    }

    function load() {
        cancel()
        block = null
        selectedKernel = null
        error = ""
        if (!ready || !provider || expectedHeight < 0 || !/^[0-9a-f]{66}$/.test(kernel)) return
        generation = provider.nextRequestId()
        loading = true
        provider.lookup(kernel, generation)
    }

    Connections {
        target: root.provider
        function onFinished(id, value, message) {
            if (id !== root.generation) return
            root.loading = false
            if (message !== "" || !root.acceptBlock(value, root.expectedHeight))
                root.error = qsTr("Block data unavailable. Reopen to retry.")
        }
    }
    onExpectedHeightChanged: refresh.restart()
    onCheckedAtHeightChanged: refresh.restart()
    onKernelChanged: refresh.restart()
    Timer { id: refresh; interval: 0; onTriggered: root.load() }

    function fields(value, prefix) {
        var rows = []
        Object.keys(value).forEach(function(key) {
            var item = value[key]
            var name = prefix ? prefix + "." + key : key
            if (item !== null && typeof item === "object" && !Array.isArray(item))
                rows = rows.concat(fields(item, name))
            else if (Array.isArray(item) && key !== "cuckoo_solution")
                item.forEach(function(entry, index) {
                    var path = name + "[" + index + "]"
                    rows = rows.concat(typeof entry === "object" && entry !== null
                                       ? fields(entry, path) : [{label: path, value: String(entry)}])
                })
            else rows.push({label: name, value: item === null ? "null" : Array.isArray(item) ? JSON.stringify(item) : String(item)})
        })
        return rows
    }

    Component.onCompleted: { ready = true; refresh.restart() }
    Component.onDestruction: cancel()

    component Field: RowLayout {
        id: field
        required property string label
        required property string value
        Layout.fillWidth: true
        spacing: 12
        Label {
            text: field.label
            textFormat: Text.PlainText
            color: "#94a1b2"
            font.pixelSize: 12
            Layout.preferredWidth: Math.min(180, root.width * 0.3)
            wrapMode: Text.WrapAnywhere
        }
        CopyableLabel {
            text: field.value
            copiedValue: field.value
            textFormat: Text.PlainText
            color: "#e8edf5"
            font.family: "monospace"
            font.pixelSize: 12
            Layout.fillWidth: true
            elide: Text.ElideMiddle
            activeFocusOnTab: true
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Copy ") + field.label
            Accessible.onPressAction: performCopy()
            Keys.onReturnPressed: performCopy()
            Keys.onSpacePressed: performCopy()
        }
    }

    Label { text: qsTr("Block · Testnet node"); color: "#f4f7ff"; font.pixelSize: 16 }
    Label {
        visible: root.loading || root.error !== ""
        text: root.loading ? qsTr("Loading block…") : root.error
        color: "#a9b4c2"; font.pixelSize: 12
        Layout.fillWidth: true; wrapMode: Text.Wrap
    }
    ColumnLayout {
        visible: root.block !== null
        Layout.fillWidth: true
        spacing: 10
        Repeater {
            model: root.block ? [
                {label: qsTr("Height"), value: String(root.block.header.height)},
                {label: qsTr("Hash"), value: root.block.header.hash},
                {label: qsTr("Time (UTC)"), value: root.block.header.timestamp},
                {label: qsTr("Inputs / outputs / kernels"), value: root.block.inputs.length + " / " + root.block.outputs.length + " / " + root.block.kernels.length}
            ] : []
            Field { required property var modelData; label: modelData.label; value: modelData.value }
        }
        Label { text: qsTr("Selected kernel"); color: "#f4f7ff"; font.pixelSize: 14; Layout.topMargin: 8 }
        Repeater {
            model: root.selectedKernel ? root.fields(root.selectedKernel, "") : []
            Field { required property var modelData; label: modelData.label === "fee" ? qsTr("Fee (nanogrin)") : modelData.label.replace(/_/g, " "); value: modelData.value }
        }
        AppButton {
            text: root.expanded ? qsTr("Hide block fields") : qsTr("All block fields")
            onClicked: root.expanded = !root.expanded
        }
        Label {
            visible: root.expanded
            text: qsTr("Whole block. Inputs and outputs are not assigned to individual kernels.")
            color: "#94a1b2"; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.Wrap
        }
        Repeater {
            model: root.expanded && root.block ? root.fields(root.block, "") : []
            Field { required property var modelData; label: modelData.label; value: modelData.value }
        }
    }
}
