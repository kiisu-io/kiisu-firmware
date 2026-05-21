# Porting Other Flipper Firmwares to Kiisu

This document is for maintainers of alternative Flipper Zero firmware
distributions (Unleashed, Momentum, RogueMaster, etc.) who want their builds
to run on Kiisu hardware with full functionality.

Kiisu is a Flipper Zero compatible board that ships with a different secure
enclave keyset. Stock firmwares from any vendor — including the official one,
this Kiisu fork, and third-party distributions — assume Flipper Devices'
enclave keys and will degrade or refuse parts of their functionality on Kiisu
unless adapted.

This file documents what needs to change, why, and how. It is intentionally
self-contained so a third-party maintainer can apply the changes without
forking or coordinating with the Kiisu project.

## Scope

The differences between a stock Flipper firmware and one that works on Kiisu
fall into three independent areas:

1. **Secure enclave verification** — sanity-check vectors in
   `targets/f7/furi_hal/furi_hal_crypto.c` are hardcoded to expected
   ciphertexts produced by Flipper Devices' factory slot keys. On Kiisu these
   checks fail and the user sees a "Secure Enclave failure" screen at boot.
2. **U2F attestation certificate** — the stock cert/key pair shipped in
   `/ext/u2f/assets/` is encrypted under slot 2 of Flipper's factory keyset.
   On Kiisu the decryption fails and the U2F app cannot start.
3. **SubGHz manufacturer keystore** — files in
   `applications/main/subghz/resources/subghz/assets/` (`keeloq_mfcodes`,
   `nice_flor_s`, `alutech_at_4n`, `came_atomo`) are encrypted under slot 1
   of Flipper's factory keyset. On Kiisu they fail to decrypt and rolling-code
   protocols stop recognizing manufacturer signatures.

Each area can be ported independently. Pick the ones that matter for your
distribution.

## Hardware background

Kiisu uses the same STM32WB55 MCU as Flipper Zero. Secure Enclave slots 1–10
are programmed during manufacturing by the Kiisu team with their own AES-256
keys; slot 0 (master) is unused; slot 11 (unique) is generated per-device on
first use; slots 12–100 are free for user keys. The provisioning flow itself
is identical to Flipper's — only the actual key material differs.

There is one STM32WB AES peripheral quirk worth knowing about: the C code
casts `uint8_t*` buffers directly to `uint32_t*` when feeding the AES `IVR`
and `DINR` registers, with `AES_CR.DATATYPE = 0` (no swap). On a little-endian
ARM core this places each 4-byte AES word in MSB-first order inside the
register, effectively word-swapping the IV / plaintext / ciphertext relative
to the natural AES byte order. Keys loaded via FUS (`SHCI_C2_FUS_LoadUsrKey`)
are *not* word-swapped — FUS consumes them as-is. This matters when
reproducing the ciphertexts in tooling outside the device (e.g. Python). The
generator script at `scripts/enclave_verify_gen.py` handles the swap
correctly.

## Crypto Recovery: How

### 1. Enclave verification

**What stock firmware does.** `furi_hal_crypto_enclave_verify()` loads each
factory slot key (1..10) with a hardcoded IV, encrypts a hardcoded 16-byte
plaintext, and compares the result against a hardcoded expected ciphertext.
The arrays `enclave_signature_iv`, `enclave_signature_input`, and
`enclave_signature_expected` in `targets/f7/furi_hal/furi_hal_crypto.c` are
populated for Flipper Devices' factory keys.

**What you need to do.** Regenerate the three arrays for the Kiisu keyset.

Procedure:

1. Obtain the 10 AES-256 slot keys for the Kiisu provisioning batch. These
   are not in the firmware repository — you need to get them from the Kiisu
   project directly. (If you do not have them, you cannot complete this step
   honestly; either leave enclave verification disabled or contact the Kiisu
   team.)
2. Place the keys in `scripts/enclave_keys.txt`, one 64-char hex line per
   slot in slot order. The file is gitignored.
3. Run `python scripts/enclave_verify_gen.py`. It picks random IVs and
   plaintexts, encrypts under each slot key (with the correct STM32 byte
   ordering), and prints three C arrays.
