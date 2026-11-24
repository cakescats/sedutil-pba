# sedutil-cli for programs: JSON output and passwords on stdin

Two global flags make `sedutil-cli` safe to drive from a GUI, an MDM script
or anything else that is not a person at a terminal:

- `-j`: stdout carries exactly one JSON object, the result. Log lines and
  progress go to stderr.
- `-S`: a password argument given as `-` is read from stdin, so it never
  appears in the process list (`ps`, Task Manager) or in shell history.

They go before the command, like `-v` or `-H`, and can be combined:

```
printf '%s\n' "$PASSWORD" | sedutil-cli -j -S --listLockingRanges - /dev/nvme0
```

Without the flags nothing changes: the text output, the arguments and the
exit codes are the same as before.

## `-j`: the result object

Every run prints one line with one object:

```json
{"ok":true,"code":0,"status":"SUCCESS", ...payload...}
{"ok":false,"code":1,"status":"NOT_AUTHORIZED","errors":["..."]}
```

| Field | Meaning |
|---|---|
| `ok` | `true` when the command succeeded, the same as exit code 0 |
| `code` | the exit code of the process, see [Exit codes](#exit-codes) |
| `status` | the name of that code |
| `errors` | the error lines logged during the run, oldest first. Present whenever something was logged, also on success: `--scan` reports here the drives it could not open |
| payload | the data of the command, below |

The exit code and `code` are always equal, so a caller that only wants
success or failure can ignore stdout.

Payload by command:

| Command | Payload |
|---|---|
| `--scan` | `devices`: array of [device objects](#device-object), one per drive the OS lists |
| `--query <device>` | `device`: one device object |
| `--isValidSED <device>` | `device`: one device object, `{"path":..., "present":false}` if it did not open |
| `--listLockingRanges <password> <device>` | `ranges`: array of [range objects](#range-object) |
| `--listLockingRange <n> <password> <device>` | `ranges`: just range n |
| `--printDefaultPassword <device>` | `msid`: the MSID string |
| `--printPasswordHash <password> <device>` | `hash`: hex of the hash sent to the drive |
| every other command | none: `ok`, `code` and `status` tell the outcome |

### Device object

```json
{
  "path": "/dev/nvme0",
  "present": true,
  "interface": "NVMe",
  "model": "KIOXIA KXG8AZNV1T02",
  "firmware": "10410104",
  "serial": "Z47F...",
  "sed": true,
  "ssc": ["opal2"],
  "locking": {
    "supported": true, "enabled": true, "locked": true,
    "mbrEnabled": true, "mbrDone": false, "mediaEncryption": true
  },
  "blockSid": {"sidIsMsid": false, "sidBlocked": false, "hardwareResetClears": true},
  "tper": {...}, "geometry": {...}, "opal2": {...}
}
```

- `present: false` and only `path`: the drive did not open. On Linux that
  usually means no root; on Windows, no administrator.
- `interface`: `ATA`, `SAS`, `NVMe`, `USB` or `other`.
- `sed`: the drive speaks a TCG SSC. `ssc` lists which: `opal2`, `opal1`,
  `enterprise`. An empty list and `"locking": null` mean a drive without
  hardware encryption that sedutil can manage.
- `locking` is the Locking feature of Discovery 0, the state a GUI shows:

  | | |
  |---|---|
  | `enabled: false` | not set up: factory state or reverted |
  | `enabled: true, locked: true` | set up, some range is locked now |
  | `enabled: true, locked: false` | set up, unlocked |
  | `mbrEnabled: true, mbrDone: false` | the host sees the PBA in place of the drive's start |

- `blockSid.sidIsMsid: true` means the SID password is still the factory
  MSID, i.e. nobody has taken ownership.
- `tper`, `geometry`, `opal2`, `opal1`, `enterprise`, `singleUser`,
  `dataStore` repeat the other Discovery 0 features, each only if the drive
  reports it. Field names follow `--query`.

Strings are trimmed of the padding drives put in them. Bytes outside
printable ASCII are written as `\u00XX`.

### Range object

```json
{
  "range": 1, "start": 2048, "length": 33554432,
  "readLockEnabled": true, "writeLockEnabled": true,
  "readLocked": true, "writeLocked": true,
  "lockOnReset": ["PowerCycle"],
  "ownPassword": true, "tryLimit": 5, "tries": 0
}
```

- `start` and `length` are in logical blocks.
- `lockOnReset`: the resets that lock the range again: `PowerCycle`,
  `HardwareReset`, `HotPlug`, `Programmatic`.
- `ownPassword`: the range has its own password (User n+1, see
  [partitions.md](partitions.md)). Only then `tryLimit` (0 means no limit)
  and `tries` (failed tries since power on).
- Enterprise drives also give `name` and `commonName`, and no `ownPassword`.

### Progress

`--loadPBAimage` takes minutes. With `-j` it writes one line to stderr per
percent:

```json
{"progress":{"done":1048576,"total":33554432,"percent":3}}
```

Other stderr lines are log text, so read the lines that start with
`{"progress"` and pass the rest on as log.

## `-S`: passwords on stdin

Each password argument that is exactly `-` takes one line of stdin, in the
order of the arguments:

```
printf '%s\n%s\n' "$OLD" "$NEW" | sedutil-cli -S --setAdmin1Pwd - - /dev/nvme0
```

- One line per password, ended by `\n` or `\r\n`. The last line may lack
  the newline. An empty line is the empty password, which some commands
  take as "use the MSID".
- Arguments that are not `-` are used as given, so a script can pass only
  the secret through stdin.
- The arguments that can be `-` are the passwords of a command: the old
  and new ones, the range password, the PSID.
- Without `-S` a `-` is a literal one-character password, as before.
- If stdin ends early the command does nothing and fails with
  `BAD_ARGUMENT` (0x83).
- On a terminal `-S` asks for each password with echo off, on stderr, and
  asks a new password twice. That is also the safe way to type a password
  by hand.
- The program overwrites the passwords it read before it exits.

Passwords are bytes, and the drive gets a hash of those bytes, so the same
bytes must reach the drive every time:

- A GUI should write UTF-8.
- On a Windows console `-S` reads Unicode and converts it to UTF-8.
- An argument on Windows uses the ANSI code page instead, so a non-ASCII
  password set as an argument and the same password through `-S` are
  different passwords to the drive.

ASCII passwords avoid the question.

Special characters are safe through `-S`: the line goes to the drive as
typed. As an argument the shell gets them first, and a password it has
changed is a different password:

| Shell | Changes | Safe as an argument |
|---|---|---|
| PowerShell | `@` at the start (splatting: the argument disappears), `$`, `` ` ``, `"` | in single quotes: `'@my$pass'` (a `'` inside is written `''`) |
| cmd.exe | `%`, `^`, `!`, `&`, `\|`, `<`, `>`, `"` | hardly; use `-S` |
| bash, zsh | `$`, `` ` ``, `\`, `!`, `"`, `'` | in single quotes |

So for passwords with special characters `-S` is the way on every system.

Windows and NVMe drives: up to the `gui-msi` changes, sedutil-cli on
Windows read no serial number from NVMe drives and hashed every password
with an empty salt, so a password set in Linux or the PBA was refused as
`NOT_AUTHORIZED`. It now reads the NVMe Identify data the way Linux does.
A password that was set on an NVMe drive by an older sedutil-cli on Windows
was hashed with that empty salt and does not match now (nor did it in the
PBA): change it with the older version first.

Examples:

```powershell
# PowerShell: the password never shows up in the command line
$pw | & sedutil-cli.exe -j -S --listLockingRanges - \\.\PhysicalDrive1 | ConvertFrom-Json
```

```python
import json, subprocess
p = subprocess.run(["sedutil-cli", "-j", "-S", "--setLockingRange", "0", "RW", "-", "/dev/nvme0"],
                   input=password.encode() + b"\n", capture_output=True)
result = json.loads(p.stdout)
if not result["ok"]:
    print(result["status"], result.get("errors"))
```

## Exit codes

Without `-j` the codes are the same, this table is their meaning.

| Code | `status` | Meaning |
|---|---|---|
| 0 | `SUCCESS` | done |
| 0x01 | `NOT_AUTHORIZED` | wrong password, or the authority may not do this |
| 0x03 | `SP_BUSY` | |
| 0x04 | `SP_FAILED` | |
| 0x05 | `SP_DISABLED` | the SP is not active |
| 0x06 | `SP_FROZEN` | |
| 0x07 | `NO_SESSIONS_AVAILABLE` | another session is open; `--stackReset` closes it |
| 0x08 | `UNIQUENESS_CONFLICT` | |
| 0x09 | `INSUFFICIENT_SPACE` | |
| 0x0A | `INSUFFICIENT_ROWS` | |
| 0x0B | `INVALID_FUNCTION` | |
| 0x0C | `INVALID_PARAMETER` | the drive rejected a value |
| 0x0D | `INVALID_REFERENCE` | |
| 0x0F | `TPER_MALFUNCTION` | |
| 0x10 | `TRANSACTION_FAILURE` | |
| 0x11 | `RESPONSE_OVERFLOW` | |
| 0x12 | `AUTHORITY_LOCKED_OUT` | too many wrong passwords: power cycle the drive |
| 0x3F | `FAIL` | |
| 0x81 | `UNSUPPORTED_LOCKING_RANGE` | no such range, or range 0 where it cannot be used |
| 0x82 | `OBJECT_CREATE_FAILED` | out of memory |
| 0x83 | `BAD_ARGUMENT` | an argument is wrong, or `-S` could not read a password |
| 0x84 | `OPEN_FAILED` | the PBA image file did not open |
| 0x85 | `INVALID_LIFECYCLE` | the Locking SP is already active (or not in the state the command needs) |
| 0x86 | `INVALID_COMMAND` | the command does not exist or does not apply to this drive |
| 0x87 | `AUTH_FAILED` | the drive refused an Authenticate inside a session: wrong password |
| 0x88 | `COMMAND_ERROR` | bad command line, or the device is not a SED |
| 0x89 | `NO_METHOD_STATUS` | the drive's answer had no status |
| 0x8A | `NO_LOCKING_INFO` | the drive did not report its locking ranges |
| 0x8B | `IO_ERROR` | the command did not reach the drive (ioctl, NVMe or ATA error, no permission). The raw value is in `errors` |
| 0xFF | `FAILED` | a catch-all in older code paths |

0x01 to 0x3F are TCG method status codes from the drive, 0x81 and up come
from sedutil. Before `IO_ERROR` existed a failed ioctl passed its errno on,
so EPERM (1) looked like `NOT_AUTHORIZED`.
