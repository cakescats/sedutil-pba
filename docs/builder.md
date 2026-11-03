# Image builder

`tools/cakes-builder.py` makes your own PBA image from a released one, with
your settings and logo. It takes a second and needs no Buildroot, root or
mtools. It is one Python 3 file with no dependencies, and runs on Linux,
macOS and Windows.

```
python3 cakes-builder.py build -i cakes-pba-<version>-default.img.gz -o my-pba.img \
    --set "title=ACME Corp Pre-Boot Authorization" --set timeout=10 \
    --logo acme.png --gzip
python3 cakes-builder.py show my-pba.img
```

Then load `my-pba.img` with `sedutil-cli --loadPBAimage` as usual.

## Options

| Option | Does |
|---|---|
| `-i`, `--image` | The image to start from: a release, `.img` or `.img.gz`, or an image the builder made before. |
| `-o`, `--output` | The new image. A `.sha256` file is written next to it. |
| `--set key=value` | One setting from [configuration.md](configuration.md), applied on top. Repeat it for more. |
| `--config file` | A whole `cakes-pba.conf` to use instead of the image's own. `--set` still applies on top. |
| `--logo file.png` | An 8-bit RGB or RGBA PNG. Transparency is blended onto the PBA background, and logos larger than 192 px are scaled down. |
| `--file src:/dest` | Any extra file for the PBA's file system, e.g. for skins later. |
| `--gzip` | Also write `.img.gz`. |

The settings start from the input image: its overlay if it has one,
otherwise its built-in `/etc/cakes-pba.conf`. So rebuilding a custom image
with one more `--set` keeps everything else. Unknown keys and a malformed
`service_hash` are refused before anything is written.

Nothing in the image is secret: anyone can read the shadow MBR without a
password. Don't put passwords into settings or files. The `service_hash`
from `tools/mkservicehash.py` is a slow salted hash, made for exactly this.

## How it works

The Linux kernel unpacks its initramfs from one or more cpio archives in a
row, and a later archive replaces files of an earlier one. GRUB can load
several files as the initramfs, so the builder doesn't touch the released
`cakes/rootfs.cpio.xz`. Instead it:

1. writes a small `cakes/overlay.cpio` with `/etc/cakes-pba.conf`, the logo
   (`/usr/share/cakes-pba/logo.ppm`) and any `--file`;
2. changes every `initrd /cakes/rootfs.cpio.xz` line in `EFI/BOOT/grub.cfg`
   into `initrd /cakes/rootfs.cpio.xz /cakes/overlay.cpio`;
3. writes the EFI partition again as FAT16, with the same size, label and
   volume ID. The partition table is not touched.

The kernel, GRUB and the base initramfs stay byte for byte the release's,
so their checksums can still be compared with the release. The output is
reproducible: the same input and options always give the same image.

`scripts/qemu-smoke.sh` boots builder images too. CI builds one from every
image it makes, checks the FAT with `fsck.fat`, and boots it.
