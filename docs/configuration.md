# Configuration

The PBA reads `/etc/cakes-pba.conf` from its initramfs, then `cakes.<key>=<value>`
options from the kernel command line. Kernel options are set in
`EFI/BOOT/grub.cfg` on the image, and they win over the file.

The `service`, `service_word` and `service_hash` keys are only read from the
file. A kernel option cannot turn the service shell on.

Nothing here is secret. Anyone can read the shadow MBR, and with it this
configuration, without a password.

| Key | Default | Meaning |
|---|---|---|
| `timeout` | `3` | Seconds to wait for a key after a password unlocked some but not all locked drives. A key lets you type another password for the rest. `0` starts the OS at once. `-1` keeps asking until every drive is unlocked. |
| `boot_delay` | `1` | Seconds the result stays on screen before the reboot. |
| `hash` | `sha1:75000` | Comma separated list of password hashing schemes to try, in order: `sha1[:iterations]` (sedutil 1.x) and `sha512[:iterations]` (500000 if the count is left out). Each extra scheme costs one more failed attempt per drive when the password is wrong. |
| `reboot` | `warm` | `warm`, `cold` or `keep`, the reset used to start the OS. Warm keeps the drive powered and therefore unlocked. |
| `service` | `on` (`off` in the hardened image) | `on`, `off` or `password`. Controls the service shell. |
| `service_word` | `debug` | Typed at the password prompt, opens the service shell. While the shell is enabled, a drive password equal to this word can't be used. |
| `service_hash` | | `sha512:<iterations>:<salt hex>:<hash hex>`, required for `service = password`. Make one with `tools/mkservicehash.py`. |
| `logo` | `on` | Draw the logo on the framebuffer. |
| `logo_file` | `/usr/share/cakes-pba/logo.ppm` | Binary PPM (P6). Build with `scripts/build.sh --logo file.png` to replace it. |
| `title` | `CakesCats Pre-Boot Authorization` | Title line. |
| `mode` | `normal` | `service` (set by the boot menu entry) unlocks first, then opens the shell instead of starting the OS. |

## Changing settings in a released image

The image is a GPT disk with one FAT partition. To change a setting:

1. Mount the partition, or use `mtools`.
2. Add `cakes.timeout=10`, for example, to `cakes_args` in `EFI/BOOT/grub.cfg`.
3. Load the image with `sedutil-cli --loadPBAimage`.

Changes to `/etc/cakes-pba.conf` need a rebuild: the file is inside `cakes/rootfs.cpio.xz`.

## Boot menu

Press Esc while the PBA starts:

- **CakesCats PBA**: normal start.
- **single CPU**: adds `nosmp`. Use it on machines that hang before the password prompt (sedutil#404, some Core Ultra and AMD laptops).
- **service mode**: unlocks, then opens the shell. Not in the hardened image.
- **UEFI firmware settings**.
