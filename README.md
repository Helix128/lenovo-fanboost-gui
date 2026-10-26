# lenovo-fanboost-gui

A native Qt 6 desktop client for the Linux `lenovo-fanboost` service. Use it to select and edit fan profiles, view live readings, and manage the service.

Fan control is provided by the `lenovo-fanboost` daemon and kernel driver. Install the backend before using the GUI to control hardware. The GUI does not control fans on its own.

## Profiles and fan control

The sidebar shows the current fan mode, configured profile, and saved profiles. Selecting a profile previews it. Choose **Save and use** to apply it. Curve edits stay as a draft until saved; editing a built-in profile creates a custom draft. If a draft is open, the app asks whether to save or discard it before switching profiles.

**Firmware automatic** and **Fan curve** switch the service mode immediately. This is separate from saving profile edits. The RPM value is a requested target, not a measured fan speed. A zero request returns control to firmware; the fan may continue to run.

## Features

- CPU temperature and fan request appear beside the curve.
- Edit curves by dragging points or entering values in the numeric table.
- Create, duplicate, rename, delete, import, and export profiles. Profile files use version 1 JSON.
- View up to 600 session samples in separate temperature and fan request plots and a timestamped table.
- Set separate startup options for the app at login and the fan-control service at boot.
- Select a light or dark appearance, use the system tray, and copy diagnostics.

## Requirements

- Qt 6 Core, Gui, Widgets, and Network development packages
- CMake 3.20 or newer and a C++20 compiler
- The installed `lenovo-fanboost` backend, kernel driver, and systemd service (required for hardware telemetry and fan control)
- A running system Polkit agent for privileged service operations

Telemetry comes from hwmon. A reading is unavailable when the driver or sensor does not provide it.

## Build and install

```sh
cmake -S . -B build
cmake --build build
sudo cmake --install build
```

To build without Qt Test targets, add `-DBUILD_TESTING=OFF` to the configure command.

## Run

```sh
lenovo-fanboost-gui
lenovo-fanboost-gui --tray
```

Only one regular instance runs per user. Launching the application again raises its existing window.

## Tests

With testing enabled, run Qt tests using the offscreen platform:

```sh
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

## License

GPL-2.0-only. See [LICENSE](LICENSE).

**Disclaimer:** This is an independent, community-developed project and is not affiliated with, endorsed by, or supported by Lenovo. Lenovo® is a trademark of Lenovo.
