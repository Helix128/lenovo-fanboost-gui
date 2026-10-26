# Product

## Purpose

Lenovo Fan Boost GUI is a native Linux desktop interface for the `lenovo-fanboost` service. It provides fan profile selection, curve editing, live readings, diagnostics, and service controls.

## Platform and operation

The application uses C++20 and Qt 6 Widgets. It reads telemetry from hwmon, manages `/etc/lenovo-fanboost.toml`, and requests privileged service operations through the desktop authorization system. The daemon remains responsible for fan control. Routine monitoring does not require elevated privileges.

The configured profile, the profile being previewed, unsaved curve edits, the service state, and the current fan request are separate states. The interface labels these states and asks for an explicit save before applying profile changes.

## Hardware and telemetry

Hardware support comes from the installed `lenovo-fanboost` driver and daemon. The GUI does not add laptop compatibility. Readings may be unavailable when a kernel sensor or service value is missing. Requested RPM is a target, not a measured fan speed. A zero request returns control to firmware; it does not stop the fan.

## Design and accessibility

The interface follows the approved ThinkBook workspace layout and uses native Qt controls, light and dark palettes, keyboard navigation, and a numeric curve table. See [DESIGN.md](DESIGN.md) and the [surface brief](docs/GUI-SURFACE-BRIEF.md).

The implementation includes accessible names and Qt announcements. Orca and end-to-end AT-SPI behavior have not been verified, so the project does not claim WCAG conformance. See [validation notes](docs/GUI-VALIDATION.md).
