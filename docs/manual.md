# CakesCats PBA user manual

CakesCats PBA is a pre-boot authorization program for SSDs with hardware
encryption that follow TCG OPAL 2.0. The drive always encrypts its own data.
While it is locked, it doesn't show its contents to the computer. Instead it
shows a small boot image, the "shadow MBR". The computer boots that image, you
type the password, the PBA unlocks the drive and restarts into your OS.

[Русская версия](manual.ru.md)

## Contents

1. [Will your drive work](#1-will-your-drive-work)
2. [Downloads](#2-downloads)
3. [Try it without risk](#3-try-it-without-risk)
4. [Install](#4-install)
5. [Day to day](#5-day-to-day)
6. [Changing passwords](#6-changing-passwords)
7. [Partitions with their own password](#7-partitions-with-their-own-password)
8. [Settings](#8-settings)
9. [Sleep and hibernation](#9-sleep-and-hibernation)
10. [Updating the PBA](#10-updating-the-pba)
11. [When something goes wrong](#11-when-something-goes-wrong)
12. [Turning encryption off](#12-turning-encryption-off)
13. [What the PBA protects against](#13-what-the-pba-protects-against)

## 1. Will your drive work

- You need an internal SATA or NVMe SSD with **TCG OPAL 2.0**: most business
  models and many consumer ones (Samsung 9x0 PRO/EVO, Crucial, KIOXIA, WD,
  Kingston KC series and others). `sedutil-cli --scan` tells you (section 3).
- **Don't use drives in USB enclosures.** Many USB bridges pass OPAL commands
  through wrongly and can brick the drive.
- The computer must boot with UEFI and have Secure Boot off. Signed images are planned.
- It needs at least 160 MB of RAM. This matters for virtual machines: with
  128 MB the kernel panics.
- The drive passwords are OPAL passwords stored in the drive. Neither the PBA
  nor the image keeps them.

## 2. Downloads

From [Releases](https://github.com/cakescats/sedutil-pba/releases), or from
the CI artifacts on the Actions tab:

| File | What for |
|---|---|
| `cakes-pba-<version>-default.img.gz` | The PBA image for most people. It has a service shell. |
| `cakes-pba-<version>-hardened.img.gz` | No service shell, no network drivers, no boot menu editor. |
| `sedutil-cli-linux-x86_64` | Drive setup tool for Linux. It is static, so it runs on any x86_64 distribution. |
| `*.sha256` | Checksums. |

```bash
sha256sum -c --ignore-missing cakes-pba-*-default.img.sha256
gunzip -k cakes-pba-*-default.img.gz
chmod +x sedutil-cli-linux-x86_64
sudo ln -s "$PWD/sedutil-cli-linux-x86_64" /usr/local/bin/sedutil-cli
```

Setting up the drive is easiest from Linux. A live USB stick such as Ubuntu
is enough, even on a Windows-only computer. DTA's `sedutil-cli.exe` 1.20 is
compatible too, but has no SHA-512 and no partition passwords.

## 3. Try it without risk

**Does the drive do OPAL?**

```bash
sudo sedutil-cli --scan
sudo sedutil-cli --query /dev/nvme0
```

- `--scan` must show a `2` after the drive name, e.g. `/dev/nvme0  2  KIOXIA ...`.
  `No` means no OPAL.
- `--query` on a fresh drive shows `LockingEnabled = N` and `MBREnabled = N`.
- For NVMe, name the controller (`/dev/nvme0`), not the namespace (`/dev/nvme0n1`).

**Does the PBA boot on this computer?** Write the image to a spare USB stick.
This erases the stick:

```bash
sudo dd if=cakes-pba-<version>-default.img of=/dev/sdX bs=4M conv=fsync
```

Boot from it through the firmware boot menu. You should see the cat, the title
and the drive list, with your drive `not locked`.

**Write down the PSID** from the drive label (32 characters). It is the only
way back if the password is lost, and it **erases all data**.

## 4. Install

Back up first. These commands don't erase anything, but a typo in the password
or a power cut during setup can leave the data out of reach.

```bash
sudo sedutil-cli --initialSetup <password> /dev/nvme0
sudo sedutil-cli --loadPBAimage <password> cakes-pba-<version>-default.img /dev/nvme0
sudo sedutil-cli --enableLockingRange 0 <password> /dev/nvme0
sudo sedutil-cli --query /dev/nvme0     # LockingEnabled = Y, MBREnabled = Y
```

- `--initialSetup` makes you the owner (SID) and administrator (Admin1) with
  this password, and turns the shadow MBR on.
- `--loadPBAimage` writes the PBA into the shadow MBR, which takes up to a couple of minutes.
- `--enableLockingRange 0` locks the whole drive from the next power cycle.

Then **shut down completely**. A reboot keeps the drive powered and unlocked.

Tips:

- Don't use `debug` as a password: in the default image it opens the service shell.
- For PBKDF2-SHA512 password hashing, add `-H sha512` to **every**
  `sedutil-cli` command and set `hash = sha512` in the image (section 8).
- More drives are set up the same way, each with its own password if you
  like. The PBA only needs to be on the drive the firmware boots from.

## 5. Day to day

At power on you see the PBA:

```
            [cat]
   CakesCats Pre-Boot Authorization
   nvme0  KIOXIA KXG8AZN84T09   Z47F...   locked
     range 1                    16.0 GiB  locked, 5 tries left
   Password: ********
```

- **Type the password and press Enter.** It is tried on every locked drive.
- If something is still locked, a 3 second countdown follows. Start typing the
  next password: the first key stops the countdown and becomes its first
  character. Without a key press the OS starts and the rest stays locked.
- **Esc**, or Enter on an empty line, starts the OS right away once at least one drive is open.
- A wrong password shows "No locked drive accepted this password". Each
  password has a limit of failed tries until the next power cycle (5 on a KIOXIA XG8).
- After unlocking, the PBA does a warm reboot: the drive keeps power and stays
  open, and the firmware boots your OS.

**Boot menu.** Press Esc right after picking the drive or stick in the
firmware. It has these entries:

- normal start;
- **single CPU** (`nosmp`), for machines that hang before the password prompt;
- **service mode**: unlock, then open a shell instead of starting the OS;
- **Reboot**;
- **UEFI firmware settings**.

**Service shell** (default image only). Type `debug` instead of a password, or
use service mode. The shell has `sedutil-cli`, and the PBA log is in
`/tmp/pbaerror.log`. `exit` goes back to the password prompt. The shell can be
turned off, or put behind its own password (section 8).

## 6. Changing passwords

```bash
sudo sedutil-cli --setAdmin1Pwd <old> <new> /dev/nvme0
sudo sedutil-cli --setSIDPassword <old> <new> /dev/nvme0
```

Change both: SID owns the drive, and Admin1 is the one you type in the PBA.

An existing password can't yet be moved from SHA1 to SHA-512 in one command,
because `-H` applies to the old and the new password at once. Pick the scheme
at setup.

## 7. Partitions with their own password

OPAL can lock parts of the drive (locking ranges, usually up to 8) with
separate passwords. For example, a work partition can open only with a second
password. Admin1 stays the master and can open everything.

```bash
sudo sedutil-cli --setupPartitionRanges nvme0n1p2 <password A> <Admin1> /dev/nvme0
```

**The data in those partitions is lost**: the range gets a new key. Format them
after the command.

In the PBA, type Admin1 first, then the partition passwords, one by one or one
for several. See [partitions.md](partitions.md).

## 8. Settings

Settings live in `/etc/cakes-pba.conf` inside the image. Most can be
overridden in the boot menu (`EFI/BOOT/grub.cfg` on the image) as
`cakes.<key>=<value>`. The full list is in [configuration.md](configuration.md).
The main ones:

| Key | Default | Does |
|---|---|---|
| `timeout` | `3` | Seconds to wait for the next password. `0` starts the OS at once, `-1` waits until everything is open. |
| `hash` | `sha1:75000` | Hash schemes: `sha1`, `sha512` or both (`sha512,sha1`). |
| `reboot` | `warm` | Warm reset, so the drive keeps power. |
| `service` | `on` | Service shell: `on`, `off` or `password` (with `service_hash` from `tools/mkservicehash.py`). |
| `ranges` | `none` | Whether partition passwords are tried before Admin1. |

To change settings or the logo, make your own image from a released one with
the [image builder](builder.md). It takes a second and runs on Linux, macOS
and Windows:

```bash
python3 cakes-builder.py build -i cakes-pba-<version>-default.img.gz -o my-pba.img \
    --set timeout=10 --set "title=ACME Corp" --logo logo.png
```

Then load `my-pba.img` with `--loadPBAimage`. `cakes-builder.py` is attached
to every release and lives in `tools/`.

## 9. Sleep and hibernation

The PBA only runs at a cold boot. What happens on sleep depends on whether
the computer cuts power to the drive:

| Mode | Result |
|---|---|
| **Hibernate (S4)**, Windows Fast Startup | Works: the boot goes through the PBA, you type the password, the OS resumes. |
| **S3 (suspend to RAM)** | On the desktop we tested (Ryzen 7800X3D) the M.2 slot loses power and the drive is locked after resume, **with the original DTA image too**. The OS gets read errors. On some laptops the drive keeps power and everything works. |
| **Modern Standby (S0ix)** | Works if the drive doesn't go to D3cold. |

Until there are OS helpers:

- **Windows**: test sleep on your machine. If it hangs after resume, use
  hibernate instead of sleep (`powercfg /h on`, "Hibernate" in the power menu).
- **Linux**: run `cat /sys/power/mem_sleep`. With `[deep]` (S3), use
  hibernate or `s2idle`. A helper that saves the key in the kernel
  (`sed-opal`) and unlocks after S3 is planned.

Results are collected in [sleep-matrix.md](sleep-matrix.md).

## 10. Updating the PBA

```bash
sudo sedutil-cli --loadPBAimage <Admin1> cakes-pba-<new version>-default.img /dev/nvme0
```

Run it from the OS while the drive is unlocked. Before loading an image, or
unlocking a drive from a running Linux, **unmount the shadow FAT** if the
desktop mounted it (label `CAKESPBA`): `udisksctl unmount -b ...`. Otherwise
a stale FAT cache can be written over real data.

## 11. When something goes wrong

| Problem | What to do |
|---|---|
| The PBA hangs before the password prompt | Esc, then **single CPU**. If that doesn't help, open an issue with the model, CPU and BIOS version. |
| "failed (locked out, power cycle)" | The try limit is used up. Power off completely (some laptops need a wait), power on, type the right password. |
| The OS doesn't start after the password | Boot the PBA from a USB stick: it sees the drive as `locked`. Type the password, and after the warm reboot the firmware boots the OS from the drive. |
| `NO_SESSIONS_AVAILABLE` from sedutil-cli | A session got stuck because a command was interrupted. Run `sudo sedutil-cli --stackReset /dev/nvme0`, or wait up to 15 minutes. |
| Windows loses the drive after sleep | This is S3, see section 9. |
| Forgotten password | Only `--yesIreallywanttoERASEALLmydatausingthePSID <PSID> /dev/nvme0`. **This erases everything.** |
| Turn locking off | See section 12. |

## 12. Turning encryption off

This keeps the data:

```bash
sudo sedutil-cli --disableLockingRange 0 <Admin1> /dev/nvme0
sudo sedutil-cli --setMBREnable off <Admin1> /dev/nvme0
```

After this the drive no longer locks, and the PBA no longer shows. To give up
ownership completely, run `--revertNoErase <Admin1>` and then
`--revertTPer <SID>`, in this order. Without the first command,
`--revertTPer` erases the drive.

## 13. What the PBA protects against

**It protects:**

- a computer that is off, a stolen laptop, a removed drive: without the
  password the data can't be read. The drive keeps and uses the keys itself.

**It doesn't protect:**

- **a running computer.** The drive is open while it has power, and that
  includes sleep when power isn't cut.
- **the PBA image from being read.** Anyone can read the shadow MBR, so the
  image holds no secrets. Changing it needs Admin1.
- **against booting another system without Secure Boot.** Someone with
  physical access can boot their own system, but still can't read the drive
  without the password.

**Default and hardened:**

- The default image has a service shell and an editable boot menu. They are
  convenient, and they don't give access to the data without the password,
  but they are extra power for whoever is at the keyboard.
- The hardened image removes them. Choose it when strangers may get to the computer.
