# 3ds-fastfetch

A [fastfetch](https://github.com/fastfetch-cli/fastfetch)-style system information tool
for the Nintendo 3DS. It prints a model-specific ASCII logo next to a column of
real system info read straight from the console.

## Features

- Model detection (Old/New 3DS, 2DS, XL variants) with matching ASCII art
- Username, region, language, serial number (with recomputed check digit)
- OS/firmware version, kernel version, Luma3DS CFW version
- CPU/SoC, screen panel vendors (New 3DS family), battery level & charging state
- SD card and NAND storage usage
- Wi-Fi signal strength, MAC address, RTC date/time

## Building

Requires [devkitARM](https://devkitpro.org/) and libctru.

```sh
make            # -> 3ds-fastfetch.3dsx
```

Run via the Homebrew Launcher on CFW.

## Credits & inspiration

This project was written independently in C, but takes inspiration from two
great 3DS projects — thanks to their authors:

- **[3DSident](https://github.com/joel16/3DSident)** by joel16 — reference for the
  3DS system-info APIs and the serial-number check-digit scheme.
  (3DSident is licensed under the Zlib license.)
- **[3dfetch](https://github.com/aliceinpalth/3dfetch)** by aliceinpalth — inspiration
  for the fastfetch-style logo-plus-columns layout.

## License

Copyright (C) 2026 xPsycho999

This program is free software: you can redistribute it and/or modify it under
the terms of the **GNU General Public License v3.0** as published by the Free
Software Foundation. See the [LICENSE](LICENSE) file for the full text, or
<https://www.gnu.org/licenses/gpl-3.0.html>.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.
