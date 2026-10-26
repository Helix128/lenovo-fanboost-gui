# lenovo-fanboost-gui

Native Qt6 companion GUI and system tray utility for [lenovo-fanboost](https://github.com/Helix128/lenovo-fanboost).

Provides a continuous fan curve editor, real-time hardware telemetry, and one-click profile switching for supported Lenovo laptops (ThinkBook, Yoga, and IdeaPad models).

## Features

- **Interactive Fan Curve Editor**: Graphically edit temperature vs fan percentage points (`[temp_celsius, percent]`) with drag-and-drop points, double-click to add, and right-click to delete.
- **Stock & Custom Profiles**: Ships with three default profiles (`Quiet`, `Balanced`, `Aggressive`) and supports creating, duplicating, and removing custom curves.
- **System Tray Integration**: Background tray icon displaying real-time CPU temperature badges, quick profile switching, and immediate toggle between `Active` and `Inactive (EC Mode)`.
- **Live Telemetry**: Real-time tracking of CPU temperature (`Tctl` from `k10temp`), target RPM and percentage, driver state, and raw EC register speed.
- **Polkit Privilege Escalation**: Clean desktop authentication prompts (`pkexec`) when updating `/etc/lenovo-fanboost.toml` or restarting the daemon.
- **Zero Electron Bloat**: Built in pure C++20 with Qt6 Widgets for minimal memory footprint and native integration with KDE Plasma and CachyOS/Arch Linux.

## Requirements

- `lenovo-fanboost` (kernel module and daemon)
- `qt6-base`
- `cmake` (>= 3.20)
- C++20 compliant compiler (`clang` or `gcc`)

## Building from source

```sh
cmake -B build -S .
cmake --build build
```

To install to system directories:

```sh
sudo cmake --install build
```

## Running

Launch the full window:

```sh
lenovo-fanboost-gui
```

Launch directly minimized to the system tray (ideal for autostart):

```sh
lenovo-fanboost-gui --tray
```

## License

GPL-2.0-only. See [LICENSE](LICENSE).
