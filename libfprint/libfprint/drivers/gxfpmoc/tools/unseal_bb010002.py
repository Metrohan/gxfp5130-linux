from __future__ import annotations

import argparse
import ctypes
import hashlib
import os
import platform
import sys
from ctypes import wintypes


CONST_1 = bytes.fromhex("9d7992b38402b66c81d1f555218942a9")
CONST_2 = bytes.fromhex("1848d71550d270d219c80632ab4f8bb3")
CONST_3 = bytes.fromhex("e47c8938db5250f0205617ee17da4eb4")

def read_all(path: str) -> bytes:
    with open(path, "rb") as f:
        return f.read()


def write_all(path: str, data: bytes) -> None:
    with open(path, "wb") as f:
        f.write(data)


def derive_root_key() -> bytes:
    out = bytearray(16)
    for i in range(8):
        out[i] = CONST_1[i] ^ CONST_2[i] ^ CONST_1[i + 8]
    for i in range(8, 16):
        out[i] = CONST_2[i] ^ CONST_3[i] ^ CONST_3[i - 8]
    return bytes(out)


def derive_entropy(seed8: bytes) -> bytes:
    seed_hash = hashlib.sha256(seed8).digest()
    session_key = hashlib.sha256(seed_hash[:16] + derive_root_key()).digest()
    return seed_hash[16:] + session_key


class _DATA_BLOB(ctypes.Structure):
    _fields_ = [("cbData", wintypes.DWORD), ("pbData", ctypes.POINTER(ctypes.c_byte))]


def _bytes_to_blob(data: bytes) -> _DATA_BLOB:
    buf = (ctypes.c_byte * len(data))()
    ctypes.memmove(buf, data, len(data))
    return _DATA_BLOB(cbData=len(data), pbData=ctypes.cast(buf, ctypes.POINTER(ctypes.c_byte)))


def crypt_unprotect_data(ciphertext: bytes, entropy: bytes | None) -> bytes:
    crypt32 = ctypes.WinDLL("crypt32", use_last_error=True)
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

    CRYPTPROTECT_UI_FORBIDDEN = 0x1
    CRYPTPROTECT_LOCAL_MACHINE = 0x4

    CryptUnprotectData = crypt32.CryptUnprotectData
    CryptUnprotectData.argtypes = [
        ctypes.POINTER(_DATA_BLOB),
        ctypes.POINTER(ctypes.c_void_p),
        ctypes.POINTER(_DATA_BLOB),
        ctypes.c_void_p,
        ctypes.c_void_p,
        wintypes.DWORD,
        ctypes.POINTER(_DATA_BLOB),
    ]
    CryptUnprotectData.restype = wintypes.BOOL

    LocalFree = kernel32.LocalFree
    LocalFree.argtypes = [ctypes.c_void_p]
    LocalFree.restype = ctypes.c_void_p

    in_blob = _bytes_to_blob(ciphertext)
    ent_blob = _bytes_to_blob(entropy) if entropy is not None else None

    attempts = [
        0,
        CRYPTPROTECT_UI_FORBIDDEN,
        CRYPTPROTECT_UI_FORBIDDEN | CRYPTPROTECT_LOCAL_MACHINE,
        CRYPTPROTECT_LOCAL_MACHINE,
    ]

    last_err = None
    last_flags = None
    for flags in attempts:
        out_blob = _DATA_BLOB()
        descr_ptr = ctypes.c_void_p()

        ok = CryptUnprotectData(
            ctypes.byref(in_blob),
            ctypes.byref(descr_ptr),
            ctypes.byref(ent_blob) if ent_blob is not None else None,
            None,
            None,
            flags,
            ctypes.byref(out_blob),
        )
        if ok:
            try:
                return ctypes.string_at(out_blob.pbData, out_blob.cbData)
            finally:
                if out_blob.pbData:
                    LocalFree(out_blob.pbData)
                if descr_ptr.value:
                    LocalFree(descr_ptr)

        last_err = ctypes.get_last_error()
        last_flags = flags

    raise OSError(
        last_err if last_err is not None else 1,
        f"CryptUnprotectData failed (last_flags=0x{(last_flags or 0):08x})",
    )


def find_goodix_cache_bin(extra_roots: list[str] | None = None) -> str:
    roots: list[str] = []
    program_data = os.environ.get("ProgramData")
    if program_data:
        roots.append(os.path.join(program_data, "Goodix"))
        roots.append(program_data)

    if extra_roots:
        roots.extend(extra_roots)

    candidates: list[tuple[float, str]] = []
    seen = set()
    for root in roots:
        if not root or root in seen:
            continue
        seen.add(root)
        if not os.path.exists(root):
            continue

        for dirpath, _, filenames in os.walk(root):
            for name in filenames:
                if name.lower() == "goodix_cache.bin":
                    p = os.path.join(dirpath, name)
                    try:
                        mtime = os.path.getmtime(p)
                    except OSError:
                        mtime = 0.0
                    candidates.append((mtime, p))

    if not candidates:
        raise FileNotFoundError("Goodix_Cache.bin not found")

    candidates.sort(key=lambda x: x[0], reverse=True)
    return candidates[0][1]


def unseal_cache(cache_bytes: bytes) -> tuple[bytes, bytes]:
    seed8 = cache_bytes[-8:]
    blob = cache_bytes[:-8]
    entropy = derive_entropy(seed8)
    psk = crypt_unprotect_data(blob, entropy)

    if len(psk) != 32:
        raise ValueError(f"unexpected decrypted length: {len(psk)}")

    return psk, seed8


def main() -> int:
    ap = argparse.ArgumentParser(description="Extract RAW PSK from Goodix_Cache.bin")
    ap.add_argument("--cache", help="Path to Goodix_Cache.bin. If omitted, auto-search is used.")
    ap.add_argument("--out-psk", default="psk_raw32.bin", help="Output PSK path (default: psk_raw32.bin)")
    ap.add_argument("--search-root", action="append", default=[], help="Additional search root(s)")
    args = ap.parse_args()

    try:
        cache_path = args.cache if args.cache else find_goodix_cache_bin(args.search_root)
        cache_data = read_all(cache_path)
        psk, seed8 = unseal_cache(cache_data)
        write_all(args.out_psk, psk)
    except Exception as e:
        sys.stderr.write(f"ERROR: {e}\n")
        return 1

    sys.stderr.write(f"cache: {cache_path}\n")
    sys.stderr.write(f"seed8: {seed8.hex()}\n")
    sys.stderr.write(f"wrote RAW PSK: {args.out_psk} (32 bytes)\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
