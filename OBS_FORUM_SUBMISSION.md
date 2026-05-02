# OBS Forum Submission Draft: Dynamic Next Stream

## Resource Title

Dynamic Next Stream

## Version

1.0.1

## Category

OBS Studio Plugins

## Tags

stream schedule, next stream, text source, countdown, weekly schedule

## Short Tagline

Write your next upcoming streams into an OBS text source.

## Supported Bit Versions

64-bit

## Supported Platforms

Windows

## Minimum OBS Studio Version

30.0.0

## Source Code URL

https://github.com/KSTYER1/dynamic-next-stream

## Download URL

TODO: publish or upload the 1.0.1 release package

## Overview

Dynamic Next Stream is an unofficial third-party dock plugin for OBS Studio. It
maintains a weekly stream schedule and writes the next upcoming stream entries
to a selected text source.

The plugin is useful for streamers who want a "next stream" overlay that updates
automatically from a configured schedule.

## Features

- Dock UI for stream days, times, categories, and output format.
- Odd/even calendar week schedules for alternating weekly programs.
- Live preview with one-click copy.
- Automatic updates on a configurable interval.
- Direct output to OBS text sources.
- Optional file output.
- English and German UI text.

## Installation

Download the Windows x64 release archive and extract or copy its contents into
your OBS Studio installation directory.

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
3. Configure stream days and times.
4. Adjust the output format.
5. Let the plugin update the selected text source automatically.

## License

GPL-2.0-or-later.

## Disclaimer

Dynamic Next Stream is an unofficial third-party plugin and is not affiliated
with or endorsed by the OBS Project.

AI-assisted tools were used during development and release preparation. The
maintainer is responsible for reviewing, testing, and publishing the released
plugin.

## Pre-Submit Checklist

- [x] Public GitHub repository exists.
- [x] README is visible on GitHub after the next documentation push.
- [x] GPL license is visible on GitHub.
- [x] Source Code URL field points to the repository.
- [ ] Release ZIP is attached to GitHub Releases or uploaded to the forum.
- [ ] At least one screenshot/GIF is added to the resource description.
- [x] Description is in English.
- [x] No OBS logo is used as resource icon or marketing artwork.