4. Paste the output over the existing `enclave_signature_iv[]`,
   `enclave_signature_input[]`, and `enclave_signature_expected[]`
   definitions in `targets/f7/furi_hal/furi_hal_crypto.c`.
5. Make sure the verification block in
   `applications/services/desktop/desktop.c` is *enabled* (a common pattern
   in third-party forks is to comment it out as a quick workaround for the
   warning — undo that).
6. Rebuild and reflash. On a Kiisu the verification now passes; on any
   device with a different keyset (including a stock Flipper Zero) it
   correctly fails and shows the warning, which is the intended security
   property.

The verify-vectors are **not** secret. They are public values that prove
slot-key integrity; they leak nothing about the slot keys themselves.

### 2. U2F self-signed attestation

**What stock firmware does.** On first launch the U2F app reads
`/ext/u2f/assets/cert.der` (X.509 cert) and `/ext/u2f/assets/cert_key.u2f`
(matching ECDSA P-256 private key, encrypted under slot 2). The cert and
key are shipped pre-installed during the official Flipper firmware update
process, and the cert is signed by Flipper Devices' attestation CA.

**Why it breaks on Kiisu.** Slot 2 on Kiisu holds a different key. The
encrypted private key cannot be decrypted. The U2F app errors out at
startup.

**What you need to do.** On first launch, generate a fresh self-signed
attestation certificate on the device itself and store it as
`U2F_CERT_USER_UNENCRYPTED`. The existing Flipper U2F code path then encrypts
the private key with the device-unique slot 11 key on the first successful
read. After this one-time bootstrap the U2F app behaves identically to stock.

The implementation in this fork lives in:

- `applications/main/u2f/u2f_cert_gen.{c,h}` — a minimal DER assembler
  written on top of the already-included mbedtls ECDSA primitives. It
  generates a P-256 keypair via `furi_hal_random`, builds a TBSCertificate
  (CN="Kiisu U2F Token", validity 2020-01-01 to 2049-12-31, random 8-byte
  serial), hashes it with SHA-256, and signs with `mbedtls_ecdsa_sign`.
  Total flash overhead: ~5 KB in the FAP. No SDK API changes required —
  the DER assembler does not depend on the `mbedtls_asn1_write_*` and
  `mbedtls_x509write_*` modules (which are not compiled into Flipper's
  mbedtls subset).
- `applications/main/u2f/u2f_data.c` — adds
  `u2f_data_cert_generate_if_missing()`. Creates `/ext/u2f/assets/` if
  needed, writes `cert.der` (binary DER) and `cert_key.u2f` (Flipper Format,
  Type=2 USER_UNENCRYPTED, Data=raw 32-byte private scalar).
- `applications/main/u2f/u2f_app.c` and `applications/main/u2f/u2f.c` — call
  `u2f_data_cert_generate_if_missing()` early in app initialization, before
  the existing `u2f_data_check()` that triggers the "No SD card or app data
  found" error scene.

Note: the per-device cert is self-attestation in the practical sense (one
device, one certificate, one CA — itself). Relying parties that accept the
WebAuthn `none` attestation conveyance accept it out of the box; relying
parties that perform strict attestation chain validation (rare for consumer
sites, common for some enterprise SSO setups) will reject it because the
root is not in the FIDO Metadata Service. The same trade-off applies to any
non-MDS-listed authenticator.

### 3. SubGHz manufacturer keystore migration

**What stock firmware does.** The four files in
`applications/main/subghz/resources/subghz/assets/` are AES-256-CBC encrypted
under slot 1 of Flipper Devices' factory keyset. They contain manufacturer
codes for KeeLoq, Princeton, Faac, Somfy, Nice Flor-S, CAME Atomo, and
others. Their `Encryption: 1` header tells the SubGHz keystore loader to
decrypt them at runtime.

**Why it breaks on Kiisu.** Slot 1 on Kiisu is a different key. Decryption
yields garbage. Manufacturer protocols fail to recognize rolling codes from
new remotes.

**What you need to do.** Re-encrypt the same plaintext keystore content
under Kiisu's slot 1.

