# Sleep, hibernation and the PBA

The PBA runs only when the firmware boots from the drive's shadow MBR. That
happens at a cold boot, and at a reboot that powered the drive off. Whether
sleep works depends on whether the drive loses power:

| State | Firmware boot path? | PBA runs? | Drive after resume |
|---|---|---|---|
| S3 (suspend to RAM) | no, resume vector | no | Usually loses power and locks. Some platforms keep it powered. |
| Modern Standby (S0ix) | no | no | Stays unlocked if the NVMe drive stays in D3hot. Locks if it goes to D3cold. |
| S4 (hibernate) | yes | yes | Unlocked by the PBA, then the OS resumes from its hibernation file. |
| Windows Fast Startup | yes (hybrid S4) | yes | Same as S4. |
| Warm reboot | yes | no | Drive stays powered and unlocked, and the shadow MBR is already done. |
| Cold reboot / power off | yes | yes | Locked. |

If a drive locks while the OS is running, the OS gets I/O errors, which shows
up as a hang or a BSOD. The PBA can't help then, because it doesn't run. The fix
has to come from the OS:

- **Linux**: the kernel's `sed-opal` can store the key and unlock on resume,
  using `sedutil-cli --prepareForS3Sleep` from the
  [sedutil-sleep](https://aur.archlinux.org/packages/sedutil-sleep-git) /
  Ralayax forks, or a systemd unit. The key then stays in kernel memory while
  the machine sleeps.
- **Windows**: there is no built-in support. Options are to keep the NVMe drive
  powered in Modern Standby, to use hibernate instead of S3, or to use a
  storage filter driver like [SEDSleep](https://github.com/lukefor/SEDSleep),
  which hard-codes the key in a driver.

Machines running the FIXED 1.20 image are reported to sleep fine, including
S3 on Windows 11. That suggests their drives keep power. The new image must
behave the same. Fill in the table below for each machine.

## How to check whether the drive locked

- Linux: `sudo sedutil-cli --query /dev/nvme0n1 | grep -i locked` after resume.
  `Locked = Y` means it locked. In practice the OS usually freezes first.
- Windows: Event Viewer, System log, `disk` or `stornvme` errors after
  resume. Or a freeze or BSOD within seconds of waking up.

## Test matrix

Record one row per machine × OS × state. Test with the FIXED image first, then
with the CakesCats image.

| Machine (model, CPU, BIOS) | OS | State | Image | Result | Notes |
|---|---|---|---|---|---|
| Desktop, Ryzen 7 7800X3D, KIOXIA XG8 3.84 TB in an M.2 slot | Ubuntu 26.04 (kernel 7.0) | deep (S3) | CakesCats and FIXED 1.20 | **locked after resume** | The board cuts power to the M.2 slot in S3; the journal shows the partition table re-read after resume. Same with both images, so the PBA is not the cause. The machine also woke up on its own after 9 to 18 s, and hung once. (2026-11-02) |
| | Windows 11 | S3 / Modern Standby | FIXED 1.20 | | |
| | Windows 11 | S3 / Modern Standby | CakesCats | | |
| | Windows 11 | Hibernate | CakesCats | | |
| | Windows 11 | Fast Startup shutdown | CakesCats | | |
| | Linux | s2idle | CakesCats | | `cat /sys/power/mem_sleep` |
| | Linux | deep (S3) | CakesCats | | |
| | Linux | hibernate | CakesCats | | |

Useful commands:

- Windows: `powercfg /a` lists the sleep states the machine offers.
- Linux: `cat /sys/power/mem_sleep`. `[s2idle]` is Modern Standby style,
  `[deep]` is S3.
