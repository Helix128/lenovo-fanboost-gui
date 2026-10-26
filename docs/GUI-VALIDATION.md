# GUI Validation

## Build and tests

The GUI built in Release mode with Qt 6 and GCC. All five Qt test suites passed:

- Curve editor and numeric input
- Configuration validation, atomic save, and rollback
- Workspace state and profile preview behavior
- Bounded session history
- Profile import/export and login startup

The tests use temporary files and local helper executables for privileged-operation paths. They do not authorize or perform a real service change or fan write.

## Hardware checks

A read-only check on the ThinkBook 14 G7 ARP read CPU temperature and the requested fan target through hwmon while the service was active. These values are device and service readings, not a thermal benchmark. GUI-triggered authorization and service changes were not tested.

## Accessibility and performance limits

Keyboard traversal at 200% application text size was checked, including focus scrolling to the curve editor controls. Accessible roles and Qt announcement events are covered at source and widget-test level. Orca is unavailable in the environment, so end-to-end AT-SPI behavior remains unverified. Platform high contrast and formal contrast ratios were not checked.

A 10-second offscreen UI fixture sample used 49,388 KiB RSS at both sample points; CPU rounded to 0.0% of one logical core at the available clock resolution. This fixture result has no comparable baseline and excludes live telemetry and compositor costs. It does not demonstrate a performance gain.

No thermal benchmark was run, and the project does not claim WCAG conformance.
