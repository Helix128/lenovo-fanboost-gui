# GUI Review

## Interaction and layout

The workspace keeps profile selection, editing, and application distinct. Selecting a profile changes the preview. **Save and use** applies it. Curve edits remain drafts, and editing a built-in profile creates a custom copy. Mode controls describe their immediate service action separately from profile saving.

The sidebar gives saved profiles more room, with visible actions for custom profiles. CPU temperature and fan request sit beside the curve. Session history is part of Diagnostics. On compact windows and with enlarged text, content scrolls and primary actions remain reachable.

## Accessibility status

The curve graph has a synchronized numeric table. Controls expose accessible names, and status and validation updates use Qt accessibility events. Keyboard traversal at 200% application text size was checked in the workspace tests.

Orca is not installed in the validation environment. End-to-end AT-SPI behavior, platform high-contrast settings, and formal contrast measurements remain unverified. This review is not a WCAG conformance assessment.

## Runtime and performance status

A read-only check on the ThinkBook 14 G7 ARP read live CPU temperature and fan request values from the installed driver. No GUI-triggered privileged service operation was exercised. The demo idle sample has no incumbent baseline and does not establish a performance improvement. Fan thermals were not benchmarked.

See [GUI validation](GUI-VALIDATION.md) for the test scope and results.
