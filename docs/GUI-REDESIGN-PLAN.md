# GUI Redesign

## Scope

The redesign keeps the application as a lightweight Qt Widgets client. The daemon remains responsible for fan control, hardware checks, and recovery. No new laptop compatibility is implied by GUI changes.

## Implemented workspace

The left sidebar contains the current service mode, configured profile, saved profile list, and navigation. The main area places live CPU temperature and fan request beside the curve editor. A numeric table supports precise editing alongside graph interaction.

Profiles can be created, duplicated, renamed, deleted, imported, and exported. Choosing a profile previews it; **Save and use** applies it. Changes to built-in profiles start a custom draft. Session history is available from Diagnostics.

## Engineering constraints

The implementation uses native Qt widgets and painting. Telemetry and privileged operations are asynchronous. Configuration writes are validated and recoverable. Session history has a bounded in-memory sample count. Demo mode remains explicit and does not perform hardware operations.

## Review

Build and test results and remaining accessibility limitations are recorded in [GUI validation](GUI-VALIDATION.md). The approved visual direction is described in the [surface brief](GUI-SURFACE-BRIEF.md).
