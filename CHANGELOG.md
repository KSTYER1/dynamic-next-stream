# Changelog

## 1.0.7

- Recover settings from a valid backup even when the main file is missing. If
  existing settings and backup cannot be read, show a warning and prevent
  automatic saves from replacing them. Preserve the valid backup on the first
  recovery save.
- Update replacement text sources and scene collections even when the selected
  source name and schedule text remain unchanged.
- Use calendar days across daylight-saving changes. Skip nonexistent local start
  times and prefer standard time when a start time occurs twice.
- Pause both TXT exports and show a warning when their destinations refer to the
  same file; resume automatically after choosing separate files.
- Run packaging from the plugin directory regardless of the caller's directory,
  restoring that directory when a build fails.

## 1.0.6

- Use the current week for Discord Sunday/Monday starts.
- Allow a manual plan-week offset.
- Count actual stream entries instead of calendar days in Discord output.
