![Logo](https://raw.githubusercontent.com/Szatakis/NasuaOS/main/documentation/images/logo.png)

# NasuaOS Builds

Prebuilt images are available for testing in QEMU, VirtualBox, or on real hardware.

## Available Images

| Image | Description |
|-------|-------------|
| **NasuaOS.iso** | Bootable ISO image for CD/DVD or USB media. Compatible with QEMU, VirtualBox, and real hardware. |
| **NasuaOS.hdd** | Virtual hard disk image intended primarily for QEMU and other virtual machines. |

## Running in QEMU

```bash
qemu-system-x86_64 -hda NasuaOS.hdd
```

## Booting the ISO

```bash
qemu-system-x86_64 -cdrom NasuaOS.iso -boot d
```

You can also write the ISO image to a USB flash drive and boot it on supported hardware.
