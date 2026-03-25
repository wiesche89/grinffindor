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
  - This is a guided exchange/session scaffold, not yet a fully interoperable Grin transaction engine.

## Mapping to `grin-wallet`

- `libwallet/src/slate.rs`
  - Source of truth for slate state machine, participant data, fee fields, and final kernel signing flow.
  - Slate structure/state is mirrored in the native C++ `SlateV4` model and backed by local secp256k1-zkp signing/finalize flow, but still not at full `grin-wallet` interoperability parity.
- `libwallet/src/slatepack/`
  - Source of truth for final interoperable binary slatepack payload and recipient encryption.
  - The current implementation covers binary SlateV4 payload encode/decode and browser-local framing/checksum flow, but recipient-encrypted Slatepacks are still pending.
- `libwallet/src/internal/scan.rs`
  - Future source for owned-output detection and rewind-based scanning.
- `libwallet/src/internal/selection.rs`
  - Future source for coin selection and lock semantics.
- `libwallet/src/internal/tx.rs`
  - Future source for transaction construction, round handling, finalize, and broadcast.
- `libwallet/src/api_impl/owner.rs`
  - Source of truth for owner-only flows that must remain local inside the browser wallet.

## Immediate next steps

1. Add a dedicated wallet node client adapter on top of `NodeForeignApi` for outputs, kernels, and tx broadcast.
2. Replace the current local workflow scaffold with real slate v4 transaction objects and participant data from `grin-wallet`.
3. Add recipient-encrypted Slatepack decrypt/encrypt support.
4. Tighten external-wallet interoperability against `grin-wallet` edge cases and binary feature variants.
5. Port output scanning and coin-selection logic to full `grin-wallet` parity.
6. Continue hardening the local seed cipher toward stronger authenticated encryption primitives.
7. Extend transaction history, owned-output tracking, and broadcast verification.
