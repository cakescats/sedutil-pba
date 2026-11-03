# Hardware test checklist

QEMU can't emulate an OPAL drive. Unlocking has to be tested on real drives.

## First test: one NVMe drive under Linux

The drive can be a spare one, which is the safest choice, or the NVMe that
Linux itself runs from. The second is the realistic case: after the PBA
unlocks the drive, the same drive boots Linux. Back it up first.

**What you need**

- The `.img` from a release, or from the `cakes-pba-default` artifact of a CI run.
- `sedutil-cli-linux-x86_64` from the `sedutil-cli` artifact. It is static, so it runs on any x86_64 distribution.
- A USB stick, and Secure Boot off.
- The drive's PSID, written down from its label. Revert by PSID erases the
  drive, but it is the only way back if the password is lost.

Use a test password, not `debug`: the service shell takes that word.

```
sha256sum -c --ignore-missing cakes-pba-*-default.img.sha256
chmod +x sedutil-cli-linux-x86_64
alias sedutil='sudo ./sedutil-cli-linux-x86_64'
```

**1. Does the drive do OPAL?**

```
sedutil --scan                    # /dev/nvme0 needs a 2 after the name (e.g. " 2 " or "12 "), "No" means no OPAL
sedutil --query /dev/nvme0        # LockingEnabled = N, MBREnabled = N for a fresh drive
```

sedutil 1.x takes the controller (`/dev/nvme0`), not the namespace (`/dev/nvme0n1`).

**2. Boot the PBA from a USB stick, drive untouched**

```
sudo dd if=cakes-pba-<version>-default.img of=/dev/sdX bs=4M conv=fsync
```

Boot from the stick:

- The logo appears, `nvme0` shows `not locked`, and the stick shows as `sda ... no OPAL`.
- If it hangs, press Esc at start and try the single CPU entry.
- Enter reboots.

**3. Turn on locking**

```
sedutil --initialSetup <pw> /dev/nvme0          # owner, locking SP, shadow MBR on
sedutil --loadPBAimage <pw> cakes-pba-<version>-default.img /dev/nvme0
sedutil --enableLockingRange 0 <pw> /dev/nvme0  # locks from the next power cycle
sedutil --query /dev/nvme0                      # LockingEnabled = Y, MBREnabled = Y
```

Then shut down completely. A reboot keeps the drive powered and unlocked.

**4. Power on**

- The PBA starts from the drive itself. A wrong password shows an error.
- The right password marks the drive `unlocked`, and a warm reboot follows.
  The firmware then boots the real contents of the drive.
- Back in Linux, `sedutil --query /dev/nvme0` shows `Locked = N` and `MBRDone = Y`.

**5. If the drive doesn't boot after step 4**

- Boot the PBA from the USB stick. It sees the internal drive as `locked`.
  Enter the password there, and it warm-reboots into the now unlocked drive.
- Or open the service shell on the stick (type `debug`) and turn locking off
  with step 6.
- Last resort, which **erases the drive**:
  `sedutil --yesIreallywanttoERASEALLmydatausingthePSID <PSID> /dev/nvme0`.

**6. Turn locking off again (data is kept)**

```
sedutil --disableLockingRange 0 <pw> /dev/nvme0
sedutil --setMBREnable off <pw> /dev/nvme0
```

To release ownership completely, follow the DTA wiki: `--revertNoErase`, then
`--revertTPer`. Read the wiki first, because in the wrong order `--revertTPer` erases.

**7. Sleep**

Running from the locked drive, try sleep, hibernate and wake. Note the
results in [sleep-matrix.md](sleep-matrix.md).

## Full checklist

**Before you start**

- [ ] Use a spare OPAL SSD if you can. If the drive holds data, back it up.
- [ ] Write down the PSID from the label of every test drive.
- [ ] Check the image hash: `sha256sum -c --ignore-missing cakes-pba-*.sha256`.

**1. Boot without touching the drive**

- [ ] Write the image to a USB stick and boot it. Secure Boot must be off.
- [ ] The logo, title and drive list appear, and every drive is listed with the right state.
- [ ] Esc at start shows the menu. Also try the `nosmp` entry.
- [ ] Machines that hung with the 6.x images from sedutil#404: does the default entry boot? Does `nosmp` boot?

**2. One drive**

- [ ] Set up the drive with `sedutil-cli` (README, Install), then power off.
- [ ] The PBA starts from the shadow MBR.
- [ ] A wrong password shows an error, and the prompt comes back.
- [ ] The right password unlocks the drive, and the OS starts after a warm reboot.
- [ ] Repeat with a drive set up by DTA 1.20 `sedutil-cli`, to check that hashing is compatible.
- [ ] Repeat with `-H sha512` and `hash = sha512` (rebuild, or use `cakes.hash=sha512` in grub.cfg).

**3. Two drives with different passwords**

- [ ] The first password unlocks drive A. A countdown appears, and a key press gives a second prompt.
- [ ] The second password unlocks drive B, and the OS starts.
- [ ] Without the key press, the OS starts with B still locked.
- [ ] One password shared by both drives unlocks both at once.

**4. Service shell**

- [ ] `debug` opens the shell. `sedutil-cli --scan` works there, and `exit` returns to the PBA.
- [ ] Hardened image: `debug` is treated as a password, and there is no service entry in the menu.

**5. Sleep**

- [ ] Fill in [sleep-matrix.md](sleep-matrix.md).

Send results to an issue or a PR against `docs/sleep-matrix.md`. Include the
machine model and BIOS version.
