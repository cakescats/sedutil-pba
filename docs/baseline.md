# Baseline: the images this fork replaces

This is what was found in the three images in use before the fork, all
UEFI-only. Each is a GPT disk with one FAT ESP holding `syslinux.efi`,
`bzImage` and `rootfs.cpio.xz`.

| Image | Kernel | Command line | Origin |
|---|---|---|---|
| `FIXED_UEFI64_1.20.0.img` | 4.11.8-DTA (DTA build, 2021) | `loglevel=0 libata.allow_tpm=1 reboot=warm` | official 1.20 image with `acpi=off noapic` removed and `reboot=warm` added |
| `linux-6.6.29_UEFI64.img` | 6.6.29 | `loglevel=0 libata.allow_tpm=1` | zygisk22, sedutil#404 |
| `linux-6.13.3_nosmp_UEFI64.img` | 6.13.3 | `loglevel=0 nosmp libata.allow_tpm=1` | zygisk22, sedutil#404 |

All three have the **same initramfs** (sha1 `43227c84…`):

- Buildroot 2017.05.1 with glibc 2.23.
- `linuxpba` and `sedutil-cli` 1.20.
- Kernel modules for 4.11.8 only. Under the 6.x kernels they are never loaded.

The FIXED image is the one that works across machines. It is the only one with
`reboot=warm`. A cold reset can cut power to the drive, and the drive locks again.

## Behaviour of the 1.20 PBA

- It asks for one password, tries it on every OPAL drive, waits 5 s and reboots.
  A second drive with a different password can't be unlocked.
- Typing `debug` as the password skips the reboot. Init then gives a `getty`
  login on the console, and `root` has an empty password. This is DTA's
  service mode, intended but undocumented.
- `S40network` runs `ifup -a`. The initramfs carries Wi-Fi, KVM and GPU modules.
- A drive scan stops at the first device that doesn't answer (`break`, not `continue`).

## Why the official images hang (sedutil#404)

- The 1.20 kernel command line has `acpi=off noapic`. On machines with recent
  firmware the kernel then freezes after unlocking.
- On Core Ultra and some new AMD and Asus machines, newer kernels still hang
  unless they boot with `nosmp`.
- jantman/sedutil 1.49.13+jantman1 applies both fixes on Buildroot 2017. It
  still fails on some hardware, such as the ThinkPad P14s Gen 6.

## What this fork keeps

- The sedutil 1.20 OPAL code and the default password hashing (PBKDF2-SHA1,
  75000 iterations, the drive serial number as salt). Drives set up with
  1.20 unlock unchanged.
- Warm reboot.
- `libata.allow_tpm=1`.
