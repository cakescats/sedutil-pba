# Hardware test checklist

QEMU can't emulate an OPAL drive. Unlocking has to be tested on real drives.

**Before you start**

- [ ] Use a spare OPAL SSD if you can. If the drive holds data, back it up.
- [ ] Write down the PSID from the label of every test drive.
- [ ] Check the image hash: `sha256sum -c cakes-pba-*.sha256`.

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
