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

    QtObject {
        id: blocks
        property int calls: 0
        property int serial: 0
        function nextRequestId() { return ++serial }
        signal finished(int id, var block, string error)
        function lookup(kernel, id) { calls++ }
        function cancel(id) {}
    }
    Pages.TileMwixnet { id: tile; visible: false; pollingEnabled: false; blockProvider: blocks }

    function response(network, phase) {
        return [
            {jsonrpc: "2.0", id: 1, result: {network: network, role: "swap", phase: phase, started_at: 1700000000, observed_at: 1700000300}},
            {jsonrpc: "2.0", id: 2, result: {started_at: 1700000000, completed: 3, failed: 1, window_s: 300, published_through_uptime_s: 300}}
        ]
    }

    function init() { tile.visible = true }
    function cleanup() { tile.visible = false; tile.cancel(); tile.fail("") }

    function test_activity_and_staleness() {
        tile.applyResponses(response("testnet", "mixing"))
        verify(tile.fresh)
        verify(tile.active)
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

    function extendedResponse() {
        var data = response("testnet", "mixing")
        data[1].result.last_success_window_end_uptime_s = 300
        data[1].result.last_failure_window_end_uptime_s = null
        data.push({jsonrpc: "2.0", id: 3, result: {network: "testnet", role: "swap", version: "0.1.0", round_interval_s: 60, minimum_fee: 1000000}})
        data.push({jsonrpc: "2.0", id: 4, result: {network: "testnet", server:
            {role: "swap", onion_address: "a".repeat(56) + ".onion", identity_key: "a".repeat(64), onion_key: "b".repeat(64)},
            next: {onion_address: "b".repeat(56) + ".onion", identity_key: "c".repeat(64)}
        }})
        data.push({jsonrpc: "2.0", id: 5, result: {network: "testnet", role: "mixer", phase: "validating", started_at: 1700000001, observed_at: 1700000301}})
        data.push({jsonrpc: "2.0", id: 6, result: {started_at: 1700000001, completed: 5, failed: 0, window_s: 300, published_through_uptime_s: 300, last_success_window_end_uptime_s: 300, last_failure_window_end_uptime_s: null}})
        data.push({jsonrpc: "2.0", id: 7, result: {network: "testnet", role: "mixer", version: "0.1.0", round_interval_s: null, minimum_fee: 2000000}})
        return data
    }

    function test_extended_response() {
        tile.applyResponses(extendedResponse().reverse())
        verify(tile.fresh)
        compare(tile.statusData.phase, "mixing")
        compare(tile.serverInfo.round_interval_s, 60)
        compare(tile.routeData.next.identity_key, "c".repeat(64))
        verify(tile.lastSuccess(tile.statsData) !== "—")
        tile.routeExpanded = true
        wait(1)
        tile.now = tile.receivedAt + 45001
        compare(tile.lastSuccess(tile.statsData), "—")
    }

    function test_partial_response_clears_old_details() {
        tile.applyResponses(extendedResponse())
        var data = extendedResponse()
        data[3] = {jsonrpc: "2.0", id: 4, error: {message: "private error"}}
        data[4].result.network = "mainnet"
        data[6].result.minimum_fee = -1
        tile.applyResponses(data)
        verify(tile.fresh)
        compare(tile.routeData, null)
        tile.applyResponses(response("testnet", "idle"))
        compare(tile.serverInfo, null)
    }

    function test_mixer_cannot_mask_swap_failure() {
        var data = extendedResponse()
        data[0] = {jsonrpc: "2.0", id: 1, error: {code: -32603}}
        data[1] = {jsonrpc: "2.0", id: 2, error: {code: -32603}}
        data[3] = {jsonrpc: "2.0", id: 4, error: {code: -32603}}
        tile.applyResponses(data)
        verify(!tile.fresh)
        compare(tile.statusData, null)
        compare(tile.statsData, null)
        verify(!tile.routeAvailable)
    }

    function test_route_and_bucket_validation() {
        var data = extendedResponse()
        data[3].result.next.onion_address = "192.168.1.1"
        tile.applyResponses(data)
        verify(tile.fresh)
        compare(tile.routeData, null)
        data = extendedResponse()
        data[3].result.server.role = "mixer"
        tile.applyResponses(data)
        compare(tile.routeData, null)
        data = extendedResponse()
        data[1].result.last_success_window_end_uptime_s = 301
        data[5].result = null
        tile.applyResponses(data)
        verify(!tile.fresh)
        data = extendedResponse()
        data[1].result.last_success_window_end_uptime_s = 600
        data[5].result = null
        tile.applyResponses(data)
        verify(!tile.fresh)
        data = extendedResponse()
        data[6].id = 5
        tile.applyResponses(data)
        verify(!tile.fresh)
    }

    function test_local_connections() {
        var data = extendedResponse()
        data[3].result.next = null
        tile.applyResponses(data)
        verify(tile.routeAvailable)
        compare(tile.routeData.next, null)
        data[3].result = {network: "testnet", servers: []}
        tile.applyResponses(data)
        verify(!tile.routeAvailable)
    }

    function test_proxy_endpoint() {
        compare(tile.endpoint, "https://mwixnet.grinffindor.org/v1")
    }
    function activityResponse() {
        var data = extendedResponse()
        data.push({jsonrpc: "2.0", id: 8, result: {status: data[0].result, unprocessed: 4, round_inputs: 2}})
        data.push({jsonrpc: "2.0", id: 9, result: {network: "testnet", started_at: 1700000000,
            transactions: [{kernel: "a".repeat(66), block_height: 100, checked_at_height: 103, confirmations: 4},
                           {kernel: "b".repeat(66), block_height: null, checked_at_height: null, confirmations: null}]}})
        return data
    }

    function test_activity_kernels_selection_and_filter() {
        tile.applyResponses(activityResponse())
        compare(tile.activityData.unprocessed, 4)
        compare(tile.activityData.round_inputs, 2)
        compare(tile.kernelModel.count, 2)
        tile.selectedKernel = "a".repeat(66)
        compare(tile.kernelModel.get(0).confirmations, 4)
        tile.kernelFilter = "BBB"
        compare(tile.kernelModel.count, 2)
        wait(1)
        verify(!findChild(tile, "kernel_" + "a".repeat(66)).visible)
        verify(findChild(tile, "kernel_" + "b".repeat(66)).visible)
        tile.kernelFilter = ""
        tile.now = tile.receivedAt + 45001
        verify(!findChild(tile, "kernel_" + "a".repeat(66)).visible)
    }

    function test_invalid_activity_and_kernels_do_not_hide_status() {
        var data = activityResponse()
        data[7].result.unprocessed = -1
        data[8].result.transactions[0].confirmations = 5
        tile.applyResponses(data)
        verify(tile.fresh)
        compare(tile.activityData, null)
        compare(tile.transactionData, null)
        data = activityResponse()
        data[8].result.transactions.push(data[8].result.transactions[0])
        tile.applyResponses(data)
        compare(tile.transactionData, null)
    }

    function test_restart_clears_selection() {
        tile.applyResponses(activityResponse())
        tile.selectedKernel = "a".repeat(66)
        var data = response("testnet", "idle")
        data[0].result.started_at++
        data[1].result.started_at++
        tile.applyResponses(data)
        compare(tile.selectedKernel, "")
        compare(tile.transactionData, null)
    }

    function test_confirmation_wait_and_completed_phases() {
        var data = activityResponse()
        data[7].result.awaiting_confirmation = 2
        data[7].result.completed_phases = ["validating", "mixing", "submitting"]
        tile.applyResponses(data)
        compare(tile.activityData.awaiting_confirmation, 2)
        compare(tile.activityData.completed_phases.length, 3)
        data[7].result.awaiting_confirmation = 0
        data[7].result.completed_phases = []
        tile.applyResponses(data)
        compare(tile.activityData.awaiting_confirmation, 0)
        compare(tile.activityData.completed_phases.length, 0)
        data[7].result.completed_phases = ["failed"]
        tile.applyResponses(data)
        compare(tile.activityData, null)
    }

    function test_inactive_phase_restores_full_opacity() {
        tile.applyResponses(response("testnet", "checking_confirmations"))
        var bar = findChild(tile, "phaseBar_checking_confirmations")
        verify(bar !== null)
        wait(100)
        tile.applyResponses(response("testnet", "idle"))
        compare(bar.opacity, 1)
        compare(bar.color, "#344356")
    }

    function test_poll_keeps_delegate_and_expansion() {
        var data = activityResponse()
        tile.applyResponses(data)
        tile.selectedKernel = "a".repeat(66)
        wait(1)
        var row = findChild(tile, "kernel_" + tile.selectedKernel)
        var details = findChild(row, "blockDetails")
        verify(details !== null)
        details.expanded = true
        data[8].result.transactions[0].checked_at_height++
        data[8].result.transactions[0].confirmations++
        tile.applyResponses(data)
        wait(1)
        compare(findChild(tile, "kernel_" + tile.selectedKernel), row)
        compare(findChild(row, "blockDetails"), details)
        verify(details.expanded)
    }

}
