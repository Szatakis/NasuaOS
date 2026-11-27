<div align="center">

# NasuaOS

**A modern hobby operating system built from scratch.**

[![GitHub Last Commit](https://img.shields.io/github/last-commit/Szatakis/NasuaOS?style=flat-square)](https://github.com/Szatakis/NasuaOS/commits)
[![License](https://img.shields.io/github/license/Szatakis/NasuaOS?style=flat-square)](LICENSE.md)
[![Build](https://img.shields.io/github/actions/workflow/status/Szatakis/NasuaOS/build.yml?style=flat-square&label=Build)](https://github.com/Szatakis/NasuaOS/actions/workflows/build.yml)
[![Tests](https://img.shields.io/github/actions/workflow/status/Szatakis/NasuaOS/checks.yml?style=flat-square&label=Tests)](https://github.com/Szatakis/NasuaOS/actions/workflows/checks.yml)

![Architecture](https://img.shields.io/badge/Architecture-x86%20%7C%20AArch64%20%7C%20RISC--V-orange?style=flat-square)

[About](#about) · [Project Information](#project-information) · [Screenshots](#screenshots) · [Documentation](#documentation) · [Building](#building)

</div>

---

## About

NasuaOS is an experimental operating system developed from the ground up, focused on learning, low-level programming, kernel development, and building a complete system independently.

The project primarily targets **x86** architecture, with portability in mind. Experimental support is planned for **AArch** and **RISC-V**, although these targets remain untested.

The project includes its own kernel, memory manager, drivers, graphical user interface, shell, filesystem, and applications. Every component is designed specifically for NasuaOS without relying on existing operating system kernels.

The long-term goal is to create a modern, lightweight operating system capable of running on both virtual machines and real hardware.

## Project Information

| Property | Details |
|:---|:---|
| Project type | Hobby operating system |
| Primary architecture | x86 |
| Additional architectures | AArch, RISC-V |
| Primary language | C++ |
| Additional languages | C, Assembly |
| Development tools | Python |
| License | MIT |

## Target Architectures

| Architecture | Support |
|:---|:---|
| **x86_64** | Primary target |
| **AArch64** | Experimental |
| **RISC-V** | Experimental |

## Community & Links

<div align="center">

[![Discord](https://img.shields.io/badge/Discord-@szatakis-5865F2?logo=discord&logoColor=white)](https://discord.com/users/1285981257498624062)
[![YouTube](https://img.shields.io/badge/YouTube-@szatakis-FF0000?logo=youtube&logoColor=white)](https://www.youtube.com/@szatakis)
[![TikTok](https://img.shields.io/badge/TikTok-@szatakis-000000?logo=tiktok&logoColor=white)](https://www.tiktok.com/@szatakis)

</div>

## Share NasuaOS

<div align="center">

[![X](https://img.shields.io/badge/Share%20on-X-000000?logo=x&logoColor=white)](https://x.com/intent/tweet?text=Check%20out%20NasuaOS%20-%20a%20hobby%20operating%20system%20written%20from%20scratch:%20https://github.com/Szatakis/NasuaOS%20%23OSDev%20%23OperatingSystem)
[![Facebook](https://img.shields.io/badge/Share-Facebook-1877F2?logo=facebook&logoColor=white)](https://www.facebook.com/sharer/sharer.php?u=https://github.com/Szatakis/NasuaOS)
[![LinkedIn](https://img.shields.io/badge/Share-LinkedIn-0A66C2?logo=linkedin&logoColor=white)](https://www.linkedin.com/sharing/share-offsite/?url=https://github.com/Szatakis/NasuaOS)
[![Reddit](https://img.shields.io/badge/Share-Reddit-FF4500?logo=reddit&logoColor=white)](https://www.reddit.com/submit?title=Check%20out%20NasuaOS%20on%20GitHub&url=https://github.com/Szatakis/NasuaOS)
[![Telegram](https://img.shields.io/badge/Share-Telegram-0088CC?logo=telegram&logoColor=white)](https://t.me/share/url?url=https://github.com/Szatakis/NasuaOS&text=Check%20out%20NasuaOS%20on%20GitHub)

</div>

## Documentation & Development

| Resource | Description |
|:---|:---|
| [📚 Documentation](documentation/main_page.md) | Technical documentation |
| [🔨 Build Instructions](documentation/build_instructions/build_instructions.md) | How to build NasuaOS |
| [🤝 Contributing](CONTRIBUTING.md) | Contribution guidelines |
| [📦 Download ISO/HDD builds](https://github.com/Szatakis/NasuaOS/releases#release-latest) | Prebuilt system images |

## Screenshots

<div align="center">

![NasuaOS Desktop](documentation/images/screenshots/desktop.png)

</div>

## Documentation

Documentation for NasuaOS can be found in the `documentation/` directory.

Additional technical information, architecture notes and driver documentation will be added as the project grows.

## Building

Currently NasuaOS can be built on Linux using GCC and CMake.

For detailed build instructions, dependencies and configuration, see:

[🔨 **Build Instructions**](documentation/build_instructions/build_instructions.md)

## Prebuilt Images

Prebuilt NasuaOS images are available in the [releases](https://github.com/Szatakis/NasuaOS/releases#release-latest).

The branch contains automatically generated builds:

| Image | Description |
|:---|:---|
| `NasuaOS.iso` | Bootable ISO image for QEMU, VirtualBox and real hardware |
| `NasuaOS.hdd` | HDD disk image for virtual machines |

[![Download Builds](https://img.shields.io/badge/Download-ISO%20%2F%20HDD-blue?style=for-the-badge&logo=github)](https://github.com/Szatakis/NasuaOS/releases#release-latest)

## Contributing

Contributions, bug reports and feature suggestions are welcome.

If you would like to contribute, please open an issue or submit a pull request.

For contribution guidelines, see [CONTRIBUTING.md](CONTRIBUTING.md).

## Author

**Szatakis**

## Acknowledgments & Credits

This project includes components from the [NasuaOS Bootloader](https://github.com/Szatakis/NasuaOS-Bootloader/) repository.

These components are part of my internal ecosystem. Please refer to the [bootloader repository](https://github.com/Szatakis/NasuaOS-Bootloader/) for full license information and original source documentation.

## License

NasuaOS is released under the **MIT License**.

See [LICENSE.md](LICENSE.md) for details.

---