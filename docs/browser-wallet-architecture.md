# Grin Browser Wallet Architecture

This repository now contains a first-pass `GrinWalletController` for a browser wallet running on Qt WASM.

## Current module split

- `GrinWalletController`
  - QML-facing orchestration layer.
  - Owns wallet state, local seed vault state, node configuration, and slatepack helpers.
- Browser persistence
  - Uses a JSON wallet document under `/persistent/grin-wallet/browser-wallet.json`.
  - On Qt WASM this path is mounted to `IDBFS`, which is backed by browser IndexedDB.
- Seed vault
  - Generates and validates 24-word BIP39 mnemonics from the browser reference word list.
  - Stores only encrypted mnemonic material and a public fingerprint.
- Crypto backend
  - `WalletCryptoBackend` is now the dedicated integration point for participant keys, offsets, commitments, bulletproofs, and aggsig handling.
  - This keeps the secp256k1-zkp integration localized instead of spreading crypto details across the UI/controller.
- External node integration
  - Uses the existing `NodeForeignApi` C++ wrapper.
  - The node is treated strictly as a remote chain-data service.
- Slatepack workbench
  - Adds local armored encode/decode framing so the wallet already has a browser-side exchange surface.
- Exchange workflow
  - The browser wallet now carries a local `SEND`/`RECEIVE` workflow state machine for `S1-S3` and `I1-I3`.
  - The browser wallet now builds and advances real local slate state, transaction skeletons, payment proofs, and Slatepack envelopes.
  - Remaining work is interoperability hardening, broader fixture coverage, and fuller `grin-wallet` parity around restore/history edge cases.

## Mapping to `grin-wallet`

- `libwallet/src/slate.rs`
  - Source of truth for slate state machine, participant data, fee fields, and final kernel signing flow.
  - Slate structure/state is mirrored in the native C++ `SlateV4` model and backed by local secp256k1-zkp signing/finalize flow, with the remaining gap now mostly in edge-case parity and external-wallet coverage.
- `libwallet/src/slatepack/`
  - Source of truth for final interoperable binary slatepack payload and recipient encryption.
  - The current implementation covers binary SlateV4 payload encode/decode, local armor framing, and recipient-encrypted Slatepacks, with more fixture-based interop validation still pending.
- `libwallet/src/internal/scan.rs`
  - Future source for owned-output detection and rewind-based scanning.
- `libwallet/src/internal/selection.rs`
  - Future source for coin selection and lock semantics.
- `libwallet/src/internal/tx.rs`
  - Future source for transaction construction, round handling, finalize, and broadcast.
- `libwallet/src/api_impl/owner.rs`
  - Source of truth for owner-only flows that must remain local inside the browser wallet.

## Immediate next steps

1. Tighten external-wallet interoperability against `grin-wallet` edge cases and binary feature variants.
2. Expand fixture coverage with additional real `send`, `receive`, `pay`, and `finalize` stage artifacts.
3. Port output scanning and coin-selection logic to fuller `grin-wallet` parity, especially around restore and cancel semantics.
4. Continue hardening transaction history reconstruction after deep rescans and partial local-state loss.
5. Consider a dedicated wallet node client adapter on top of `NodeForeignApi` for outputs, kernels, and tx broadcast.
6. Continue hardening the local seed cipher toward stronger authenticated encryption primitives.
