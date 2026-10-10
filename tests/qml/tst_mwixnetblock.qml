import QtQuick 2.15
import QtTest 1.2
import "../../qml/components" as Components

TestCase {
    name: "MwixnetBlock"
    QtObject {
        id: provider
        property int lastId: -1
        property int lastHeight: -1
        property int calls: 0
        property int serial: 0
        function nextRequestId() { return ++serial }
        signal finished(int id, string blockJson, string error)
        function lookup(kernel, id, height) { lastId = id; lastHeight = height; calls++ }
        function cancel(id) {}
    }
    Components.MwixnetBlockDetails { id: details; kernel: ""; provider: provider }

    function sampleBlock(height, hash) {
        return {header: {height: height, hash: hash || "c".repeat(64), timestamp: "2026-10-10T14:13:13+00:00"},
            kernels: [{excess: "a".repeat(66)}], inputs: [], outputs: []}
    }
    function init() { details.kernel = ""; details.expectedHeight = -1; details.checkedAtHeight = -1; details.cancel() }
    function cleanup() { details.kernel = ""; details.cancel() }

    function test_reorg_and_late_reply() {
        details.kernel = "a".repeat(66)
        details.expectedHeight = 42
        details.checkedAtHeight = 43
        wait(1)
        compare(provider.lastHeight, 42)
        var oldId = provider.lastId
        provider.finished(oldId, JSON.stringify(sampleBlock(42)), "")
        compare(details.block.header.height, 42)
        details.expectedHeight = 43
        wait(1)
        compare(details.block, null)
        provider.finished(oldId, JSON.stringify(sampleBlock(42)), "")
        compare(details.block, null)
        compare(provider.lastHeight, 43)
        provider.finished(provider.lastId, JSON.stringify(sampleBlock(43)), "")
        compare(details.block.header.height, 43)
        details.expectedHeight = -1
        wait(1)
        compare(details.block, null)
    }
    function test_same_height_rechecked_and_timeout() {
        details.kernel = "a".repeat(66)
        details.expectedHeight = 42
        details.checkedAtHeight = 43
        wait(1)
        provider.finished(provider.lastId, JSON.stringify(sampleBlock(42)), "")
        details.checkedAtHeight = 44
        wait(1)
        provider.finished(provider.lastId, JSON.stringify(sampleBlock(42, "d".repeat(64))), "")
        compare(details.block.header.hash, "d".repeat(64))
        details.checkedAtHeight = 45
        wait(1)
        provider.finished(provider.lastId, JSON.stringify({}), "timeout")
        verify(!details.loading)
        verify(details.error !== "")
        compare(details.block, null)
    }

    function test_block_must_contain_selected_kernel() {
        details.kernel = "a".repeat(66)
        var block = {header: {height: 42, hash: "c".repeat(64), timestamp: "2026-10-10T14:13:13+00:00"}, kernels: [{excess: details.kernel}], inputs: [], outputs: []}
        verify(!details.acceptBlock(block, 43))
        verify(details.acceptBlock(block, 42))
        block.kernels = [{excess: "b".repeat(66)}]
        verify(!details.acceptBlock(block, 42))
        verify(!details.acceptBlock({header: {height: 42}}, 42))
    }

    function test_fields_preserve_values() {
        var rows = details.fields({header: {nonce: "8689176513303957280"}, inputs: ["a"], outputs: [{commit: "b", proof: "c".repeat(1400)}]}, "")
        compare(rows[0].label, "header.nonce")
        compare(rows[0].value, "8689176513303957280")
        compare(rows[1].label, "inputs[0]")
        compare(rows[3].value.length, 1400)
    }
}
