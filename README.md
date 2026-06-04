# Dynamic Next Stream

Dynamic Next Stream is a third-party native dock plugin for OBS Studio. It
maintains a weekly stream schedule and writes the next upcoming stream entries
to a selected text source.

The plugin is designed for streamers who want an always-current "next stream"
text overlay without editing the text source manually before every show.

## Features

- Dock UI for configuring stream days, times, categories, and output format.
- Odd/even calendar week schedules for alternating weekly programs.
- Live preview with one-click copy.
- Automatic updates on a configurable interval.
- Direct output to OBS text sources.
- Optional free-format file output for use outside OBS.
- Optional configurable Discord-ready text file for Streamer.bot or chat posting.
- German and English UI text.

## Requirements

- OBS Studio 30.x, 31.x, or 32.x
- Windows 64-bit release build
- Qt 6, provided by OBS Studio

## Installation

Download the release archive and extract or copy its contents into your OBS
Studio installation directory.

The final layout should include:

```text
obs-plugins/64bit/dynamic-next-stream.dll
data/obs-plugins/dynamic-next-stream/locale/en-US.ini
data/obs-plugins/dynamic-next-stream/locale/de-DE.ini
```

Restart OBS after installation. The dock appears under:

```text
View -> Docks -> Next Stream
```

## Basic Usage

1. Open the dock from `View -> Docks -> Next Stream`.
2. Choose the target text source.
3. Configure the days and times when you stream.
4. Adjust the output format and preview.
5. Let the plugin update the selected text source automatically.

The file tab can also write a Discord plan. Sunday and Monday week starts use
the current week for that weekday; the plan week control can manually advance
to later weeks. The Discord count is the number of real stream entries to list,
so stream-free days are skipped.

## Building from Source

Requires CMake 3.28 or newer, Qt 6, OBS development headers, and a supported
compiler toolchain.

```bash
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo
```

## Source Code

Source code is available at:

```text
https://github.com/KSTYER1/dynamic-next-stream
```

## Version History

### 1.0.6

- Changed Discord TXT Sunday/Monday starts to use the current week for the
  selected weekday instead of rolling forward automatically.
- Added a manual plan week offset for switching to later planned weeks.
- Changed the Discord TXT count from calendar days to real stream entries, so
  stream-free days are skipped.

### 1.0.5

- Fixed Discord TXT week start dates so Sunday/Monday start modes roll forward to the next matching plan start instead of staying on the previous week.

### 1.0.4

- Changed Discord TXT day count to a free numeric field.
- Updated Discord TXT formatting to Markdown-style weekly entries with a Sunday week divider.

### 1.0.3

- Added Discord TXT options for week start, past streams, day count, categories, and layout.

### 1.0.2

- Reworked the Plan tab into two-row weekday entries with full-width category fields.
- Added separate free TXT and Discord weekly TXT output paths.

### 1.0.1

- Current Windows x64 release.
- Split from the older combined time/date/countdown package.
- Added a dedicated dock for weekly stream planning.

## License

Dynamic Next Stream is licensed under GPL-2.0-or-later.

## Disclaimer

Dynamic Next Stream is an unofficial third-party plugin and is not affiliated
with or endorsed by the OBS Project.

AI-assisted tools were used during development and release preparation. The
maintainer is responsible for reviewing, testing, and publishing the released
plugin.
