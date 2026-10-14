// Copyright (C) 2026 K_STYER1. GPL-2.0-or-later.
// Real dock/Qt/libobs JSON; source registry and frontend boundary are simulated.
#include "next-stream-dock.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QTimeEdit>
#include <QTimer>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QLabel>
#include <QSpinBox>
#include <cstdio>
#include <cstdlib>
#include <functional>

struct obs_source {
	const char *name = "Schedule";
	QString text = "untouched";
	int updates = 0;
	const char *uuid = "uuid-a";
	const char *type = "text_gdiplus";
};
static obs_source a, b;
static obs_source *active = &a;
static QString configRoot;
static time_t frozenNow;
static NextStreamDock *registeredDock = nullptr;
static obs_frontend_event_cb frontendCallback = nullptr;
time_t review_now()
{
	return frozenNow;
}
extern "C" {
void next_stream_dock_register();
void next_stream_dock_unregister();
const char *obs_module_text(const char *key)
{
	if (!strcmp(key, "NextStreamNextStreamStatus"))
		return "%1 %2%3 %4";
	if (!strcmp(key, "NextStreamDiscordWeekHeader"))
		return "Week %1.%2";
	if (!strcmp(key, "NextStreamSettingsLoadError"))
		return "Cannot load %1. Saving disabled.";
	if (!strncmp(key, "NextStreamDaySingular", 21) || !strcmp(key, "NextStreamDayPlural") ||
	    !strcmp(key, "NextStreamHourSingular") || !strcmp(key, "NextStreamHourPlural") ||
	    !strcmp(key, "NextStreamMinuteSingular") || !strcmp(key, "NextStreamMinutePlural"))
		return "%1 units";
	return key;
}
obs_module_t *obs_current_module()
{
	return nullptr;
}
char *obs_module_get_config_path(obs_module_t *, const char *file)
{
	return bstrdup((configRoot + "/" + file).toUtf8().constData());
}
void obs_log(int, const char *, ...) {}
void obs_enum_sources(bool (*cb)(void *, obs_source_t *), void *data)
{
	if (active)
		cb(data, active);
}
const char *obs_source_get_unversioned_id(const obs_source_t *s)
{
	return s->type;
}
const char *obs_source_get_uuid(const obs_source_t *s)
{
	return s->uuid;
}
const char *obs_source_get_name(const obs_source_t *s)
{
	return s->name;
}
obs_source_t *obs_get_source_by_name(const char *name)
{
	return active && QString::fromUtf8(name) == active->name ? active : nullptr;
}
void obs_source_release(obs_source_t *) {}
void obs_source_update(obs_source_t *s, obs_data_t *settings)
{
	s->text = QString::fromUtf8(obs_data_get_string(settings, "text"));
	++s->updates;
}
bool obs_frontend_add_dock_by_id(const char *, const char *, void *widget)
{
	registeredDock = static_cast<NextStreamDock *>(widget);
	return true;
}
void obs_frontend_remove_dock(const char *)
{
	delete registeredDock;
	registeredDock = nullptr;
}
void obs_frontend_add_event_callback(obs_frontend_event_cb cb, void *)
{
	frontendCallback = cb;
}
void obs_frontend_remove_event_callback(obs_frontend_event_cb, void *)
{
	frontendCallback = nullptr;
}
}
static void require(bool ok, const char *message)
{
	if (!ok)
		throw message;
}
static void write(const QString &path, const QByteArray &bytes)
{
	QFile f(path);
	require(f.open(QIODevice::WriteOnly), "fixture write failed");
	f.write(bytes);
}
static QByteArray read(const QString &path)
{
	QFile f(path);
	require(f.open(QIODevice::ReadOnly), "fixture read failed");
	return f.readAll();
}
static time_t localDate(int year, int month, int day, int hour, int minute)
{
	tm t{};
	t.tm_year = year - 1900;
	t.tm_mon = month - 1;
	t.tm_mday = day;
	t.tm_hour = hour;
	t.tm_min = minute;
	t.tm_isdst = -1;
	return mktime(&t);
}
static void stop(NextStreamDock &dock)
{
	dock.m_update_timer_obj->stop();
	dock.m_preview_timer_obj->stop();
}
static void days(NextStreamDock &dock, int enabled)
{
	for (int i = 0; i < 7; ++i) {
		dock.m_day_enabled[i]->blockSignals(true);
		dock.m_day_enabled[i]->setChecked(i == enabled);
	}
	dock.m_time_fixed->blockSignals(true);
	dock.m_time_fixed->setTime(QTime(20, 0));
}

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	_putenv_s("TZ", "PST8PDT");
	_tzset();
	int checks = 0, failed = 0;
	auto test = [&](const char *name, const std::function<void()> &body) {
		QTemporaryDir temp;
		configRoot = temp.path();
		a = obs_source{};
		b = obs_source{};
		b.uuid = "uuid-b";
		active = &a;
		frozenNow = localDate(2026, 10, 14, 12, 0);
		try {
			body();
			++checks;
			std::printf("PASS %s\n", name);
		} catch (const char *error) {
			++failed;
			std::printf("FAIL %s: %s\n", name, error);
		}
	};
	test("control: initial target receives rendered text", [&] {
		write(configRoot + "/next-stream.json", R"({"target_source_name":"Schedule"})");
		NextStreamDock dock;
		stop(dock);
		require(a.updates == 1 && a.text != "untouched", "initial output absent");
	});
	test("same-name replacement source receives output without manual refresh", [&] {
		write(configRoot + "/next-stream.json", R"({"target_source_name":"Schedule"})");
		NextStreamDock dock;
		stop(dock);
		require(a.updates == 1, "initial update absent");
		active = &b;
		dock.on_update_timer();
		require(b.updates == 1 && b.text == a.text, "replacement did not receive output");
		dock.on_update_timer();
		require(b.updates == 1, "unchanged source updated repeatedly");
	});
	test("missing main config recovers valid backup", [&] {
		write(configRoot + "/next-stream.json.bak", R"({"prefix":"RESTORE-ME"})");
		NextStreamDock dock;
		stop(dock);
		require(dock.m_prefix->text() == "RESTORE-ME", "backup was ignored");
		dock.m_prefix->setText("edit-one");
		dock.m_prefix->setText("edit-two");
		require(read(configRoot + "/next-stream.json").contains("edit-two"),
			"recovered settings cannot be saved");
	});
	test("corrupt main and backup remain byte-identical after edits and shutdown", [&] {
		const QByteArray corrupt = "{\"prefix\":\"RECOVERABLE\", broken";
		write(configRoot + "/next-stream.json", corrupt);
		write(configRoot + "/next-stream.json.bak", corrupt + "backup");
		{
			NextStreamDock dock;
			stop(dock);
			dock.m_prefix->setText("first-edit");
			require(!dock.m_settings_warning->isHidden(), "load-error warning not visible");
		}
		require(read(configRoot + "/next-stream.json") == corrupt, "corrupt main overwritten");
		{
			NextStreamDock dock;
			stop(dock);
			dock.m_prefix->setText("later-edit");
		}
		require(read(configRoot + "/next-stream.json") == corrupt, "original main overwritten on restart");
		require(read(configRoot + "/next-stream.json.bak") == corrupt + "backup",
			"original backup overwritten");
	});
	test("control: ordinary weekly dates are distinct", [&] {
		NextStreamDock dock;
		stop(dock);
		days(dock, 6);
		QVector<StreamEntry> out;
		dock.compute_next_streams(2, out);
		require(out.size() == 2 && out[0].start_time != out[1].start_time, "ordinary dates duplicated");
	});
	test("fall DST returns consecutive Sundays once each", [&] {
		frozenNow = localDate(2026, 11, 1, 0, 30);
		NextStreamDock dock;
		stop(dock);
		days(dock, 6);
		QVector<StreamEntry> out;
		dock.compute_next_streams(2, out);
		require(out.size() == 2 && out[0].day == 1 && out[1].day == 8 && out[1].month == 11,
			"Sunday duplicated or skipped");
	});
	test("spring DST includes tomorrow Sunday", [&] {
		frozenNow = localDate(2026, 3, 7, 23, 30);
		NextStreamDock dock;
		stop(dock);
		days(dock, 6);
		QVector<StreamEntry> out;
		dock.compute_next_streams(1, out);
		require(out.size() == 1 && out[0].day == 8 && out[0].month == 3, "upcoming Sunday skipped");
		require(out[0].day_name == "NextStreamTomorrow", "tomorrow label wrong");
	});
	test("conflicting TXT destinations preserve existing bytes until resolved", [&] {
		NextStreamDock dock;
		stop(dock);
		const auto p = configRoot + "/shared.txt";
		write(p, "KEEP");
		dock.m_file_path->blockSignals(true);
		dock.m_discord_file_path->blockSignals(true);
		dock.m_file_path->setText(p);
		dock.m_discord_file_path->setText(p);
		dock.on_update_timer();
		require(read(p) == "KEEP", "collision overwrote file");
		require(!dock.m_export_warning->isHidden(), "conflict warning hidden");
		dock.m_discord_file_path->setText(configRoot + "/discord.txt");
		dock.on_update_timer();
		require(read(p) != "KEEP" && read(p) != read(configRoot + "/discord.txt"),
			"outputs did not resume separately");
		require(dock.m_export_warning->isHidden(), "conflict warning did not clear");
	});
	test("corrupt main recovers a valid backup without modifying originals on load", [&] {
		const auto p = configRoot + "/next-stream.json";
		write(p, "{broken");
		write(p + ".bak", R"({"prefix":"BACKUP"})");
		NextStreamDock dock;
		stop(dock);
		require(dock.m_prefix->text() == "BACKUP", "valid backup not loaded");
		require(read(p) == "{broken" && read(p + ".bak").contains("BACKUP"), "load mutated originals");
	});
	test("new settings save and reload normally", [&] {
		{
			NextStreamDock dock;
			stop(dock);
			dock.m_prefix->setText("Persist me");
		}
		NextStreamDock dock;
		stop(dock);
		require(dock.m_prefix->text() == "Persist me", "save/reload failed");
	});
	test("same UUID after collection refresh is updated", [&] {
		write(configRoot + "/next-stream.json", R"({"target_source_name":"Schedule"})");
		NextStreamDock dock;
		stop(dock);
		b.uuid = a.uuid;
		active = &b;
		dock.on_refresh_sources_clicked();
		dock.on_update_timer();
		require(b.updates == 1 && b.text == a.text, "refresh retained stale identity cache");
	});
	test("missing then recreated source receives output", [&] {
		write(configRoot + "/next-stream.json", R"({"target_source_name":"Schedule"})");
		NextStreamDock dock;
		stop(dock);
		active = nullptr;
		dock.on_update_timer();
		b.uuid = a.uuid;
		active = &b;
		dock.on_update_timer();
		require(b.updates == 1, "recreated source skipped");
	});
	test("same-name non-text source is not modified", [&] {
		write(configRoot + "/next-stream.json", R"({"target_source_name":"Schedule"})");
		b.type = "image_source";
		active = &b;
		NextStreamDock dock;
		stop(dock);
		require(b.updates == 0, "non-text source modified");
	});
	test("spring missing local time is skipped rather than silently shifted", [&] {
		frozenNow = localDate(2026, 3, 7, 12, 0);
		NextStreamDock dock;
		stop(dock);
		days(dock, 6);
		dock.m_time_fixed->setTime(QTime(2, 30));
		QVector<StreamEntry> out;
		dock.compute_next_streams(1, out);
		require(out.size() == 1 && out[0].day == 15, "nonexistent local time emitted");
	});
	test("ambiguous fall local time uses standard-time occurrence", [&] {
		frozenNow = localDate(2026, 11, 1, 0, 30);
		NextStreamDock dock;
		stop(dock);
		days(dock, 6);
		dock.m_time_fixed->setTime(QTime(1, 30));
		QVector<StreamEntry> out;
		dock.compute_next_streams(1, out);
		require(out.size() == 1 && out[0].day == 1, "ambiguous day missing");
		tm resolved{};
		localtime_s(&resolved, &out[0].start_time);
		require(resolved.tm_isdst == 0, "ambiguous time did not use standard time");
	});
	test("relative and case aliases are rejected as conflicting TXT paths", [&] {
		NextStreamDock dock;
		stop(dock);
		const auto p = configRoot + "/Alias.txt";
		write(p, "UNCHANGED");
		dock.m_file_path->blockSignals(true);
		dock.m_discord_file_path->blockSignals(true);
		dock.m_file_path->setText(p);
		dock.m_discord_file_path->setText(configRoot + "/./alias.TXT");
		dock.on_update_timer();
		require(read(p) == "UNCHANGED", "path aliases bypassed conflict guard");
	});
	test("unreadable backup without main never creates default main", [&] {
		const auto p = configRoot + "/next-stream.json";
		write(p + ".bak", "{bad backup");
		{
			NextStreamDock dock;
			stop(dock);
			dock.m_prefix->setText("unsaved edit");
		}
		require(!QFile::exists(p), "default main created over failed recovery");
		require(read(p + ".bak") == "{bad backup", "unreadable backup changed");
	});
	test("collection event refreshes a source with same persistent UUID", [&] {
		write(configRoot + "/next-stream.json", R"({"target_source_name":"Schedule"})");
		next_stream_dock_register();
		stop(*registeredDock);
		b.uuid = a.uuid;
		active = &b;
		frontendCallback(OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED, nullptr);
		QCoreApplication::processEvents();
		const bool updated = b.updates == 1 && b.text == a.text;
		next_stream_dock_unregister();
		next_stream_dock_unregister();
		require(updated, "collection event retained stale cache");
		require(!frontendCallback && !registeredDock, "dock cleanup incomplete");
	});
	test("first recovery save preserves the known-good backup", [&] {
		const auto p = configRoot + "/next-stream.json";
		write(p, "{broken");
		const QByteArray backup = R"({"prefix":"KNOWN-GOOD"})";
		write(p + ".bak", backup);
		{
			NextStreamDock dock;
			stop(dock);
		}
		require(read(p).contains("KNOWN-GOOD"), "recovered main not saved");
		require(read(p + ".bak") == backup, "known-good backup replaced by corrupt main");
	});
	test("nonexistent nested conflicting destinations are not created", [&] {
		NextStreamDock dock;
		stop(dock);
		dock.m_file_path->blockSignals(true);
		dock.m_discord_file_path->blockSignals(true);
		dock.m_file_path->setText(configRoot + "/new/nested/plan.txt");
		dock.m_discord_file_path->setText(configRoot + "/new/nested/./plan.txt");
		dock.on_update_timer();
		require(!QFile::exists(configRoot + "/new"), "conflict created export directories");
	});
	test("Discord schedule uses the same DST gap and fold rules", [&] {
		frozenNow = localDate(2026, 3, 7, 12, 0);
		NextStreamDock dock;
		stop(dock);
		days(dock, 6);
		dock.m_discord_week_start->setCurrentIndex(dock.m_discord_week_start->findData("today"));
		dock.m_discord_show_past->setChecked(false);
		dock.m_discord_streams->setValue(1);
		dock.m_time_fixed->setTime(QTime(2, 30));
		QVector<StreamEntry> out;
		QString header;
		dock.compute_discord_week_streams(out, header);
		require(out.size() == 1 && out[0].day == 15 && out[0].month == 3, "Discord emitted DST gap");
		frozenNow = localDate(2026, 11, 1, 0, 30);
		dock.m_time_fixed->setTime(QTime(1, 30));
		dock.compute_discord_week_streams(out, header);
		tm resolved{};
		require(out.size() == 1 && out[0].day == 1, "Discord fold missing");
		localtime_s(&resolved, &out[0].start_time);
		require(resolved.tm_isdst == 0, "Discord fold differs from OBS");
	});
	std::printf("Passed=%d Failed=%d\n", checks, failed);
	return failed ? 1 : 0;
}
