#!/usr/bin/env python3
"""Generate enclave verify-vectors for furi_hal_crypto.c.

Stock Flipper firmware ships with `enclave_signature_iv`,
`enclave_signature_input` and `enclave_signature_expected` arrays whose values
correspond to the AES-256 keys Flipper Devices provisions into slots 1..10 of
the STM32WB55 secure enclave. On a Kiisu board provisioned with a different
keyset, `furi_hal_crypto_enclave_verify()` reports
`valid_keys_nb < ENCLAVE_FACTORY_KEY_SLOTS` and the desktop secure-enclave
warning fires.

This script generates fresh test vectors for your own keyset: it picks a
random IV and a random 16-byte plaintext per slot, encrypts the plaintext with
AES-256-CBC under your slot key, and emits the resulting C arrays.

Usage
-----
1. Create `enclave_keys.txt` next to this script (or pass `--keys <path>`),
   with exactly 10 lines, one 64-character hex AES-256 key per line in slot
   order (slot 1 first). Blank lines and lines starting with `#` are ignored.
2. Run: `python scripts/enclave_verify_gen.py`
3. Paste the printed three arrays over the existing
   `enclave_signature_iv`, `enclave_signature_input` and
   `enclave_signature_expected` definitions in
   `targets/f7/furi_hal/furi_hal_crypto.c` (lines 44-81 at the time of
   writing).
4. Re-enable the secure-enclave check in
   `applications/services/desktop/desktop.c` (currently commented out around
   line 542).
5. Rebuild and reflash. `furi_hal_crypto_enclave_verify()` will return true
   on devices with your provisioned keys, and the warning scene will fire on
   devices with a different keyset.

Security note: `enclave_keys.txt` contains your enclave keys in plaintext.
Keep it out of git (the repository's `.gitignore` should cover it) and out
of any shared filesystem.
"""

from __future__ import annotations

import argparse
import secrets
import sys
from pathlib import Path
from typing import List

try:
    from Crypto.Cipher import AES
except ImportError:
    sys.exit(
        "pycryptodome is required. Install with:\n"
        "    pip install pycryptodome"
    )


N_SLOTS = 10
KEY_SIZE = 32  # AES-256
BLOCK_SIZE = 16


def bswap32(data: bytes) -> bytes:
    """Reverse byte order within each 4-byte word.

    The STM32WB AES peripheral effectively word-swaps IV/data when the C code
    casts uint8_t buffers to uint32_t on a little-endian core (the bytes
    end up MSB-to-LSB within each register, then get reassembled in memory in
    reverse word order). FUS, by contrast, loads the user key as-is.
    """
    assert len(data) % 4 == 0
    return b"".join(data[i:i + 4][::-1] for i in range(0, len(data), 4))


def load_keys(path: Path) -> List[bytes]:
    if not path.exists():
        sys.exit(
            f"Key file not found: {path}\n"
            f"Create it with {N_SLOTS} lines, each a 64-char hex AES-256 key, "
            f"in slot order (slot 1 first)."
        )

    keys: List[bytes] = []
    for lineno, raw in enumerate(path.read_text().splitlines(), start=1):
        stripped = raw.strip()
        if not stripped or stripped.startswith("#"):
            continue
        try:
            key = bytes.fromhex(stripped)
        except ValueError as e:
            sys.exit(f"{path}:{lineno}: invalid hex: {e}")
        if len(key) != KEY_SIZE:
            sys.exit(
                f"{path}:{lineno}: expected {KEY_SIZE}-byte key, "
                f"got {len(key)} bytes"
            )
        keys.append(key)

    if len(keys) != N_SLOTS:
        sys.exit(f"Expected {N_SLOTS} keys, got {len(keys)} in {path}")
    return keys


def format_c_array(name: str, blocks: List[bytes]) -> str:
    indent = " " * 4
    lines = [
        f"static const uint8_t {name}"
        f"[ENCLAVE_FACTORY_KEY_SLOTS][ENCLAVE_SIGNATURE_SIZE] = {{"
    ]
    for block in blocks:
        hex_bytes = ", ".join(f"0x{b:02x}" for b in block)
        lines.append(f"{indent}{{{hex_bytes}}},")
    lines.append("};")
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate enclave verify-vectors from AES-256 slot keys."
    )
    parser.add_argument(
        "--keys",
        type=Path,
        default=Path(__file__).parent / "enclave_keys.txt",
        help="path to file with %d hex AES-256 keys (default: enclave_keys.txt "
             "next to this script)" % N_SLOTS,
    )
    args = parser.parse_args()

    keys = load_keys(args.keys)

    iv_blocks: List[bytes] = []
    input_blocks: List[bytes] = []
    expected_blocks: List[bytes] = []

    for key in keys:
        # Generate the raw bytes that will land verbatim in the C arrays.
        iv_raw = secrets.token_bytes(BLOCK_SIZE)
        plaintext_raw = secrets.token_bytes(BLOCK_SIZE)

        # On the device these bytes get reinterpreted as little-endian 32-bit
        # words and written into the AES_IVRx / AES_DINR registers, which
        # places them MSB-first inside each AES word — effectively a word-swap
        # compared to "natural" AES byte order. The key, loaded via FUS, is
        # consumed as-is. So we feed PyCryptodome the swapped IV/plaintext and
        # swap the resulting ciphertext back into raw byte order.
        cipher = AES.new(key, AES.MODE_CBC, bswap32(iv_raw))
        expected_raw = bswap32(cipher.encrypt(bswap32(plaintext_raw)))

        iv_blocks.append(iv_raw)
        input_blocks.append(plaintext_raw)
        expected_blocks.append(expected_raw)

    print("/* Generated by scripts/enclave_verify_gen.py */")
    print(format_c_array("enclave_signature_iv", iv_blocks))
    print()
    print(format_c_array("enclave_signature_input", input_blocks))
    print()
    print(format_c_array("enclave_signature_expected", expected_blocks))


if __name__ == "__main__":
    main()
