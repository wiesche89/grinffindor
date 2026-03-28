# Browser Wallet Manual Check

Use this after a fresh WASM build and before a production deployment.

## Setup

1. Serve `build/WebAssembly_Qt_6_10_1_single_threaded-Release` behind the configured `nginx` setup.
2. Open the wallet in a clean browser profile.
3. Verify the browser console has no startup errors.

## Storage

1. Open the wallet and check the storage durability panel in Settings.
2. Click `Request Persistent Storage` and verify the storage state updates when the browser supports it.
3. Reload the page and verify the local wallet still exists.

## Mainnet / Testnet

1. Create or restore a wallet on `Mainnet`.
2. Confirm the selected network is shown correctly after reload.
3. Switch to `Testnet` from the login/setup flow.
4. Confirm balances, outputs, history, and scan height are isolated from Mainnet.
5. Switch back to `Mainnet` and confirm the previous Mainnet state returns.

## Backup / Recovery

1. Generate an encrypted backup JSON.
2. Test `Copy Backup`.
3. Test `Download Backup`.
4. Delete the local wallet.
5. Import the encrypted backup JSON.
6. Unlock it and verify wallet name, history, scan height, and network selection are restored.

## Session Hardening

1. Unlock the wallet.
2. Move the browser tab to the background or switch the app to inactive.
3. Return to the tab and verify the wallet is locked again.

## Slatepack / Network Safety

1. On `Mainnet`, decode and process a valid Mainnet Slatepack.
2. Switch to `Testnet`.
3. Attempt to process a Mainnet Slatepack and verify the wallet blocks it with a network mismatch error.

## Broadcast Recovery

1. Start a send workflow.
2. Broadcast while the node is reachable.
3. Reload the page before confirmation.
4. Verify the transaction stays in recovery-capable state and the UI shows pending broadcast guidance.
5. Disconnect the node or point to an invalid endpoint and verify offline guidance appears.
