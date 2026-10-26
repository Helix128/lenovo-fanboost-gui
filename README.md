# lenovo-fanboost-gui

A native Qt 6 GUI for the Linux `lenovo-fanboost` service. Fan control comes from its daemon and kernel driver; the GUI does not control hardware by itself.

Select a profile to preview it, then choose **Save and use** to apply it. Edit curves with the graph or numeric table. Changes to built-in profiles become custom profiles. Firmware automatic and Fan curve switch service modes immediately. Displayed RPM is the requested target; a zero request returns control to firmware.

The GUI also includes live temperature readings, profile import and export, session history in Diagnostics, system tray controls, and separate startup settings for the app and service.

## Build

Requires Qt 6 Core, Gui, Widgets, and Network, CMake 3.20+, and a C++20 compiler. Install `lenovo-fanboost` and its driver for hardware control. Privileged service actions require a system Polkit agent.

```sh
cmake -S . -B build
cmake --build build
sudo cmake --install build
```

Telemetry comes from hwmon. Values may be unavailable if the driver or sensor does not report them. The fan request is not a measured RPM value.

## Install from AUR

On Arch-based systems, install with an AUR helper:

```sh
yay -S lenovo-fanboost-gui
```

This also installs `lenovo-fanboost`, the backend required for hardware control.

## Run

```sh
lenovo-fanboost-gui
lenovo-fanboost-gui --tray
```

Launching it again raises the existing window.

## Tests

Run the tests with:

```sh
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

## License

GPL-2.0-only. See [LICENSE](LICENSE).

**Disclaimer:** This is an independent, community-developed project and is not affiliated with, endorsed by, or supported by Lenovo. Lenovo® is a trademark of Lenovo.
