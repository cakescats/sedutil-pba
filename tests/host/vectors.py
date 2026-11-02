#!/usr/bin/env python3
"""Reference hashes for test_main.cpp: PBKDF2 with the 20 byte drive serial as salt."""
import hashlib

serial = b"S5GXNF0R123456      "
pw = b"correct horse"
salt = bytes(range(16))
service = hashlib.pbkdf2_hmac("sha512", b"svc-pass", salt, 1000, 32).hex()
print(hashlib.pbkdf2_hmac("sha1", pw, serial, 75000, 32).hex(),
      hashlib.pbkdf2_hmac("sha512", pw, serial, 1000, 32).hex(),
      f"sha512:1000:{salt.hex()}:{service}")
