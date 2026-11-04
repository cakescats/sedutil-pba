# Roadmap

## Done

**Stage 1, the base image.** A reproducible Linux PBA that works like the 1.20
FIXED image on current hardware. It adds:

- several drives;
- partitions with their own password;
- SHA-512;
- default and hardened profiles;
- branding.

Tested on a real KIOXIA XG8, both in a VM with the drive passed through and on bare metal.

**Image builder** (`tools/cakes-builder.py`, [builder.md](builder.md)). It
makes a custom image from a released one in a second: settings, logo and
extra files go into an overlay cpio that GRUB loads after the initramfs. The
released kernel and initramfs stay untouched.

The stages below are separate pieces of work. Order and status are tracked here.

## Next

### Skins and the configurator

These come first because the GUI and corporate images build on them. The
overlay mechanism and the builder are done.

- **Skin**: a directory with `skin.ini` (colours, texts, layout options)
  plus PNG images and fonts. It holds:
  - the logo;
  - a background;
  - a helpdesk line ("Forgot the password? Call +…");
  - a legal banner;
  - options to turn off parts of the screen (for example the game).
- **Delivery without a rebuild**: through the builder's overlay, like
  settings and the logo today (`--skin dir` on top of `--file`).
- **Secure Boot** in the builder: sign GRUB and the kernel with the owner's
  keys through `sbctl`, or with shim.
- **Configurator**: a static web page (GitHub Pages). You pick the settings,
  upload a logo and see a preview, and the image is built in the browser
  (WebAssembly), so nothing is uploaded anywhere.

### Graphical interface

- **LVGL** (MIT) on the firmware framebuffer, with keyboard and touchpad
  through evdev. LVGL gives animations, themes, PNG and TrueType fonts and
  costs about 1 to 2 MB.
- The OPAL minimum for the shadow MBR is 128 MiB, and the current image is 32,
  so there is room. Larger images take longer to load with `--loadPBAimage`.
- The text interface stays as a fallback when there is no framebuffer (serial
  console) or when `ui = text` is set.
- **Animations:**
  - the cat blinks while waiting;
  - its ears go down after a wrong password;
  - a padlock opens for each unlocked drive;
  - a smooth transition to "Starting OS".
- **Mini-game** (fun mode): the cat runs and catches fish or yarn, reachable
  from the menu.
  - It runs before or after unlocking and never touches the drives or the
    password prompt.
  - It is off in the hardened image and with `game = off`. Corporate skins
    can turn it off.
  - It is an original game, our own design and art.
- **Skins** for the GUI work as in the previous section. Corporate customers
  get their logo, colours, texts and helpdesk contacts.

### Emergency erase password

- Typing a special password erases the data: crypto erase, which gives the
  ranges new keys (GenKey). It takes milliseconds, and the old data can't be
  recovered.
- Preferred design: a dedicated OPAL user (User9, for example) whose only
  right is GenKey on the ranges. The ACE rights are set with sedutil-cli, the
  same way range passwords are. The password then lives in the drive only, and
  the image holds no secret.
- Fallback for drives that refuse such ACE changes: the Admin1 password
  encrypted with a slow KDF (Argon2id) of the erase password and stored in the
  image. That design must be documented honestly, because the image can be
  read and attacked offline.
- Off by default, with an explicit warning in the manual. An erase with
  confirmation, from the service shell or the admin menu, comes with it.

### PIN

- The drive password is kept in the image, encrypted with a slow KDF of a
  short PIN. A TPM 2.0 is optional: sealed to PCRs, it adds anti-hammering.
- The documentation must say plainly that a short PIN without a TPM can be
  brute-forced offline.

### USB key

- A key file on a USB stick, alone or together with a password.
- FAT, exFAT and NTFS: the kernel already has the drivers.

### Corporate unlock

This now lives in
[cakescats/sedutil-opal-enterprise](https://github.com/cakescats/sedutil-opal-enterprise).

- **Done:** SSO and MDM integration ([sso.md](sso.md)): the PBA hands off to the company SSO through a QR code. F2 in the PBA shows a
  signed one-time code, and the unlock server shows the password after an
  OpenID Connect sign-in. MDM rollout scripts and guides are done too.
- **TPM 2.0:** keep the machine key, and later the drive secret, sealed to
  the PBA's measured boot. The QR code then proves the machine itself, not
  only its drive, and the PBA can derive the password or a one-time code from
  the TPM.
- **Smart cards and NFC:** a PIV or OpenPGP card in a USB reader, or a phone
  over NFC, as a second factor or in place of the password.
- **Network unlock:** on a trusted LAN, unlock without user input, like
  BitLocker Network Unlock.
- **Mobile app:** approve an unlock with one tap, and the PBA fetches the key
  over the network.

### OS helpers for sleep

S3 was confirmed to lock the drive on a desktop, see
[sleep-matrix.md](sleep-matrix.md).

- **Linux:** a sedutil command that hands the key to the kernel's `sed-opal`
  (`IOC_OPAL_SAVE`), plus a systemd unit. The kernel then unlocks the ranges
  and sets MBRDone after resume.
- **Windows:** a service or tool that unlocks after resume, or a documented
  recommendation to use hibernate.

### OPAL tools for the running OS

- A command line tool for Windows and Linux: status, passwords, loading the
  PBA, PSID revert, sleep helpers. A GUI later.

### Native UEFI PBA (Rust)

- A UEFI application with no Linux kernel, which avoids the hangs from
  sedutil#404. It chainloads the OS without a reboot, uses the TPM through
  `EFI_TCG2_PROTOCOL` and is signed for Secure Boot.
- The Linux image stays in the same ESP as a fallback. Reference:
  [necauqua/opal-uefi-greeter](https://github.com/necauqua/opal-uefi-greeter).

## Small items

- Moving a password from SHA1 to SHA-512 in one command: today `-H` applies to the old and new password at once.
- Upstream `--objDump` segfaults in the middle of a session (use `--stackReset` after it).
- Smart cards and FIDO2, as part of the corporate stage.
