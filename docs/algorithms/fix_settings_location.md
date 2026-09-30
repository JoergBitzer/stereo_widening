# settings.json in the preset folder (1.0.4, rebuilt)

Until now the per-user settings (`settings.json`: GUI size, theme, meter ballistics) were in
their own folder (`<userApplicationDataDirectory>/StereoWidener/`, e.g. `~/.config/StereoWidener/`),
separate from the presets (`~/.config/Jade_Hochschule/StereoWidener/`). Now both are in the preset
folder, as in the AdvancedAudioTemplate: one folder to find, back up or delete.

- `GlobalSettings::getSettingsFile()` takes the folder from `PresetHandler::getUserPresetsFolder()`
  (now `static`), so the path logic exists only once (including the macOS preset location).
- The name stays `settings.json` -- not `.xml`, because the preset handler loads every `.xml` file
  in that folder as a preset.
- Migration: if the new file does not exist but the old one does, it is moved on the next start
  (settings kept) and the empty old folder is removed.

Tested (temporary home folder, Release build): old file with day theme and scale 1.5 is moved,
values kept, old folder removed, 21 presets deployed; fresh install creates the file in the preset
folder; pluginval --strictness-level 10: 2/2 SUCCESS. README, plugin README and manual updated.
Version stays 1.0.4 (user decision: the 1.0.4 release is rebuilt with this change).
