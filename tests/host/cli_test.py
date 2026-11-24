#!/usr/bin/env python3
"""sedutil-cli -j and -S against the stand-in drive of FakeDevOS.cpp.

    python3 cli_test.py ./sedutil-cli-fake
"""
import json
import subprocess
import sys

CLI = sys.argv[1] if len(sys.argv) > 1 else "./sedutil-cli-fake"
failures = 0


def run(*args, stdin=b""):
    p = subprocess.run([CLI, *args], input=stdin, capture_output=True, timeout=30)
    return p.returncode, p.stdout.decode(), p.stderr.decode()


def check(cond, what):
    global failures
    if not cond:
        failures += 1
        print("FAIL", what)


def jrun(*args, stdin=b""):
    """Run with -j: stdout must be exactly one JSON object on one line."""
    rc, out, err = run("-j", *args, stdin=stdin)
    lines = out.splitlines()
    check(len(lines) == 1, f"{args}: one line on stdout, got {out!r}")
    try:
        doc = json.loads(out)
    except ValueError:
        check(False, f"{args}: stdout is JSON: {out!r}")
        return rc, {}, err
    check(doc.get("code") == rc, f"{args}: code {doc.get('code')} == exit {rc}")
    check(doc.get("ok") == (rc == 0), f"{args}: ok matches exit {rc}")
    return rc, doc, err


# --scan lists every path, opened or not
rc, doc, err = jrun("--scan")
check(rc == 0, "scan exit 0")
devs = {d["path"]: d for d in doc.get("devices", [])}
check(list(devs) == ["/dev/fake0", "/dev/fake1", "/dev/fake9"], f"scan paths {list(devs)}")
d0 = devs.get("/dev/fake0", {})
check(d0.get("present") is True and d0.get("sed") is True, "fake0 is a SED")
check(d0.get("model") == 'FAKE "OPAL" SSD', f"model trimmed and escaped: {d0.get('model')!r}")
check(d0.get("serial") == "S5GXNF0R123456" and d0.get("firmware") == "FW1.0", "serial, firmware trimmed")
check(d0.get("interface") == "NVMe" and d0.get("ssc") == ["opal2"], "interface, ssc")
check(d0.get("locking") == {"supported": True, "enabled": True, "locked": True,
                            "mbrEnabled": True, "mbrDone": False, "mediaEncryption": True},
      f"locking {d0.get('locking')}")
check(d0.get("blockSid", {}).get("sidIsMsid") is False, "SID differs from MSID")
check(d0.get("opal2", {}).get("lockingUsers") == 8, "opal2 users")
check(d0.get("geometry", {}).get("logicalBlockSize") == 512, "geometry")
d1 = devs.get("/dev/fake1", {})
check(d1.get("sed") is False and d1.get("locking") is None and d1.get("ssc") == [], "fake1 not a SED")
check(devs.get("/dev/fake9") == {"path": "/dev/fake9", "present": False}, "fake9 did not open")
check("Cannot open /dev/fake9" in doc.get("errors", []), "open error reported")
check("Cannot open /dev/fake9" in err, "log lines go to stderr")

# --query and --isValidSED give the same device object
rc, q, _ = jrun("--query", "/dev/fake0")
check(rc == 0 and q.get("device") == d0, "query == scan entry")
rc, v, _ = jrun("--isValidSED", "/dev/fake0")
check(rc == 0 and v.get("device") == d0, "isValidSED == scan entry")

# errors: names for the exit codes
rc, doc, _ = jrun("--query", "/dev/fake1")
check(rc == 0x88 and doc.get("status") == "COMMAND_ERROR", f"non-SED query {rc} {doc.get('status')}")
check(any("unsupported disk" in e for e in doc.get("errors", [])), "non-SED message")
rc, doc, _ = jrun("--listLockingRanges", "pw", "/dev/fake0")
check(rc == 0x8b and doc.get("status") == "IO_ERROR", f"transport failure {rc} {doc.get('status')}")
rc, doc, _ = jrun("--bogus")
check(rc == 0x88 and doc.get("status") == "COMMAND_ERROR", "unknown command")
rc, doc, _ = jrun()
check(rc == 0x88 and "No command given, see --help" in doc.get("errors", []), "no command")
rc, doc, _ = jrun("-v", "--query", "/dev/fake0")
check(rc == 0, "-v keeps stdout JSON")

# -S: a password given as - comes from stdin and hashes like the argument
rc, out, _ = run("--printPasswordHash", "s3cret pass", "/dev/fake0")
argv_hash = out.strip()
check(rc == 0 and len(argv_hash) == 64, f"text hash {out!r}")
rc, doc, _ = jrun("-S", "--printPasswordHash", "-", "/dev/fake0", stdin=b"s3cret pass\n")
check(rc == 0 and doc.get("hash") == argv_hash, "stdin hash == argv hash")
rc, doc, _ = jrun("-S", "--printPasswordHash", "-", "/dev/fake0", stdin=b"s3cret pass\r\n")
check(doc.get("hash") == argv_hash, "CRLF line end is cut")
rc, doc, _ = jrun("-S", "--printPasswordHash", "-", "/dev/fake0", stdin=b"s3cret pass")
check(doc.get("hash") == argv_hash, "last line without newline")
rc, out, _ = run("--printPasswordHash", "", "/dev/fake0")
rc, doc, _ = jrun("-S", "--printPasswordHash", "-", "/dev/fake0", stdin=b"\n")
check(rc == 0 and doc.get("hash") == out.strip(), "empty line is the empty password")
rc, out, _ = run("-S", "--printPasswordHash", "-", "/dev/fake0", stdin=b"s3cret pass\n")
check(rc == 0 and out.strip() == argv_hash, "-S without -j")
rc, doc, _ = jrun("-S", "--printPasswordHash", "-", "/dev/fake0", stdin=b"")
check(rc == 0x83 and doc.get("status") == "BAD_ARGUMENT", f"empty stdin {rc}")
rc, doc, _ = jrun("-S", "--printPasswordHash", "pw", "/dev/fake0")
check(rc == 0x83, "-S needs a - argument")
rc, doc, _ = jrun("--printPasswordHash", "-", "/dev/fake0", stdin=b"other\n")
check(rc == 0 and doc.get("hash") != argv_hash, "without -S a - is the literal password")
# two passwords: both lines are needed, in argument order
rc, doc, _ = jrun("-S", "--setAdmin1Pwd", "-", "-", "/dev/fake0", stdin=b"old\n")
check(rc == 0x83 and any("password 2 of 2" in e for e in doc.get("errors", [])), "second line missing")
rc, doc, _ = jrun("-S", "--setAdmin1Pwd", "-", "-", "/dev/fake0", stdin=b"old\nnew\n")
check(rc == 0x8b, f"both lines read, then the fake drive fails: {rc}")
rc, doc, _ = jrun("-S", "--setAdmin1Pwd", "-", "new", "/dev/fake0", stdin=b"old\n")
check(rc == 0x8b, "only the - argument is read")

# text output did not change
rc, out, _ = run("--query", "/dev/fake0")
check(rc == 0 and "Locked = Y, LockingEnabled = Y" in out, "text query on stdout")
rc, out, _ = run("--scan")
check(out.startswith("Scanning for Opal compliant disks"), "text scan")

if failures:
    print(f"cli_test: {failures} failed")
    sys.exit(1)
print("cli_test: all passed")
