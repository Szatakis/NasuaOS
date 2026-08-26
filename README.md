![Logo](documentation/images/logo.png)

# NasuaOS

Modern hobby operating system primarily targeting **x86_64** computers, written from scratch in **C++**.

Experimental support is planned for **AArch64**, **LoongArch64**, and **RISC-V**. The architecture is designed to be portable, but these targets are currently untested.

NasuaOS is an experimental operating system focused on learning, low-level programming, kernel development, and building a complete system from the ground up.

## Community & Links

[![Discord](https://img.shields.io/discord/1525067180410929222.svg?logo=discord&logoColor=white&logoWidth=20&labelColor=7289DA&label=Discord&color=17cf48)](https://discord.gg/esxDrzsCTt)
&nbsp;&nbsp;
[![YouTube](https://img.shields.io/badge/YouTube-@sz.atakis-FF0000?logo=youtube&logoColor=white&logoWidth=20)](https://www.youtube.com/@sz.atakis)
&nbsp;&nbsp;
[![TikTok](https://img.shields.io/badge/TikTok-@szatakis-000000?logo=tiktok&logoColor=white&logoWidth=20)](https://www.tiktok.com/@szatakis)

## Project Information

[![GitHub last commit](https://img.shields.io/github/last-commit/Szatakis/NasuaOS)](https://github.com/Szatakis/NasuaOS)
[![Language](https://img.shields.io/badge/Language-C%2B%2B-00599C?logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![Architecture](https://img.shields.io/badge/Architecture-x86__64%20%7C%20ARM64%20%7C%20RISC--V%20%7C%20LoongArch64-FF8C00)](https://github.com/limine-bootloader/limine-cxx-template)
[![License](https://img.shields.io/github/license/Szatakis/NasuaOS)](LICENSE.md)

## Share NasuaOS

[![X](https://img.shields.io/badge/Share%20on-X-000000?logo=x&logoColor=white)](https://x.com/intent/tweet?text=Check%20out%20NasuaOS%20-%20a%20hobby%20operating%20system%20written%20from%20scratch:%20https://github.com/Szatakis/NasuaOS%20%23OSDev%20%23OperatingSystem)
[![Facebook](https://img.shields.io/badge/Share-Facebook-1877F2?logo=facebook&logoColor=white)](https://www.facebook.com/sharer/sharer.php?u=https://github.com/Szatakis/NasuaOS)
[![LinkedIn](https://img.shields.io/badge/Share-LinkedIn-0A66C2?logo=linkedin&logoColor=white)](https://www.linkedin.com/sharing/share-offsite/?url=https://github.com/Szatakis/NasuaOS)
[![Reddit](https://img.shields.io/badge/Share-Reddit-FF4500?logo=reddit&logoColor=white)](https://www.reddit.com/submit?title=Check%20out%20NasuaOS%20on%20GitHub&url=https://github.com/Szatakis/NasuaOS)
[![Telegram](https://img.shields.io/badge/Share-Telegram-0088CC?logo=telegram&logoColor=white)](https://t.me/share/url?url=https://github.com/Szatakis/NasuaOS&text=Check%20out%20NasuaOS%20on%20GitHub)

## Documentation & Development

[📚 Documentation](documentation/main_page.md)  
[🔨 Build Instructions](documentation/build_instructions/build_instructions.md)  
[🤝 Contributing](CONTRIBUTING.md)  
[📦 Download ISO/HDD builds](https://github.com/Szatakis/NasuaOS/tree/iso)

---

## About

NasuaOS is a hobby operating system developed from scratch with a focus on performance, simplicity, and learning low-level system programming.

The project includes its own kernel, memory manager, drivers, graphical user interface, shell, filesystem, and applications. Every component is designed specifically for NasuaOS without relying on existing operating system kernels.

The long-term goal is to create a modern, lightweight operating system capable of running on both virtual machines and real hardware.

## Screenshots

![Desktop](documentation/images/screenshots/desktop.png)

## Features

- Modern 64-bit kernel
- Physical & Virtual Memory Manager (PMM/VMM)
- Paging support
- ACPI support
- PCI device enumeration
- ATA PIO storage driver
- SATA storage driver (in plans)
- PS/2 keyboard
- USB keyboard (in plans)
- Framebuffer graphics
- Desktop environment (minimal)
- Built-in shell
- Bitmap font renderer
- Hardware interrupt handling
- Serial debugging (UART)
- Runs in QEMU, VirtualBox and on real hardware

## Documentation

Documentation for NasuaOS can be found in the `documentation/` directory.

Additional technical information, architecture notes and driver documentation will be added as the project grows.

## Building

Currently NasuaOS can be built on Linux using GCC and CMake.

[🔨 Build Instructions](documentation/build_instructions/build_instructions.md)  

## Prebuilt Images

Prebuilt NasuaOS images are available in the `iso` branch.

The branch contains automatically generated builds:

- `NasuaOS.iso` - bootable ISO image for QEMU, VirtualBox and real hardware
- `NasuaOS.hdd` - HDD disk image for virtual machines

You can download the latest images from:

[📦 Download ISO/HDD builds](https://github.com/Szatakis/NasuaOS/tree/iso)

# Contributing

Contributions, bug reports and feature suggestions are welcome.

If you would like to contribute, please open an issue or submit a pull request.


# Author

**Szatakis**

# Acknowledgments & Credits

This project includes components from the [bootloader](https://github.com/Szatakis/NasuaOS-Bootloader/) repository.

These components are part of my internal ecosystem. Please refer to the [bootloader](https://github.com/Szatakis/NasuaOS-Bootloader/) repository for full license information and original source documentation.

# License

NasuaOS is released under the MIT License.
