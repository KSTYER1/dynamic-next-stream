# Dynamic Next Stream

Dynamic Next Stream is a third-party native OBS Studio dock for displaying the
next planned stream time, live countdown information, and schedule text output.

It is intended for portable OBS setups that want a built-in weekly planner and
live preview without an external scheduling tool.

## Features

- Native OBS dock for planning the next stream.
- Weekly schedule editor with enabled state, automatic or manual time, and
  category per day.
- Live countdown to the next stream.
- Live preview of the final rendered text.
- Formatting controls for prefix, separator, suffix, day style, category
  display, and number of visible upcoming streams.
- Optional fixed stream times or even/odd calendar-week logic.
- Optional TXT export for use outside OBS.
- Automatic persistence in the OBS plugin config folder.

## Requirements

- OBS Studio 30.x, 31.x, or 32.x
- Windows x64 for the packaged release
- Qt 6, provided by OBS Studio

## Installation

### Windows

Download the release archive and extract or copy its contents into your OBS
Studio installation directory.

The final layout should include:

```text
obs-plugins/64bit/dynamic-next-stream.dll
data/obs-plugins/dynamic-next-stream/locale/en-US.ini
data/obs-plugins/dynamic-next-stream/locale/de-DE.ini
```

The packaged release also includes `INSTALL.bat`, which can copy the plugin
files into a selected OBS directory.

Restart OBS after installation. The dock appears under:

```text
View -> Docks -> Next Stream
```

## Basic Usage

1. Open `View -> Docks -> Next Stream`.
2. Configure the weekly plan for each day.
3. Adjust output formatting in the format tab.
4. Select the target text source if needed.
5. Use the live preview and countdown to verify the result.

Schedule data is stored automatically in:

```text
<OBS>\config\obs-studio\plugin_config\dynamic-next-stream\next-stream.json
```

## Version History

### 1.0.0

- Initial standalone release.
- Split the dock out from the earlier combined `dynamic-texts` package.
- Added a 3-tab layout for planning, formatting, and file export.
- Added live countdown and live preview.
- Added `QTimeEdit` based time fields and full tooltip coverage.

## License

Dynamic Next Stream is licensed under GPL-2.0-or-later.

## Disclaimer

Dynamic Next Stream is an unofficial third-party plugin and is not affiliated
with or endorsed by the OBS Project.
