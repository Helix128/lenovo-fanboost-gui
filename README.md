# lenovo-fanboost-gui

A lightweight Qt6 interface for [lenovo-fanboost](https://github.com/Helix128/lenovo-fanboost), making it easier to control and monitor fan behavior on supported Lenovo ThinkBook, Yoga, and IdeaPad laptops.

It lets you customize fan curves, manage profiles, and monitor temperatures without having to manually edit configuration files.

## Features

- **Fan Curve Editor**: Create and adjust fan curves by dragging points on a graph.
- **Profile Management**: Create, edit, and switch between fan curve profiles.
- **System Tray**: Monitor temperatures, switch profiles, and toggle fan control from the tray.
- **Live Monitoring**: View CPU temperature, fan speed, and EC readings in real time.

## Requirements

- [lenovo-fanboost](https://github.com/Helix128/lenovo-fanboost) (kernel driver and daemon)
- `qt6-base`
- `cmake` (3.20+)
- C++20 compiler (GCC or Clang)

## Building

```bash
cmake -B build -S .
cmake --build build
```

To install:

```bash
sudo cmake --install build
```

## Usage

Launch the application:

```bash
lenovo-fanboost-gui
```

Or start minimized to the system tray:

```bash
lenovo-fanboost-gui --tray
```

Only one instance can run at a time. Launching it again will bring the existing window to the front.

## License

GPL-2.0-only. See [LICENSE](LICENSE).

**Disclaimer:** This is an independent, community-developed project and is not affiliated with, endorsed by, or supported by Lenovo. Lenovo® is a trademark of Lenovo.
