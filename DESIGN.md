# Design

## Layout

The window opens at 1180 × 780 logical pixels and has a 600 × 440 minimum. A 238 to 288 pixel sidebar contains the fan mode, service state, configured profile, saved profiles, and navigation. The curve editor shows CPU temperature and fan request above the graph and numeric table. Save and discard actions stay in the footer.

Profile selection is a preview. **Save and use** applies it. Curve edits remain drafts until saved. Editing a built-in profile creates a custom draft. Narrow layouts can scroll; the graph and table stack when the available width is limited.

## Color and shape

Light and dark themes use neutral window and editor surfaces with Lenovo red for primary actions and curve traces. Selected profiles use a low-intensity red tint with a distinct text color. Controls use 6 px corner radii and palette-aware borders. Focus remains visible in both themes.

| Token | Value |
| --- | --- |
| Primary | `#b51225` |
| Primary hover | `#941020` |
| Light curve | `#c8102e` |
| Dark curve | `#ff8c9b` |
| Light selected background / text | `#f6e3e7` / `#851126` |
| Dark selected background / text | `#51343e` / `#ffe8ed` |
| Light window / editor | `#f4f5f6` / `#ffffff` |
| Dark window / editor | `#202124` / `#292b2f` |

## Typography and controls

The application uses the desktop system font. Page titles use semibold weight; controls, table values, and labels retain native Qt sizing. Profile rows use larger labels and 26 px icons. Standard dialogs, menus, and keyboard navigation follow Qt Widgets behavior.

The [surface brief](docs/GUI-SURFACE-BRIEF.md) records interaction rules. The [validation notes](docs/GUI-VALIDATION.md) list checks and known gaps.
