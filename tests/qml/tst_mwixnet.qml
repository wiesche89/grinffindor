import QtQuick 2.15
import QtTest 1.2
import "../../qml/pages" as Pages

TestCase {
    id: test
    name: "Mwixnet"
    property string assetBaseUrl: Qt.resolvedUrl("../../").toString()
    visible: true
    width: 960
    height: 850
    when: windowShown

    Pages.TileMwixnet { id: tile }

    function response(network, phase) {
        return [
            {jsonrpc: "2.0", id: 1, result: {network: network, role: "swap", phase: phase, started_at: 1700000000, observed_at: 1700000300}},
            {jsonrpc: "2.0", id: 2, result: {started_at: 1700000000, completed: 3, failed: 1, window_s: 300, published_through_uptime_s: 300}}
        ]
    }

    function cleanup() { tile.cancel(); tile.fail("") }

    function test_activity_and_staleness() {
        tile.applyResponses(response("testnet", "mixing"))
        verify(tile.fresh)
        verify(tile.active)
        compare(tile.stage, 1)
        tile.now = tile.receivedAt + 45001
        verify(!tile.active)
        compare(tile.phase, "unavailable")
    }

    function test_idle_failure_and_wrong_network() {
        tile.applyResponses(response("testnet", "idle"))
        verify(tile.fresh)
        verify(!tile.active)
        tile.applyResponses(response("testnet", "failed"))
        verify(!tile.active)
        compare(tile.phase, "failed")
        tile.applyResponses(response("mainnet", "mixing"))
        verify(!tile.fresh)
        compare(tile.statsData, null)
    }

    function test_batch_validation() {
        var value = response("testnet", "mixing")
        value.reverse()
        tile.applyResponses(value)
        verify(tile.active)
        value[0].result.started_at++
        tile.applyResponses(value)
        verify(!tile.fresh)
        value = response("testnet", "mixing")
        value[1].result.completed = "3"
        tile.applyResponses(value)
        verify(!tile.fresh)
        tile.applyResponses({error: {message: "private error"}})
        verify(!tile.fresh)
        verify(tile.errorText.indexOf("private error") < 0)
    }

    function test_error_stops_animation() {
        tile.applyResponses(response("testnet", "submitting"))
        verify(tile.active)
        tile.fail("unreachable")
        verify(!tile.active)
        compare(tile.statusData, null)
    }

    function test_endpoint_validation() {
        verify(tile.validEndpoint("https://testnet.example/v1"))
        verify(tile.validEndpoint("http://127.0.0.1:3000/v1"))
        verify(!tile.validEndpoint("https://user:password@example.org/v1"))
        verify(!tile.validEndpoint("http://example.org/v1"))
        verify(!tile.validEndpoint("https://example.org/v1?token=secret"))
    }
}