The plaintext content itself is not in any firmware repository. You need to
extract it once from a device whose slot 1 can decrypt the original files —
i.e., a Flipper Zero. The Kiisu fork ships a debug-mode CLI command
`subghz decrypt_keystore <src_encrypted> <dst_plain>` (in
`applications/main/subghz/subghz_cli.c`) that handles both the regular
keystore format (`Flipper SubGhz Keystore File`) and the RAW variant
(`Flipper SubGhz Keystore RAW File`). It is the inverse of the existing
`subghz encrypt_keeloq` / `encrypt_raw` commands.

End-to-end migration procedure:

1. Flash this Kiisu firmware onto an original Flipper Zero (reversible —
   the factory keys are not affected). Enable Debug RTC flag
   (`Settings → System → Debug`).
2. On that Flipper, run:
   ```
   subghz decrypt_keystore /ext/subghz/assets/keeloq_mfcodes /ext/keeloq_mfcodes_plain
   subghz decrypt_keystore /ext/subghz/assets/nice_flor_s   /ext/nice_flor_s_plain
   subghz decrypt_keystore /ext/subghz/assets/alutech_at_4n /ext/alutech_at_4n_plain
   subghz decrypt_keystore /ext/subghz/assets/came_atomo   /ext/came_atomo_plain
   ```
   Each command prints either `Wrote N keys to ...` or
   `Decrypted N blocks (M bytes) to ...`.
3. Pull the four `_plain` files off the SD card (`scripts/storage.py
   receive`, qFlipper file manager, etc.).
4. Flash the Kiisu firmware onto the actual Kiisu device. Push the four
   `_plain` files to its SD card.
5. On Kiisu, with Debug enabled, generate a random 16-byte IV per file
   (`python -c 'import secrets; print(secrets.token_bytes(16).hex())'`)
   and re-encrypt:
   ```
   subghz encrypt_keeloq /ext/keeloq_mfcodes_plain /ext/keeloq_mfcodes <iv1>
   subghz encrypt_raw    /ext/nice_flor_s_plain   /ext/nice_flor_s    <iv2>
   subghz encrypt_raw    /ext/alutech_at_4n_plain /ext/alutech_at_4n  <iv3>
   subghz encrypt_raw    /ext/came_atomo_plain    /ext/came_atomo     <iv4>
   ```
6. Pull the four newly-encrypted files off the SD card, replace the four
   originals under
   `applications/main/subghz/resources/subghz/assets/` in your firmware
   repository, and commit.
7. Delete every plaintext file from the SD cards of both devices and from
   your workstation. They contain decrypted manufacturer codes; do not
   commit them and do not redistribute them.
8. Reflash the original Flipper Zero with its stock firmware (optional but
   recommended).

Once the four `_user` files in the same directory remain available for
end-user-supplied keys; they are unaffected by this migration.

## Quick reference

| Area | Files to change | Where to get content |
|---|---|---|
| Enclave verify | `targets/f7/furi_hal/furi_hal_crypto.c` lines ~44–81, `applications/services/desktop/desktop.c` around the secure-enclave scene block | Generated locally via `scripts/enclave_verify_gen.py` |
| U2F cert | `applications/main/u2f/u2f_cert_gen.{c,h}` (new), `applications/main/u2f/u2f_data.{c,h}`, `applications/main/u2f/u2f.c`, `applications/main/u2f/u2f_app.c` | Generated on-device at first launch |
| SubGHz keystore | `applications/main/subghz/resources/subghz/assets/{keeloq_mfcodes,nice_flor_s,alutech_at_4n,came_atomo}` | Re-encrypted from extracted plaintext as described above |
| Migration tooling | `applications/main/subghz/subghz_cli.c` (`decrypt_keystore` command) | Already in this fork; copy if you want the same workflow |

## Legal note

Re-encrypting an existing manufacturer keystore under a different
provisioning key is a derivative operation on the same underlying data.
If you have any concerns about distributing those manufacturer codes —
even encrypted — review the terms under which the original files are made
available before redistributing your re-encrypted versions. The Kiisu
project's view is that distributing the data only in encrypted form,
keyed to its own provisioning, follows the same model Flipper Devices uses
and inherits the same legal posture.

Verify-vectors and tooling carry no such concern — they are independently
generated and contain no third-party material.
