#!/usr/bin/env python3
"""Make a service_hash line for /etc/cakes-pba.conf (service = password).

    tools/mkservicehash.py [--iterations N]

Reads the password twice from the terminal and prints
    service_hash = sha512:<iterations>:<salt hex>:<hash hex>
"""
import argparse
import getpass
import hashlib
import os
import sys

ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
ap.add_argument("--iterations", type=int, default=500000)
args = ap.parse_args()

pw = getpass.getpass("Service password: ")
if pw != getpass.getpass("Again: "):
    sys.exit("passwords differ")
if not pw:
    sys.exit("empty password")
salt = os.urandom(16)
digest = hashlib.pbkdf2_hmac("sha512", pw.encode(), salt, args.iterations, 32)
print(f"service_hash = sha512:{args.iterations}:{salt.hex()}:{digest.hex()}")
