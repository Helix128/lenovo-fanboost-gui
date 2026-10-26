# GUI Surface Brief

## Direction

The approved design uses a ThinkBook workspace layout: fan mode and profiles sit in a left sidebar, with telemetry beside the curve editor. The [direction mockup](images/thinkbook-workspace.png) is an early visual reference, not a screenshot of the finished application.

## Interaction

- Selecting a profile previews it. **Save and use** applies the selected profile.
- Curve edits remain drafts until saved. Editing a built-in profile creates a custom draft.
- Firmware automatic and Fan curve change the service mode immediately. They do not apply an unsaved profile.
- Session history is part of Diagnostics.
- Requested RPM describes the target sent to the controller, not measured fan speed. A zero request returns control to firmware.

## Layout and accessibility

The sidebar gives profile selection room and keeps custom profile actions visible. The curve graph and numeric table share selection and validation. On narrow windows or enlarged text, content can scroll while primary actions remain available.

The interface supports keyboard navigation, accessible control names, and Qt accessibility announcements. Orca and end-to-end AT-SPI behavior remain unverified. The project makes no WCAG conformance claim.
