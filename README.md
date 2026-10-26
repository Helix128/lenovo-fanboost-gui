# Lenovo Fan Boost

Lenovo Fan Boost is a native Qt 6 interface for the `lenovo-fanboost` Linux service. It combines profile selection, curve editing, live telemetry, settings, diagnostics, and session history.

## Profiles and fan control

The scrollable sidebar contains the fan mode, configured profile, and saved profiles. Click a profile or select it and press Enter to preview it. Apply the selected profile with the named Save and use button. If a curve draft is pending, the app asks whether to save or discard it first. Editing a built-in profile creates a custom draft. Use the named save button to persist curve edits.

**Firmware automatic** and **Fan curve** request immediate service mode changes. These controls are separate from saving curve edits. A zero fan request returns control to the firmware EC. It does not mean that the fan is stopped. Displayed RPM is a requested target, not a measured speed.

## Features

- Live CPU temperature and requested fan speed appear beside the curve.
- Edit curves by dragging points or entering values in the numeric table.
- Create, duplicate, rename, delete, import, and export profiles. Profile files use version 1 JSON.
- View up to 600 session samples in separate temperature and fan request plots and a timestamped table.
- Set separate startup options for the app at login and the fan-control service at boot.
- Select a light or dark appearance, use the system tray, and copy diagnostics.
- Run `--demo` to inspect sample data without hardware or system operations.

## Requirements

- Qt 6 Core, Gui, Widgets, and Network development packages
- CMake 3.20 or newer and a C++20 compiler
- For hardware telemetry and fan control, the installed `lenovo-fanboost` backend, kernel driver, and systemd service
- A running system Polkit agent for privileged service operations

Telemetry is read from hwmon. CPU temperature, requested RPM, and EC readings may be unavailable when their source is unavailable.

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
lenovo-fanboost-gui --demo
```

Only one regular instance runs per user. Launching the application again raises its existing window. Demo mode runs separately.

## Tests

With testing enabled, run Qt tests using the offscreen platform:

```sh
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

## License

GPL-2.0-only. See [LICENSE](LICENSE).

**Disclaimer:** This is an independent, community-developed project and is not affiliated with, endorsed by, or supported by Lenovo. Lenovo® is a trademark of Lenovo.
