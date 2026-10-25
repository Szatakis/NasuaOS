# Doom for NasuaOS

A Doomgeneric port packaged as a NasuaOS `.napp` application, using the
Freedoom 2 IWAD. The IWAD is intentionally not included in Git; obtain the
official Freedoom 0.13.0 release and place it in `data/freedoom2.wad`. The
Doomgeneric source is vendored in
[`third_party/doomgeneric`](third_party/doomgeneric) and its full GPLv2 license
is in [`third_party/doomgeneric-LICENSE`](third_party/doomgeneric-LICENSE).

## Build

From this directory, run:

```sh
make
```

This produces `doom.napp` (the current build is below the 1 MiB NAPP image
limit). The directory includes its NAPP header and linker script, so it can be
built independently of the NasuaOS source tree with a GCC/G++ x86-64 toolchain.
Because NAPP images load at runtime-selected addresses, the build preserves
position-independent GOT references and packs required 64-bit pointer fixups
and GOT entries into the image; startup applies these fixups without using the
GOT itself. The build fails if unsupported absolute 32-bit references appear.
Build the complete OS image with:

```sh
make all
```

The OS build installs `data/freedoom2.wad` into the rootfs at
`/home/freedoom2.wad`, so the ISO includes the game data. The Doom app reads
the ClawFS copy first when present, then falls back to the ISO rootfs. The
build also installs the Doomgeneric GPLv2 license and Freedoom license and
credits in `/share/licenses/doom/`. The main NasuaOS license is unchanged.

The port requires NasuaOS NAPP ABI 7 and the matching kernel additions for
key-up events, FPU access, and ClawFS range reads. These changes are outside
this directory and must also be included in the OS when the app is integrated.
The current port targets x86-64 only.

## Freedoom data

The BSD-licensed Freedoom 2 IWAD is a separate file from `doom.napp`. Download
the official [Freedoom 0.13.0 release](https://github.com/freedoom/freedoom/releases/tag/v0.13.0)
and extract `freedoom2.wad` to `data/freedoom2.wad`. Its SHA-256 is
`a8772e088847032510d97ba2312406a6998f21cbab44d4ff10696faa9c0ecd4b`.
Place the IWAD at `data/freedoom2.wad` before building the ISO. The build
copies it into the ISO rootfs; no separate ClawFS installation is required.
The file is about 28.8 MB. It remains ignored by Git, so it must be supplied
locally when building from a fresh checkout. GitHub Actions downloads the
official release archive and verifies the IWAD checksum before building.

Freedoom's complete license and credits are included in
[`third_party/freedoom/`](third_party/freedoom/). The license requires
redistributions of the IWAD to retain the copyright notice and disclaimer;
see [`third_party/freedoom/COPYING.txt`](third_party/freedoom/COPYING.txt).

## Controls and limitations

- Up / Down: move forward / backward
- Left / Right: turn
- Hold Alt and press Left / Right: strafe
- `/` or Ctrl: fire (`/` is recommended in VirtualBox)
- Space: use/open doors
- Shift: run
- Tab: toggle the automap
- Escape: open the menu
- Enter: confirm menu choices

Use the window's maximize button (`[]`) to expand Doom to the desktop work
area; the image scales while keeping its 320x200 aspect ratio. Click the same
button again to restore the normal window. This is window maximization, not a
borderless mode, so the title bar and taskbar remain visible.

The game renders internally at 320x200 and does not provide sound, persistent
configuration, or save-game storage.

## Source and licensing

- Doomgeneric was obtained from
  [`ozkl/doomgeneric`](https://github.com/ozkl/doomgeneric), commit
  `e257a09527062bfb986b6e5e0780839b6cbd5b71`. Its source changes for NasuaOS
  are marked in the affected files. The engine and this combined Doom
  application are distributed under GPLv2; see
  [`third_party/doomgeneric-LICENSE`](third_party/doomgeneric-LICENSE).
- Freedoom 2 is a separate game-data project, distributed under the
  3-clause BSD license. Its release WAD, license, and credits are kept
  together under [`third_party/freedoom/`](third_party/freedoom/), with the
  WAD itself in [`data/`](data/).
- NasuaOS's MIT license remains unchanged. A copy for distributions of this
  standalone directory is included in [`LICENSE-NASUA.md`](LICENSE-NASUA.md).

When redistributing this directory, retain all of these separate licenses and
the upstream source notices.
