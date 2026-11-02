# Partitions with their own password

An OPAL drive can lock parts of itself separately: besides the global range
(everything else), it has locking ranges, 8 on most drives. sedutil-cli can
give a range its own password, and the PBA asks for those passwords one after
the other.

- **Admin1** (the password from `--initialSetup`) opens the global range and
  the shadow MBR, which is what the OS boots from. It can also open, lock and
  reset every range: it stays the master password.
- **A range password** opens only its range. Behind it is OPAL user `User<n+1>`
  for range `n`.

## Set up

Make the partitions first, then lock them. **The data in them is lost**: each
range gets a new encryption key, so format the partitions afterwards.

```
sedutil-cli --setupPartitionRanges nvme0n1p2 <password A> <Admin1 password> /dev/nvme0
sedutil-cli --setupPartitionRanges nvme0n1p3,nvme0n1p4,nvme0n1p6 <password B> <Admin1 password> /dev/nvme0
sedutil-cli --listLockingRanges <Admin1 password> /dev/nvme0
mkfs.ext4 /dev/nvme0n1p2   # and so on
```

`--setupPartitionRanges` reads the start and size of each partition from Linux,
takes a free range for it, and gives all of them the same password. Partitions
next to each other share one range. It refuses partitions that are mounted or
overlap a range already in use. It also sets the range to lock on power
cycles only (`LockOnReset = PowerCycle`), so the warm reboot from the PBA into
the OS keeps it open.

The same by hand, with block numbers:

```
sedutil-cli --setupLockingRange 1 <start> <length> <Admin1> /dev/nvme0   # new key: data lost
sedutil-cli --enableLockingRange 1 <Admin1> /dev/nvme0
sedutil-cli --setLockOnReset 1 powercycle <Admin1> /dev/nvme0
sedutil-cli --setRangeUser 1 <range password> <Admin1> /dev/nvme0
```

Other commands:

| Command | Does |
|---|---|
| `--setLockingRangeUser <n> <RW\|RO\|LK> <range password> <dev>` | unlock or lock a range with its own password, e.g. from the running OS |
| `--setLockingRange <n> <RW\|RO\|LK> <Admin1> <dev>` | the same with the master password |
| `--removeRangeUser <n> <Admin1> <dev>` | the range opens with Admin1 only again |
| `--setLockOnReset <n> <powercycle\|hwreset> <Admin1> <dev>` | lock on power cycle only, or also on a hardware reset |
| `--stackReset <dev>` | close every open session, see below |

## In the PBA

1. The first password is tried as Admin1. Once it works, the PBA reads the
   range table and lists the ranges under the drive, with their size and the
   tries left.
2. Ranges without a password of their own open with Admin1.
3. During the countdown, type the next password: the first key stops the
   countdown and is the first character of the password. Keys typed while the
   last password is still being checked are kept for the next one.
4. Each password is tried on every range that is still locked. One password
   can open several ranges.
5. Without a key press the OS starts with the remaining ranges locked. They
   can be opened later from the OS with `--setLockingRangeUser`.

By default a range password typed before the Admin1 password is not tried:
the PBA doesn't know yet which ranges exist. To allow it, list the ranges:
`ranges = 1,2,3` (or `auto` for 1..8) in the config, see
[configuration.md](configuration.md).

## Failed tries

Every authority (Admin1, each range user) has a TryLimit, 5 on the KIOXIA XG8
we tested. Each wrong password counts, until a successful login or the next
power cycle. At the limit the drive answers `AUTHORITY_LOCKED_OUT` and refuses
even the right password until it is powered off.

A password is tried on every locked range, so typing password A also costs
one try on the range of password B. The PBA reads the tries left through
Admin1, shows them, and stops trying a range that has none left; the screen
then says `locked out, power cycle`.

`--listLockingRanges` shows the tries of each range user and `LockOnReset`.

## Stuck sessions

A drive has one or a few sessions. A program killed in the middle of one
(Ctrl-C, a crash) leaves it open, and the drive answers
`NO_SESSIONS_AVAILABLE` until it times out, up to 15 minutes. sedutil-cli now
waits up to 30 seconds for a free session, ignores `SIGPIPE` (so
`sedutil-cli ... | head` is safe), and `--stackReset <dev>` closes all
sessions at once without a password.
