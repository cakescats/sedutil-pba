#!/usr/bin/env python3
"""Reference hashes for test_main.cpp: PBKDF2 with the 20 byte drive serial as salt."""
import hashlib

serial = b"S5GXNF0R123456      "
pw = b"correct horse"
salt = bytes(range(16))
service = hashlib.pbkdf2_hmac("sha512", b"svc-pass", salt, 1000, 32).hex()
# SSO token (LinuxPBA/PbaSso.cpp, server/unlock.py): challenge + HMAC-SHA512
# over "cakes-pba-sso-v1\0" + machine id + "\0" + challenge, base32
import base64
import hmac
challenge = bytes([1, 2, 3, 4])
msg = b"cakes-pba-sso-v1\0" + b"ACME-0042" + b"\0" + challenge
mac = hmac.new(bytes(range(32)), msg, hashlib.sha512).digest()[:6]
sso = base64.b32encode(challenge + mac).decode()
print(hashlib.pbkdf2_hmac("sha1", pw, serial, 75000, 32).hex(),
      hashlib.pbkdf2_hmac("sha512", pw, serial, 1000, 32).hex(),
      f"sha512:1000:{salt.hex()}:{service}", sso)
