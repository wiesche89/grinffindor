import QtQuick 2.15
import QtTest 1.2
import "../../qml/pages" as Pages

TestCase {
    name: "Mwixnet"
    property string assetBaseUrl: Qt.resolvedUrl("../../").toString()
    visible: true
    width: 960
    height: 850
    when: windowShown
    Pages.TileMwixnet { id: tile; visible: false; pollingEnabled: false }

    function response() {
        return [
            {jsonrpc: "2.0", id: 3, result: {network: "testnet", role: "swap", version: "0.1.0", round_interval_s: 60, minimum_fee: 12500000}},
            {jsonrpc: "2.0", id: 4, result: {network: "testnet", server: {role: "swap", onion_address: "a".repeat(56) + ".onion", identity_key: "a".repeat(64), onion_key: "b".repeat(64)}, next: null}}
        ]
    }
    function init() { tile.visible = true }
    function cleanup() { tile.visible = false; tile.cancel(); tile.fail("") }

    function test_settings_and_staleness() {
        tile.applyResponses(response().reverse())
        verify(tile.fresh)
        compare(tile.serverInfo.minimum_fee, 12500000)
        verify(tile.routeAvailable)
        tile.now = tile.receivedAt + 45001
        verify(!tile.fresh)
        compare(tile.usageExample, "")
    }
    function test_invalid_server() {
        var data = response()
        data[0].result.network = "mainnet"
        tile.applyResponses(data)
        verify(!tile.fresh)
        data = response()
        data[0].result.role = "mixer"
        tile.applyResponses(data)
        verify(!tile.fresh)
        data = response()
        data[0].result.minimum_fee = -1
        tile.applyResponses(data)
        verify(!tile.fresh)
    }
    function test_route_failure_clears_old_details() {
        tile.applyResponses(response())
        var data = response()
        data[1].result.server.onion_address = "127.0.0.1"
        tile.applyResponses(data)
        verify(tile.fresh)
        compare(tile.routeData, null)
        tile.applyResponses([response()[0]])
        verify(tile.fresh)
    }
    function test_legacy_monitoring_not_required() {
        var data = response()
        data.push({jsonrpc: "2.0", id: 1, error: {code: -32601}})
        data.push({jsonrpc: "2.0", id: 8, result: {unprocessed: 99}})
        tile.applyResponses(data)
        verify(tile.fresh)
        compare(tile.activityData, undefined)
        compare(tile.transactionData, undefined)
    }
    function test_invalid_batch() {
        var data = response()
        data.push(data[0])
        tile.applyResponses(data)
        verify(!tile.fresh)
        tile.applyResponses({error: {message: "private error"}})
        verify(tile.errorText.indexOf("private error") < 0)
        tile.applyResponses([{jsonrpc: "2.0", id: 3, error: {code: -32603}}])
        compare(tile.serverInfo, null)
    }
}
