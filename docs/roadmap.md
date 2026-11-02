# Roadmap

Stage 1 is the base image: a reproducible Linux PBA that works like the 1.20
FIXED image on current hardware, plus several drives and branding. The stages
below are separate pieces of work, done in order of demand.

## 2. Builder and provisioning

- `cakes-builder`: builds a custom image from a released one plus a config and
  a logo, without a full Buildroot build. Rewrites `cakes-pba.conf` inside the
  initramfs.
- Secure Boot: sign GRUB and the kernel with the owner's keys (`sbctl`) or
  shim. The official DTA images require Secure Boot to be off.
- Provisioning scripts on top of `sedutil-cli`: set up a drive, load the PBA,
  set a password per drive, test.

## 3. OPAL tools for the running OS

- A command line tool for Windows and Linux: status, change passwords, load
  the PBA, PSID revert, sleep helpers. `sedutil-cli` already has a Windows
  transport, the starting point.
- A GUI later.

## 4. PIN and emergency erase

- A PIN unlocks a secret (a drive password, or Admin1 for erase). The secret
  is encrypted with a key from a slow KDF (Argon2id or PBKDF2 with many
  iterations) and stored in the image. The image is readable by anyone, so a
  short PIN without a TPM can be brute-forced offline. The docs must say so.
- TPM 2.0 on top, optional: seal the secret to PCRs and let the TPM's
  anti-hammering slow down PIN guessing.
- Emergency erase password: cryptographic erase (GenKey on the locking
  ranges). Off by default, with an explicit warning.

## 5. USB key

- A key file on a USB stick, used alone or together with a password.
- FAT, exFAT and NTFS: the kernel already has `vfat`, `exfat` and `ntfs3`.
  The UEFI PBA (stage 6) would use efifs drivers.

## 6. Native UEFI PBA (Rust)

- A UEFI application that unlocks through `EFI_NVM_EXPRESS_PASS_THRU`,
  `EFI_ATA_PASS_THRU` and `EFI_STORAGE_SECURITY_COMMAND`. No Linux kernel, so
  none of the #404 hangs. Starts in well under a second.
- Chainloads the OS loader without a reboot, uses the TPM through
  `EFI_TCG2_PROTOCOL`, and is signed for Secure Boot.
- The Linux image stays in the same ESP as a fallback.
- Reference: [necauqua/opal-uefi-greeter](https://github.com/necauqua/opal-uefi-greeter) (MIT).

## 7. Corporate features

- Network unlock, similar to BitLocker Network Unlock or a Kerberos-style
  ticket. The network drivers already ship as modules in the default image.
- Smart cards and FIDO2.
- Write a design document first.

## OS-side sleep support

Do this where the [sleep matrix](sleep-matrix.md) shows a problem:

- Linux: a systemd unit that saves the OPAL key with the kernel's `sed-opal`.
- Windows: a guide to keeping the drive powered in Modern Standby. A filter
  driver, in the style of SEDSleep, is research only.
