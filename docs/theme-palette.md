# Theme palettes

`danket::ui::ThemeManager` owns the colours used by the Qt client. It resolves
three modes — `System`, `Light` and `Dark` — against built-in defaults plus an
optional palette loaded from a JSON file. Every change emits `paletteChanged()`,
so QML bindings repaint immediately without restarting the application.

## Using the UI

- **View → Appearance → Light / Dark / System** switches the mode. `System`
  follows `QStyleHints::colorScheme()` and tracks live OS theme changes.
- **View → Palette → Load JSON…** loads an external palette.
- **View → Palette → Reset** returns to the built-in palette.
- `danket-sub --palette <file>` loads a palette during startup.

## JSON schema

The root must be an object. `name` is optional. Colour keys can appear at the
top level (applied to both modes) and inside `light` / `dark` sections (applied
to that mode only). Mode sections override shared keys.

```json
{
  "name": "Copper Night",
  "accent": "#7ad0c0",
  "light": {
    "windowBg": "#fbf3e6",
    "surfaceBg": "#fffaf0"
  },
  "dark": {
    "windowBg": "#101820",
    "surfaceBg": "#1b2a30",
    "waveformBg": "#0d1b20",
    "cursorColor": "#ff9f43"
  }
}
```

Colour values are strings accepted by `QColor::fromString` (`#RGB`, `#RRGGBB`,
`#AARRGGBB`, and Qt colour names).

## Semantic keys

| Key | Purpose |
| --- | --- |
| `windowBg` | Window / workspace background |
| `surfaceBg` | Panels, video and grid backgrounds |
| `surfaceAltBg` | Secondary surfaces and toolbars |
| `borderColor` | Panel and splitter borders |
| `textPrimary` | Primary text |
| `textSecondary` | Secondary text, timecodes |
| `accent` | Highlights, waveform bars |
| `selectionBg` | Selected grid row |
| `gridZebraBg` | Alternate grid row |
| `waveformBg` | Waveform viewport |
| `waveformSelection` | Waveform selection range |
| `cursorColor` | Playhead / cursor |
| `danger` | Error messages |

Inspect `ThemeManager::paletteKeys()` for the authoritative list.

## Fallback rules

Palette loading never leaves the application with an unusable theme:

- A missing file, malformed JSON, a non-object root, or a `light`/`dark`
  section that is not an object makes `loadPalette()` return `false`; the
  current palette stays active and `lastError` describes the problem.
- Unknown keys are ignored, so palettes can target newer builds.
- A colour value that does not parse is ignored per key; the built-in colour
  for that key remains.
- Sections and keys may be partial. Omitted keys fall back to the built-in
  light or dark palette.

The `danket_theme_tests` and `danket_ui_tests` targets cover mode switching,
partial palettes, unknown keys, invalid colours and malformed files.
