/*
Dynamic Next Stream — OBS Dock for managing a weekly stream schedule
Copyright (C) 2026 K_STYER, GPLv2 or later
*/

#include <obs-module.h>
#include <plugin-support.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Dock for managing a weekly stream schedule and writing the next streams into a chosen text source.";
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return "Dynamic Next Stream";
}

MODULE_EXPORT const char *obs_module_author(void)
{
	return "K_STYER";
}

extern void next_stream_dock_register(void);
extern void next_stream_dock_unregister(void);

bool obs_module_load(void)
{
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

MODULE_EXPORT void obs_module_post_load(void)
{
	next_stream_dock_register();
}

void obs_module_unload(void)
{
	next_stream_dock_unregister();
	obs_log(LOG_INFO, "plugin unloaded");
}
