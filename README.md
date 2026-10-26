# lenovo-fanboost-gui

Qt6 companion interface and tray monitor for [lenovo-fanboost](https://github.com/Helix128/lenovo-fanboost).

Provides a continuous fan curve editor, live telemetry readouts, and profile switching for supported Lenovo laptops (ThinkBook, Yoga, and IdeaPad).

## Features

- **Curve Editor**: Graphically edit temperature vs fan percentage points (`[temp_celsius, percent]`). Points are draggable and constrained to monotonic fan speeds.
- **Stock and Custom Profiles**: Ships with three default presets (`Quiet`, `Balanced`, `Aggressive`) kept as read-only templates. Custom profiles can be created, renamed, or removed.
- **System Tray Monitor**: Background tray icon with real-time temperature badge, quick profile switcher, and an immediate toggle between `Active` and `Inactive (EC Mode)`.
- **Live Telemetry**: Real-time display of CPU temperature (`Tctl` from `k10temp`), target RPM, percentage, and EC fan register readback.
- **Desktop Privilege Escalation**: Uses `pkexec` when saving changes to `/etc/lenovo-fanboost.toml` or restarting the daemon.
- **Native Implementation**: Written in C++20 and Qt6 Widgets.

## Requirements

- `lenovo-fanboost` (kernel driver and daemon)
- `qt6-base`
- `cmake` (>= 3.20)
- C++20 compiler (`clang` or `gcc`)

## Building

```sh
cmake -B build -S .
cmake --build build
```

To install:

```sh
sudo cmake --install build
```

## Usage

Launch the main window:

```sh
lenovo-fanboost-gui
```

Start minimized to the system tray (for desktop autostart):

```sh
lenovo-fanboost-gui --tray
```

Only one instance can run at a time; launching a second instance brings the existing window to the front and exits.

## License

GPL-2.0-only. See [LICENSE](LICENSE).
