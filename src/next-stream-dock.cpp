/*
Next Stream Dock — Qt UI for managing a weekly stream schedule and writing
the next streams into a chosen OBS text source.
*/

#include "next-stream-dock.hpp"
#include "iso-week.h"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>
#include <util/bmem.h>
#include <util/platform.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QScrollArea>
#include <QFileDialog>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QTabWidget>
#include <QTimeEdit>
#include <QTime>
#include <QSizePolicy>
#include <QFrame>
#include <QStyle>
#include <QApplication>
#include <QClipboard>
#include <QByteArray>

#include <ctime>
#include <vector>

static const char *DAY_FULL_KEYS[7] = {
	"NextStreamDayFullMonday", "NextStreamDayFullTuesday", "NextStreamDayFullWednesday",
	"NextStreamDayFullThursday", "NextStreamDayFullFriday", "NextStreamDayFullSaturday",
	"NextStreamDayFullSunday",
};

static const char *DAY_SHORT_KEYS[7] = {
	"NextStreamDayShortMonday", "NextStreamDayShortTuesday", "NextStreamDayShortWednesday",
	"NextStreamDayShortThursday", "NextStreamDayShortFriday", "NextStreamDayShortSaturday",
	"NextStreamDayShortSunday",
};

static QString module_text(const char *key)
{
	const char *text = obs_module_text(key);
	return QString::fromUtf8(text ? text : key);
}

static QString day_name_full(int index)
{
	return module_text(DAY_FULL_KEYS[index]);
}

static QString day_name_short(int index)
{
	return module_text(DAY_SHORT_KEYS[index]);
}

static QTime parse_hhmm(const QString &s)
{
	QTime t = QTime::fromString(s, "H:mm");
	if (!t.isValid())
		t = QTime::fromString(s, "HH:mm");
	if (!t.isValid())
		t = QTime(20, 0);
	return t;
}

static int days_since_wday(int current_wday, int target_wday)
{
	return (current_wday - target_wday + 7) % 7;
}

NextStreamDock::NextStreamDock(QWidget *parent) : QFrame(parent)
{
	setFrameShape(QFrame::NoFrame);
	build_ui();
	load_settings();
	apply_settings_to_widgets();
	refresh_source_combo();
	connect_signals();

	m_update_timer_obj = new QTimer(this);
	connect(m_update_timer_obj, &QTimer::timeout, this, &NextStreamDock::on_update_timer);
	m_update_timer_obj->start(m_update_interval->value() * 1000);

	m_preview_timer_obj = new QTimer(this);
	connect(m_preview_timer_obj, &QTimer::timeout, this, &NextStreamDock::on_preview_timer);
	m_preview_timer_obj->start(1000);

	on_use_even_odd_toggled(m_use_even_odd->isChecked());
	for (int i = 0; i < 7; i++)
		on_day_mode_changed(i);

	on_update_timer();
	on_preview_timer();
}

NextStreamDock::~NextStreamDock()
{
	shutdown();
}

void NextStreamDock::shutdown()
{
	if (m_shutdown)
		return;

	m_shutdown = true;
	if (m_update_timer_obj)
		m_update_timer_obj->stop();
	if (m_preview_timer_obj)
		m_preview_timer_obj->stop();
	if (!m_loading)
		save_settings();
}

// ----------------------------------------------------------------
// UI
// ----------------------------------------------------------------

void NextStreamDock::build_ui()
{
	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(8, 8, 8, 8);
	outer->setSpacing(8);

	// =========================================================
	// STICKY TOP — Status + Source + Preview
	// =========================================================
	auto *top_frame = new QFrame();
	top_frame->setFrameShape(QFrame::StyledPanel);
	top_frame->setStyleSheet("QFrame { background-color: rgba(255,255,255,8); border-radius: 6px; }");
	auto *top_lay = new QVBoxLayout(top_frame);
	top_lay->setContentsMargins(10, 8, 10, 10);
	top_lay->setSpacing(6);

	// Status row 1: clock
	m_status_clock = new QLabel();
	m_status_clock->setStyleSheet("color: #888; font-size: 10pt;");

	// Status row 2: next stream + countdown
	m_status_next = new QLabel();
	m_status_next->setStyleSheet("color: #f0c040; font-weight: 600; font-size: 11pt;");
	m_status_next->setWordWrap(true);

	top_lay->addWidget(m_status_clock);
	top_lay->addWidget(m_status_next);

	// Source row
	auto *src_row = new QHBoxLayout();
	auto *src_label = new QLabel(obs_module_text("NextStreamGroupSource") + QString(":"));
	src_label->setStyleSheet("color: #ccc;");
	m_target_source = new QComboBox();
	m_target_source->setToolTip(module_text("NextStreamTargetSourceTip"));
	m_target_source->setMinimumWidth(180);
	m_refresh_btn = new QPushButton();
	m_refresh_btn->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
	m_refresh_btn->setToolTip(obs_module_text("NextStreamRefresh"));
	m_refresh_btn->setFixedWidth(34);
	src_row->addWidget(src_label);
	src_row->addWidget(m_target_source, 1);
	src_row->addWidget(m_refresh_btn);
	top_lay->addLayout(src_row);

	// Preview header (label + copy button)
	auto *preview_header = new QHBoxLayout();
	preview_header->setContentsMargins(0, 0, 0, 0);
	preview_header->setSpacing(6);

	auto *preview_label = new QLabel(obs_module_text("NextStreamGroupPreview"));
	preview_label->setStyleSheet("color: #888; font-size: 9pt; margin-top: 4px;");

	m_preview_copy_btn = new QPushButton(obs_module_text("NextStreamCopyPreview"));
	m_preview_copy_btn->setToolTip(obs_module_text("NextStreamCopyPreviewTip"));
	m_preview_copy_btn->setMaximumHeight(22);

	preview_header->addWidget(preview_label, 1);
	preview_header->addWidget(m_preview_copy_btn);

	m_preview = new QPlainTextEdit();
	m_preview->setReadOnly(true);
	m_preview->setMaximumHeight(90);
	m_preview->setStyleSheet(
		"QPlainTextEdit { font-family: Consolas, 'Courier New', monospace; "
		"background: #181818; color: #e0e0e0; border: 1px solid #333; "
		"border-radius: 4px; padding: 4px; }");
	top_lay->addLayout(preview_header);
	top_lay->addWidget(m_preview);

	outer->addWidget(top_frame);

	// =========================================================
	// TABS
	// =========================================================
	m_tabs = new QTabWidget();
	m_tabs->setDocumentMode(true);

	// ---------- Tab 1: PLAN ----------
	{
		auto *tab = new QWidget();
		auto *tab_lay = new QVBoxLayout(tab);
		tab_lay->setContentsMargins(6, 6, 6, 6);
		tab_lay->setSpacing(4);

		auto *help = new QLabel(module_text("NextStreamPlanHelp"));
		help->setWordWrap(true);
		help->setStyleSheet("color: #888; font-size: 9pt; padding: 4px;");
		tab_lay->addWidget(help);

		auto *days_grid = new QGridLayout();
		days_grid->setHorizontalSpacing(8);
		days_grid->setVerticalSpacing(5);
		days_grid->setContentsMargins(4, 0, 4, 0);

		auto add_hdr = [&](int col, const QString &text) {
			auto *l = new QLabel(text);
			l->setStyleSheet("color: #888; font-size: 9pt; font-weight: 600;");
			days_grid->addWidget(l, 0, col);
		};
		add_hdr(0, module_text("NextStreamHeaderDay"));
		add_hdr(1, module_text("NextStreamHeaderMode"));
		add_hdr(2, module_text("NextStreamHeaderTime"));

		for (int i = 0; i < 7; i++) {
			int row = 1 + (i * 3);

			m_day_enabled[i] = new QCheckBox(day_name_full(i));
			m_day_enabled[i]->setMinimumWidth(120);
			m_day_enabled[i]->setToolTip(module_text("NextStreamDayEnabledTip"));

			m_day_time_choice[i] = new QComboBox();
			m_day_time_choice[i]->addItem(obs_module_text("NextStreamTimeAuto"), "auto");
			m_day_time_choice[i]->addItem(obs_module_text("NextStreamTimeManual"), "custom");
			m_day_time_choice[i]->setMinimumWidth(180);
			m_day_time_choice[i]->setToolTip(module_text("NextStreamTimeChoiceTip"));

			m_day_custom_time[i] = new QTimeEdit();
			m_day_custom_time[i]->setDisplayFormat("HH:mm");
			m_day_custom_time[i]->setTime(QTime(20, 0));
			m_day_custom_time[i]->setToolTip(module_text("NextStreamCustomTimeTip"));
			m_day_custom_time[i]->setMinimumWidth(86);

			m_day_category[i] = new QLineEdit();
			m_day_category[i]->setPlaceholderText(module_text("NextStreamCategoryPlaceholder"));
			m_day_category[i]->setToolTip(module_text("NextStreamCategoryTip"));

			auto *cat_label = new QLabel(obs_module_text("NextStreamCategory"));
			cat_label->setStyleSheet("color: #888; font-size: 9pt; font-weight: 600;");

			auto *line = new QFrame();
			line->setFrameShape(QFrame::HLine);
			line->setFrameShadow(QFrame::Sunken);
			line->setStyleSheet("color: #444;");

			days_grid->addWidget(m_day_enabled[i], row, 0);
			days_grid->addWidget(m_day_time_choice[i], row, 1);
			days_grid->addWidget(m_day_custom_time[i], row, 2);
			days_grid->addWidget(cat_label, row + 1, 0);
			days_grid->addWidget(m_day_category[i], row + 1, 1, 1, 2);
			days_grid->addWidget(line, row + 2, 0, 1, 3);
		}
		days_grid->setColumnStretch(0, 0);
		days_grid->setColumnStretch(1, 1);
		days_grid->setColumnStretch(2, 0);
		tab_lay->addLayout(days_grid);
		tab_lay->addStretch(1);

		m_tabs->addTab(tab, module_text("NextStreamTabPlan"));
		m_tabs->setTabIcon(0, style()->standardIcon(QStyle::SP_FileDialogContentsView));
	}

	// ---------- Tab 2: FORMAT ----------
	{
		auto *tab = new QWidget();
		auto *scroll = new QScrollArea();
		scroll->setWidgetResizable(true);
		scroll->setFrameShape(QFrame::NoFrame);
		auto *content = new QWidget();
		auto *vbox = new QVBoxLayout(content);
		vbox->setContentsMargins(6, 6, 6, 6);

		// Format-Sektion
		{
			auto *grp = new QGroupBox(obs_module_text("NextStreamGroupFormat"));
			auto *form = new QFormLayout(grp);
			form->setLabelAlignment(Qt::AlignRight);

			m_prefix = new QLineEdit();
			m_prefix->setPlaceholderText(module_text("NextStreamPrefixPlaceholder"));
			m_prefix->setToolTip(module_text("NextStreamPrefixTip"));

			m_separator = new QLineEdit();
			m_separator->setPlaceholderText(module_text("NextStreamDefaultSeparator"));
			m_separator->setToolTip(module_text("NextStreamSeparatorTip"));

			m_suffix = new QLineEdit();
			m_suffix->setPlaceholderText(module_text("NextStreamClockSuffix"));
			m_suffix->setToolTip(module_text("NextStreamSuffixTip"));

			m_day_format = new QComboBox();
			m_day_format->addItem(obs_module_text("NextStreamDayFormatFull"), "full");
			m_day_format->addItem(obs_module_text("NextStreamDayFormatShort"), "short");
			m_day_format->setToolTip(module_text("NextStreamDayFormatTip"));

			m_max_streams = new QSpinBox();
			m_max_streams->setRange(1, 7);
			m_max_streams->setToolTip(module_text("NextStreamMaxStreamsTip"));

			m_category_mode = new QComboBox();
			m_category_mode->addItem(obs_module_text("NextStreamCatShow"), "show");
			m_category_mode->addItem(obs_module_text("NextStreamCatRound"), "show_round");
			m_category_mode->addItem(obs_module_text("NextStreamCatSquare"), "show_square");
			m_category_mode->addItem(obs_module_text("NextStreamCatHide"), "hide");
			m_category_mode->setToolTip(module_text("NextStreamCategoryModeTip"));

			form->addRow(obs_module_text("NextStreamPrefix"), m_prefix);
			form->addRow(obs_module_text("NextStreamSeparator"), m_separator);
			form->addRow(obs_module_text("NextStreamSuffix"), m_suffix);
			form->addRow(obs_module_text("NextStreamDayFormat"), m_day_format);
			form->addRow(obs_module_text("NextStreamMaxStreams"), m_max_streams);
			form->addRow(obs_module_text("NextStreamCategoryMode"), m_category_mode);
			vbox->addWidget(grp);
		}

		// Zeiten-Sektion
		{
			auto *grp = new QGroupBox(obs_module_text("NextStreamGroupTimes"));
			m_times_form = new QFormLayout(grp);
			m_times_form->setLabelAlignment(Qt::AlignRight);

			m_use_even_odd = new QCheckBox(obs_module_text("NextStreamUseEvenOdd"));
			m_use_even_odd->setToolTip(module_text("NextStreamUseEvenOddTip"));

			m_time_fixed = new QTimeEdit(QTime(20, 0));
			m_time_fixed->setDisplayFormat("HH:mm");
			m_time_fixed->setToolTip(module_text("NextStreamTimeFixedTip"));

			m_time_odd = new QTimeEdit(QTime(20, 0));
			m_time_odd->setDisplayFormat("HH:mm");
			m_time_odd->setToolTip(module_text("NextStreamTimeOddTip"));

			m_time_even = new QTimeEdit(QTime(21, 0));
			m_time_even->setDisplayFormat("HH:mm");
			m_time_even->setToolTip(module_text("NextStreamTimeEvenTip"));

			m_update_interval = new QSpinBox();
			m_update_interval->setRange(10, 3600);
			m_update_interval->setSuffix(" s");
			m_update_interval->setToolTip(module_text("NextStreamUpdateIntervalTip"));

			m_times_form->addRow(m_use_even_odd);
			m_times_form->addRow(obs_module_text("NextStreamTimeFixed"), m_time_fixed);
			m_times_form->addRow(obs_module_text("NextStreamTimeOdd"), m_time_odd);
			m_times_form->addRow(obs_module_text("NextStreamTimeEven"), m_time_even);
			m_times_form->addRow(obs_module_text("NextStreamUpdateInterval"), m_update_interval);
			vbox->addWidget(grp);
		}

		vbox->addStretch(1);
		scroll->setWidget(content);

		auto *tab_lay = new QVBoxLayout(tab);
		tab_lay->setContentsMargins(0, 0, 0, 0);
		tab_lay->addWidget(scroll);
		m_tabs->addTab(tab, module_text("NextStreamTabFormat"));
		m_tabs->setTabIcon(1, style()->standardIcon(QStyle::SP_FileDialogDetailedView));
	}

	// ---------- Tab 3: DATEI ----------
	{
		auto *tab = new QWidget();
		auto *vbox = new QVBoxLayout(tab);
		vbox->setContentsMargins(6, 6, 6, 6);

		auto *grp = new QGroupBox(obs_module_text("NextStreamGroupFileFree"));
		auto *form = new QFormLayout(grp);
		form->setLabelAlignment(Qt::AlignRight);

		auto *path_row = new QHBoxLayout();
		m_file_path = new QLineEdit();
		m_file_path->setPlaceholderText(module_text("NextStreamFilePlaceholder"));
		m_file_path->setToolTip(module_text("NextStreamFileTip"));
		m_file_browse = new QPushButton();
		m_file_browse->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));
		m_file_browse->setMaximumWidth(34);
		m_file_browse->setToolTip(module_text("NextStreamFileBrowseTip"));
		path_row->addWidget(m_file_path, 1);
		path_row->addWidget(m_file_browse);

		m_file_use_emojis = new QCheckBox(obs_module_text("NextStreamFileEmojis"));
		m_file_use_emojis->setToolTip(module_text("NextStreamFileEmojisTip"));

		m_file_show_category = new QCheckBox(obs_module_text("NextStreamFileShowCat"));
		m_file_show_category->setToolTip(module_text("NextStreamFileShowCatTip"));

		m_file_max_streams = new QSpinBox();
		m_file_max_streams->setRange(1, 7);
		m_file_max_streams->setToolTip(module_text("NextStreamFileMaxStreamsTip"));

		m_file_separator = new QLineEdit();
		m_file_separator->setPlaceholderText(module_text("NextStreamDefaultSeparator"));
		m_file_separator->setToolTip(module_text("NextStreamFileSeparatorTip"));

		m_file_multiline = new QCheckBox(obs_module_text("NextStreamFileMultiline"));
		m_file_multiline->setToolTip(module_text("NextStreamFileMultilineTip"));

		form->addRow(obs_module_text("NextStreamFilePath"), path_row);
		form->addRow(QString(), m_file_use_emojis);
		form->addRow(QString(), m_file_show_category);
		form->addRow(obs_module_text("NextStreamFileMaxStreams"), m_file_max_streams);
		form->addRow(obs_module_text("NextStreamFileSeparator"), m_file_separator);
		form->addRow(QString(), m_file_multiline);
		vbox->addWidget(grp);

		auto *discord_grp = new QGroupBox(obs_module_text("NextStreamGroupFileDiscord"));
		auto *discord_form = new QFormLayout(discord_grp);
		discord_form->setLabelAlignment(Qt::AlignRight);

		auto *discord_path_row = new QHBoxLayout();
		m_discord_file_path = new QLineEdit();
		m_discord_file_path->setPlaceholderText(module_text("NextStreamDiscordFilePlaceholder"));
		m_discord_file_path->setToolTip(module_text("NextStreamDiscordFileTip"));
		m_discord_file_browse = new QPushButton();
		m_discord_file_browse->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));
		m_discord_file_browse->setMaximumWidth(34);
		m_discord_file_browse->setToolTip(module_text("NextStreamDiscordBrowseTip"));
		discord_path_row->addWidget(m_discord_file_path, 1);
		discord_path_row->addWidget(m_discord_file_browse);

		auto *discord_hint = new QLabel(module_text("NextStreamDiscordDescription"));
		discord_hint->setWordWrap(true);
		discord_hint->setStyleSheet("color: #888; font-size: 9pt; padding: 4px;");

		m_discord_week_start = new QComboBox();
		m_discord_week_start->addItem(obs_module_text("NextStreamDiscordWeekStartSunday"), "sunday");
		m_discord_week_start->addItem(obs_module_text("NextStreamDiscordWeekStartMonday"), "monday");
		m_discord_week_start->addItem(obs_module_text("NextStreamDiscordWeekStartToday"), "today");

		m_discord_week_offset = new QSpinBox();
		m_discord_week_offset->setRange(0, 8);
		m_discord_week_offset->setPrefix("+");
		m_discord_week_offset->setSuffix(" " + module_text("NextStreamWeeksSuffix"));
		m_discord_week_offset->setToolTip(module_text("NextStreamDiscordWeekOffsetTip"));

		m_discord_show_past = new QCheckBox(obs_module_text("NextStreamDiscordShowPast"));

		m_discord_streams = new QSpinBox();
		m_discord_streams->setRange(1, 31);
		m_discord_streams->setSuffix(" " + module_text("NextStreamStreamsSuffix"));
		m_discord_streams->setToolTip(module_text("NextStreamDiscordStreamsTip"));

		m_discord_show_category = new QCheckBox(obs_module_text("NextStreamDiscordShowCategory"));

		m_discord_layout = new QComboBox();
		m_discord_layout->addItem(obs_module_text("NextStreamDiscordLayoutSpacious"), "spacious");
		m_discord_layout->addItem(obs_module_text("NextStreamDiscordLayoutCompact"), "compact");

		discord_form->addRow(obs_module_text("NextStreamDiscordFilePath"), discord_path_row);
		discord_form->addRow(obs_module_text("NextStreamDiscordWeekStart"), m_discord_week_start);
		discord_form->addRow(obs_module_text("NextStreamDiscordWeekOffset"), m_discord_week_offset);
		discord_form->addRow(QString(), m_discord_show_past);
		discord_form->addRow(obs_module_text("NextStreamDiscordStreams"), m_discord_streams);
		discord_form->addRow(QString(), m_discord_show_category);
		discord_form->addRow(obs_module_text("NextStreamDiscordLayout"), m_discord_layout);
		discord_form->addRow(QString(), discord_hint);
		vbox->addWidget(discord_grp);
		vbox->addStretch(1);
		m_tabs->addTab(tab, module_text("NextStreamTabFile"));
		m_tabs->setTabIcon(2, style()->standardIcon(QStyle::SP_DirIcon));
	}

	outer->addWidget(m_tabs, 1);

	// Opaque popup background for all combo boxes (fixes transparent dropdown
	// inheritance from the OBS dark theme).
	const QString comboPopupQss =
		"QComboBox QAbstractItemView {"
		"  background-color: #2b2b2b;"
		"  color: #e0e0e0;"
		"  selection-background-color: #3b6ea5;"
		"  border: 1px solid #555;"
		"}";
	QList<QComboBox *> combos = {
		m_target_source, m_day_format, m_category_mode,
		m_day_time_choice[0], m_day_time_choice[1], m_day_time_choice[2],
		m_day_time_choice[3], m_day_time_choice[4], m_day_time_choice[5],
		m_day_time_choice[6], m_discord_week_start, m_discord_layout,
	};
	for (QComboBox *c : combos)
		c->setStyleSheet(c->styleSheet() + comboPopupQss);
}

void NextStreamDock::connect_signals()
{
	auto onChange = [this]() {
		if (!m_loading && !m_shutdown) {
			save_settings();
			on_update_timer();
			on_preview_timer();
		}
	};

	connect(m_prefix, &QLineEdit::textChanged, this, onChange);
	connect(m_separator, &QLineEdit::textChanged, this, onChange);
	connect(m_suffix, &QLineEdit::textChanged, this, onChange);
	connect(m_file_path, &QLineEdit::textChanged, this, onChange);
	connect(m_discord_file_path, &QLineEdit::textChanged, this, onChange);
	connect(m_file_separator, &QLineEdit::textChanged, this, onChange);

	connect(m_target_source, &QComboBox::currentIndexChanged, this, onChange);
	connect(m_day_format, &QComboBox::currentIndexChanged, this, onChange);
	connect(m_category_mode, &QComboBox::currentIndexChanged, this, onChange);
	connect(m_discord_week_start, &QComboBox::currentIndexChanged, this, onChange);
	connect(m_discord_week_offset, &QSpinBox::valueChanged, this, onChange);
	connect(m_discord_streams, &QSpinBox::valueChanged, this, onChange);
	connect(m_discord_layout, &QComboBox::currentIndexChanged, this, onChange);

	connect(m_max_streams, &QSpinBox::valueChanged, this, onChange);
	connect(m_file_max_streams, &QSpinBox::valueChanged, this, onChange);
	connect(m_update_interval, &QSpinBox::valueChanged, this, [this]() {
		if (m_loading)
			return;
		save_settings();
		if (m_update_timer_obj)
			m_update_timer_obj->setInterval(m_update_interval->value() * 1000);
	});

	connect(m_time_fixed, &QTimeEdit::timeChanged, this, onChange);
	connect(m_time_odd, &QTimeEdit::timeChanged, this, onChange);
	connect(m_time_even, &QTimeEdit::timeChanged, this, onChange);

	connect(m_use_even_odd, &QCheckBox::toggled, this, &NextStreamDock::on_use_even_odd_toggled);
	connect(m_use_even_odd, &QCheckBox::toggled, this, onChange);
	connect(m_file_use_emojis, &QCheckBox::toggled, this, onChange);
	connect(m_file_show_category, &QCheckBox::toggled, this, onChange);
	connect(m_file_multiline, &QCheckBox::toggled, this, onChange);
	connect(m_discord_show_past, &QCheckBox::toggled, this, onChange);
	connect(m_discord_show_category, &QCheckBox::toggled, this, onChange);

	for (int i = 0; i < 7; i++) {
		connect(m_day_enabled[i], &QCheckBox::toggled, this, onChange);
		connect(m_day_time_choice[i], &QComboBox::currentIndexChanged, this, onChange);
		connect(m_day_time_choice[i], &QComboBox::currentIndexChanged, this, [this, i]() {
			on_day_mode_changed(i);
		});
		connect(m_day_custom_time[i], &QTimeEdit::timeChanged, this, onChange);
		connect(m_day_category[i], &QLineEdit::textChanged, this, onChange);
	}

	connect(m_refresh_btn, &QPushButton::clicked, this, &NextStreamDock::on_refresh_sources_clicked);
	connect(m_file_browse, &QPushButton::clicked, this, &NextStreamDock::on_browse_file_clicked);
	connect(m_discord_file_browse, &QPushButton::clicked, this, &NextStreamDock::on_browse_discord_file_clicked);

	connect(m_preview_copy_btn, &QPushButton::clicked, this, [this]() {
		QApplication::clipboard()->setText(m_preview->toPlainText());
		const QString original = obs_module_text("NextStreamCopyPreview");
		const QString done     = obs_module_text("NextStreamCopyPreviewDone");
		m_preview_copy_btn->setText(done);
		m_preview_copy_btn->setEnabled(false);
		QTimer::singleShot(1200, m_preview_copy_btn, [this, original]() {
			if (m_preview_copy_btn) {
				m_preview_copy_btn->setText(original);
				m_preview_copy_btn->setEnabled(true);
			}
		});
	});
}

void NextStreamDock::on_use_even_odd_toggled(bool checked)
{
	auto hide_row = [&](QWidget *field, bool visible) {
		if (!field || !m_times_form)
			return;
		field->setVisible(visible);
		QWidget *label = m_times_form->labelForField(field);
		if (label)
			label->setVisible(visible);
	};
	hide_row(m_time_fixed, !checked);
	hide_row(m_time_odd, checked);
	hide_row(m_time_even, checked);
}

void NextStreamDock::on_day_mode_changed(int day_index)
{
	if (day_index < 0 || day_index >= 7)
		return;
	bool manual = (m_day_time_choice[day_index]->currentData().toString() == "custom");
	m_day_custom_time[day_index]->setEnabled(manual);
	m_day_custom_time[day_index]->setStyleSheet(manual ? "" : "color: #555;");
}

// ----------------------------------------------------------------
// Settings (JSON)
// ----------------------------------------------------------------

QString NextStreamDock::config_file_path() const
{
	char *cfg = obs_module_get_config_path(obs_current_module(), "next-stream.json");
	QString p = QString::fromUtf8(cfg ? cfg : "");
	bfree(cfg);
	return p;
}

QString NextStreamDock::selected_target_source_name() const
{
	return m_target_source ? m_target_source->currentData().toString() : QString();
}

void NextStreamDock::load_settings()
{
	m_loading = true;

	QString path = config_file_path();
	obs_data_t *d = nullptr;
	if (!path.isEmpty() && QFile::exists(path)) {
		QByteArray path_utf8 = path.toUtf8();
		d = obs_data_create_from_json_file_safe(path_utf8.constData(), "bak");
		if (!d)
			obs_log(LOG_WARNING, "next-stream: failed to load settings from %s", path_utf8.constData());
	}
	if (!d)
		d = obs_data_create();

	obs_data_set_default_string(d, "target_source_name", "");
	obs_data_set_default_string(d, "prefix", obs_module_text("NextStreamDefaultPrefix"));
	obs_data_set_default_string(d, "separator", obs_module_text("NextStreamDefaultSeparator"));
	obs_data_set_default_string(d, "suffix", obs_module_text("NextStreamClockSuffix"));
	obs_data_set_default_string(d, "day_format", "full");
	obs_data_set_default_int(d, "max_streams_displayed", 1);
	obs_data_set_default_string(d, "category_display_mode", "show");
	obs_data_set_default_bool(d, "use_even_odd_schedule", false);
	obs_data_set_default_string(d, "stream_time_fixed", "20:00");
	obs_data_set_default_string(d, "stream_time_odd", "20:00");
	obs_data_set_default_string(d, "stream_time_even", "21:00");
	obs_data_set_default_int(d, "update_interval_sec", 30);
	obs_data_set_default_string(d, "file_path", "");
	obs_data_set_default_string(d, "discord_file_path", "");
	obs_data_set_default_string(d, "discord_week_start", "sunday");
	obs_data_set_default_int(d, "discord_week_offset", 0);
	obs_data_set_default_bool(d, "discord_show_past", true);
	obs_data_set_default_int(d, "discord_streams_displayed", 3);
	obs_data_set_default_bool(d, "discord_show_category", true);
	obs_data_set_default_string(d, "discord_layout", "spacious");
	obs_data_set_default_bool(d, "file_use_emojis", true);
	obs_data_set_default_bool(d, "file_show_category", true);
	obs_data_set_default_int(d, "file_max_streams_displayed", 1);
	obs_data_set_default_string(d, "file_separator", obs_module_text("NextStreamDefaultSeparator"));
	obs_data_set_default_bool(d, "file_multiline", false);

	m_pending_load = d;
}

void NextStreamDock::apply_settings_to_widgets()
{
	obs_data_t *d = m_pending_load;
	m_pending_load = nullptr;
	if (!d) {
		m_loading = false;
		return;
	}

	auto get_str = [&](const char *key) {
		return QString::fromUtf8(obs_data_get_string(d, key));
	};
	auto set_combo_data = [&](QComboBox *c, const QString &val) {
		int idx = c->findData(val);
		if (idx >= 0)
			c->setCurrentIndex(idx);
	};
	QString target = get_str("target_source_name");
	int t_idx = m_target_source->findData(target);
	if (target.isEmpty()) {
		m_target_source->setCurrentIndex(0);
	} else if (t_idx >= 0) {
		m_target_source->setCurrentIndex(t_idx);
	} else {
		m_target_source->addItem(target, target);
		m_target_source->setCurrentIndex(m_target_source->count() - 1);
	}

	m_prefix->setText(get_str("prefix"));
	m_separator->setText(get_str("separator"));
	m_suffix->setText(get_str("suffix"));
	set_combo_data(m_day_format, get_str("day_format"));
	m_max_streams->setValue((int)obs_data_get_int(d, "max_streams_displayed"));
	set_combo_data(m_category_mode, get_str("category_display_mode"));

	m_use_even_odd->setChecked(obs_data_get_bool(d, "use_even_odd_schedule"));
	m_time_fixed->setTime(parse_hhmm(get_str("stream_time_fixed")));
	m_time_odd->setTime(parse_hhmm(get_str("stream_time_odd")));
	m_time_even->setTime(parse_hhmm(get_str("stream_time_even")));
	m_update_interval->setValue((int)obs_data_get_int(d, "update_interval_sec"));

	m_file_path->setText(get_str("file_path"));
	m_discord_file_path->setText(get_str("discord_file_path"));
	set_combo_data(m_discord_week_start, get_str("discord_week_start"));
	m_discord_week_offset->setValue((int)obs_data_get_int(d, "discord_week_offset"));
	m_discord_show_past->setChecked(obs_data_get_bool(d, "discord_show_past"));
	m_discord_streams->setValue((int)obs_data_get_int(d, "discord_streams_displayed"));
	m_discord_show_category->setChecked(obs_data_get_bool(d, "discord_show_category"));
	set_combo_data(m_discord_layout, get_str("discord_layout"));
	m_file_use_emojis->setChecked(obs_data_get_bool(d, "file_use_emojis"));
	m_file_show_category->setChecked(obs_data_get_bool(d, "file_show_category"));
	m_file_max_streams->setValue((int)obs_data_get_int(d, "file_max_streams_displayed"));
	m_file_separator->setText(get_str("file_separator"));
	m_file_multiline->setChecked(obs_data_get_bool(d, "file_multiline"));

	obs_data_array_t *days = obs_data_get_array(d, "days");
	for (int i = 0; i < 7; i++) {
		obs_data_t *day = days ? obs_data_array_item(days, i) : nullptr;
		bool en = day ? obs_data_get_bool(day, "enabled") : (i == 0);
		bool autoT = day ? obs_data_get_bool(day, "auto_time") : true;
		QString cust = day ? QString::fromUtf8(obs_data_get_string(day, "custom_time")) : QString("20:00");
		QString cat = day ? QString::fromUtf8(obs_data_get_string(day, "category")) : "";

		m_day_enabled[i]->setChecked(en);
		set_combo_data(m_day_time_choice[i], autoT ? "auto" : "custom");
		m_day_custom_time[i]->setTime(parse_hhmm(cust));
		m_day_category[i]->setText(cat);

		if (day)
			obs_data_release(day);
	}
	if (days)
		obs_data_array_release(days);

	obs_data_release(d);
	m_loading = false;
}

void NextStreamDock::save_settings()
{
	obs_data_t *d = obs_data_create();

	obs_data_set_string(d, "target_source_name", selected_target_source_name().toUtf8().constData());
	obs_data_set_string(d, "prefix", m_prefix->text().toUtf8().constData());
	obs_data_set_string(d, "separator", m_separator->text().toUtf8().constData());
	obs_data_set_string(d, "suffix", m_suffix->text().toUtf8().constData());
	obs_data_set_string(d, "day_format", m_day_format->currentData().toString().toUtf8().constData());
	obs_data_set_int(d, "max_streams_displayed", m_max_streams->value());
	obs_data_set_string(d, "category_display_mode", m_category_mode->currentData().toString().toUtf8().constData());
	obs_data_set_bool(d, "use_even_odd_schedule", m_use_even_odd->isChecked());
	obs_data_set_string(d, "stream_time_fixed", m_time_fixed->time().toString("HH:mm").toUtf8().constData());
	obs_data_set_string(d, "stream_time_odd", m_time_odd->time().toString("HH:mm").toUtf8().constData());
	obs_data_set_string(d, "stream_time_even", m_time_even->time().toString("HH:mm").toUtf8().constData());
	obs_data_set_int(d, "update_interval_sec", m_update_interval->value());

	obs_data_set_string(d, "file_path", m_file_path->text().toUtf8().constData());
	obs_data_set_string(d, "discord_file_path", m_discord_file_path->text().toUtf8().constData());
	obs_data_set_string(d, "discord_week_start", m_discord_week_start->currentData().toString().toUtf8().constData());
	obs_data_set_int(d, "discord_week_offset", m_discord_week_offset->value());
	obs_data_set_bool(d, "discord_show_past", m_discord_show_past->isChecked());
	obs_data_set_int(d, "discord_streams_displayed", m_discord_streams->value());
	obs_data_set_bool(d, "discord_show_category", m_discord_show_category->isChecked());
	obs_data_set_string(d, "discord_layout", m_discord_layout->currentData().toString().toUtf8().constData());
	obs_data_set_bool(d, "file_use_emojis", m_file_use_emojis->isChecked());
	obs_data_set_bool(d, "file_show_category", m_file_show_category->isChecked());
	obs_data_set_int(d, "file_max_streams_displayed", m_file_max_streams->value());
	obs_data_set_string(d, "file_separator", m_file_separator->text().toUtf8().constData());
	obs_data_set_bool(d, "file_multiline", m_file_multiline->isChecked());

	obs_data_array_t *days = obs_data_array_create();
	for (int i = 0; i < 7; i++) {
		obs_data_t *day = obs_data_create();
		obs_data_set_bool(day, "enabled", m_day_enabled[i]->isChecked());
		obs_data_set_bool(day, "auto_time", m_day_time_choice[i]->currentData().toString() == "auto");
		obs_data_set_string(day, "custom_time",
				    m_day_custom_time[i]->time().toString("HH:mm").toUtf8().constData());
		obs_data_set_string(day, "category", m_day_category[i]->text().toUtf8().constData());
		obs_data_array_push_back(days, day);
		obs_data_release(day);
	}
	obs_data_set_array(d, "days", days);
	obs_data_array_release(days);

	QString path = config_file_path();
	if (!path.isEmpty()) {
		QFileInfo fi(path);
		QByteArray path_utf8 = path.toUtf8();
		if (!QDir().mkpath(fi.absolutePath())) {
			obs_log(LOG_WARNING, "next-stream: cannot create config directory %s",
				fi.absolutePath().toUtf8().constData());
		} else if (!obs_data_save_json_safe(d, path_utf8.constData(), "tmp", "bak")) {
			obs_log(LOG_WARNING, "next-stream: failed to save settings to %s", path_utf8.constData());
		}
	}
	obs_data_release(d);
}

// ----------------------------------------------------------------
// Source enumeration
// ----------------------------------------------------------------

static bool enum_text_sources_cb(void *param, obs_source_t *source)
{
	auto *list = (QStringList *)param;
	const char *id = obs_source_get_unversioned_id(source);
	if (id && (strcmp(id, "text_gdiplus") == 0 || strcmp(id, "text_ft2_source") == 0)) {
		const char *name = obs_source_get_name(source);
		if (name)
			list->append(QString::fromUtf8(name));
	}
	return true;
}

void NextStreamDock::refresh_source_combo()
{
	QString prev = selected_target_source_name();
	QStringList names;
	obs_enum_sources(enum_text_sources_cb, &names);
	names.sort(Qt::CaseInsensitive);

	bool was_loading = m_loading;
	m_loading = true;
	m_target_source->clear();
	m_target_source->addItem(module_text("NextStreamNoSource"), "");
	for (const QString &n : names)
		m_target_source->addItem(n, n);

	if (!prev.isEmpty()) {
		int idx = m_target_source->findData(prev);
		if (idx >= 0)
			m_target_source->setCurrentIndex(idx);
		else {
			m_target_source->addItem(prev, prev);
			m_target_source->setCurrentIndex(m_target_source->count() - 1);
		}
	}
	m_loading = was_loading;
}

void NextStreamDock::on_refresh_sources_clicked()
{
	refresh_source_combo();
}

void NextStreamDock::on_browse_file_clicked()
{
	QString start = m_file_path->text();
	QString fn = QFileDialog::getSaveFileName(this, obs_module_text("NextStreamFilePath"), start, "Text (*.txt)");
	if (!fn.isEmpty())
		m_file_path->setText(fn);
}

void NextStreamDock::on_browse_discord_file_clicked()
{
	QString start = m_discord_file_path->text();
	QString fn = QFileDialog::getSaveFileName(this, obs_module_text("NextStreamDiscordFilePath"), start,
						  "Text (*.txt)");
	if (!fn.isEmpty())
		m_discord_file_path->setText(fn);
}

static bool write_utf8_text_file(const QString &path, const QString &text, const char *label)
{
	QFileInfo fi(path);
	QByteArray path_utf8 = path.toUtf8();
	QByteArray dir_utf8 = fi.absolutePath().toUtf8();
	if (!QDir().mkpath(fi.absolutePath())) {
		obs_log(LOG_WARNING, "next-stream: cannot create %s output directory %s", label,
			dir_utf8.constData());
		return false;
	}

	QSaveFile f(path);
	if (!f.open(QIODevice::WriteOnly)) {
		QByteArray error_utf8 = f.errorString().toUtf8();
		obs_log(LOG_WARNING, "next-stream: cannot write %s output %s: %s", label, path_utf8.constData(),
			error_utf8.constData());
		return false;
	}

	QByteArray bytes = text.toUtf8();
	qint64 written = f.write(bytes);
	if (written != static_cast<qint64>(bytes.size())) {
		obs_log(LOG_WARNING, "next-stream: incomplete write for %s output %s", label, path_utf8.constData());
		f.cancelWriting();
		return false;
	}

	if (!f.commit()) {
		QByteArray error_utf8 = f.errorString().toUtf8();
		obs_log(LOG_WARNING, "next-stream: cannot commit %s output %s: %s", label, path_utf8.constData(),
			error_utf8.constData());
		return false;
	}

	return true;
}

QString NextStreamDock::wrap_category(const QString &cat, const QString &mode)
{
	if (cat.isEmpty() || mode == "hide")
		return "";
	if (mode == "show_round")
		return "(" + cat + ")";
	if (mode == "show_square")
		return "[" + cat + "]";
	return cat;
}

QString NextStreamDock::humanize_seconds(long long secs)
{
	if (secs < 0)
		secs = 0;
	long long days = secs / 86400;
	secs -= days * 86400;
	long long hours = secs / 3600;
	secs -= hours * 3600;
	long long mins = secs / 60;

	QStringList parts;
	if (days > 0)
		parts << module_text(days == 1 ? "NextStreamDaySingular" : "NextStreamDayPlural").arg(days);
	if (hours > 0)
		parts << module_text(hours == 1 ? "NextStreamHourSingular" : "NextStreamHourPlural").arg(hours);
	if (days == 0 && mins > 0)
		parts << module_text(mins == 1 ? "NextStreamMinuteSingular" : "NextStreamMinutePlural").arg(mins);
	if (parts.isEmpty())
		return module_text("NextStreamNow");
	return parts.join(" ");
}

void NextStreamDock::compute_next_streams(int max_count, QVector<StreamEntry> &out)
{
	out.clear();
	if (max_count <= 0)
		return;

	time_t now = time(nullptr);
	int count = 0;
	int offset = 0;
	const int max_offset = 365;

	while (count < max_count && offset < max_offset) {
		time_t check = now + (time_t)offset * 86400;
		struct tm dt;
#ifdef _WIN32
		localtime_s(&dt, &check);
#else
		localtime_r(&check, &dt);
#endif
		int wday = dt.tm_wday;
		int mon_first = (wday == 0) ? 6 : (wday - 1);

		bool day_enabled = m_day_enabled[mon_first]->isChecked();
		if (day_enabled) {
			bool auto_time = (m_day_time_choice[mon_first]->currentData().toString() == "auto");
			QTime chosen;
			if (auto_time) {
				if (m_use_even_odd->isChecked()) {
					int week = iso_week_from_tm(&dt);
					chosen = (week % 2 == 0) ? m_time_even->time() : m_time_odd->time();
				} else {
					chosen = m_time_fixed->time();
				}
			} else {
				chosen = m_day_custom_time[mon_first]->time();
			}

			struct tm st = dt;
			st.tm_hour = chosen.hour();
			st.tm_min = chosen.minute();
			st.tm_sec = 0;
			st.tm_isdst = -1;
			time_t stream_start = mktime(&st);

			if (stream_start > now) {
				QString day_name;
				if (offset == 0)
					day_name = module_text("NextStreamToday");
				else if (offset == 1)
					day_name = module_text("NextStreamTomorrow");
				else {
					QString fmt = m_day_format->currentData().toString();
					day_name = (fmt == "full") ? day_name_full(mon_first) : day_name_short(mon_first);
				}
				StreamEntry e;
				e.day_name = day_name;
				e.time_hhmm = chosen.toString("HH:mm");
				e.category = m_day_category[mon_first]->text();
				e.start_time = stream_start;
				e.day_index = mon_first;
				e.day = st.tm_mday;
				e.month = st.tm_mon + 1;
				e.seconds_from_now = (long long)(stream_start - now);
				out.append(e);
				count++;
			}
		}
		offset++;
	}
}

void NextStreamDock::compute_discord_week_streams(QVector<StreamEntry> &out, QString &week_header)
{
	out.clear();

	time_t now = time(nullptr);
	struct tm now_tm;
#ifdef _WIN32
	localtime_s(&now_tm, &now);
#else
	localtime_r(&now, &now_tm);
#endif

	struct tm week_start_tm = now_tm;
	week_start_tm.tm_hour = 0;
	week_start_tm.tm_min = 0;
	week_start_tm.tm_sec = 0;
	week_start_tm.tm_isdst = -1;
	const QString week_start_mode = m_discord_week_start->currentData().toString();
	if (week_start_mode == "sunday") {
		week_start_tm.tm_mday -= days_since_wday(week_start_tm.tm_wday, 0);
	} else if (week_start_mode == "monday") {
		week_start_tm.tm_mday -= days_since_wday(week_start_tm.tm_wday, 1);
	}
	week_start_tm.tm_mday += m_discord_week_offset->value() * 7;
	time_t week_start = mktime(&week_start_tm);

	struct tm header_tm;
#ifdef _WIN32
	localtime_s(&header_tm, &week_start);
#else
	localtime_r(&week_start, &header_tm);
#endif
	week_header = module_text("NextStreamDiscordWeekHeader")
			      .arg(header_tm.tm_mday, 2, 10, QLatin1Char('0'))
			      .arg(header_tm.tm_mon + 1, 2, 10, QLatin1Char('0'));

	const int stream_limit = (m_discord_streams->value() > 0) ? m_discord_streams->value() : 3;

	for (int offset = 0; offset < 366 && out.size() < stream_limit; offset++) {
		struct tm dt = header_tm;
		dt.tm_mday += offset;
		dt.tm_hour = 0;
		dt.tm_min = 0;
		dt.tm_sec = 0;
		dt.tm_isdst = -1;
		time_t day_time = mktime(&dt);

#ifdef _WIN32
		localtime_s(&dt, &day_time);
#else
		localtime_r(&day_time, &dt);
#endif
		int mon_first = (dt.tm_wday == 0) ? 6 : (dt.tm_wday - 1);
		if (!m_day_enabled[mon_first]->isChecked())
			continue;

		bool auto_time = (m_day_time_choice[mon_first]->currentData().toString() == "auto");
		QTime chosen;
		if (auto_time) {
			if (m_use_even_odd->isChecked()) {
				int week = iso_week_from_tm(&dt);
				chosen = (week % 2 == 0) ? m_time_even->time() : m_time_odd->time();
			} else {
				chosen = m_time_fixed->time();
			}
		} else {
			chosen = m_day_custom_time[mon_first]->time();
		}

		struct tm st = dt;
		st.tm_hour = chosen.hour();
		st.tm_min = chosen.minute();
		st.tm_sec = 0;
		st.tm_isdst = -1;
		time_t stream_start = mktime(&st);
		if (!m_discord_show_past->isChecked() && stream_start <= now)
			continue;

		StreamEntry e;
		e.day_name = day_name_full(mon_first);
		e.time_hhmm = chosen.toString("HH:mm");
		e.category = m_day_category[mon_first]->text();
		e.start_time = stream_start;
		e.day_index = mon_first;
		e.day = st.tm_mday;
		e.month = st.tm_mon + 1;
		e.seconds_from_now = (long long)(stream_start - now);
		out.append(e);
	}
}

QString NextStreamDock::render_obs(const QVector<StreamEntry> &list)
{
	if (list.isEmpty())
		return module_text("NextStreamNoStreamPlanned");

	QString sep = m_separator->text();
	if (sep.isEmpty())
		sep = module_text("NextStreamDefaultSeparator");
	QString suffix = m_suffix->text();
	QString cat_mode = m_category_mode->currentData().toString();
	QStringList lines;

	for (const StreamEntry &e : list) {
		QString cat = wrap_category(e.category, cat_mode);
		QString line = e.day_name;
		if (!cat.isEmpty())
			line += " " + sep + " " + cat;
		line += " " + sep + " " + e.time_hhmm;
		if (!suffix.isEmpty())
			line += " " + suffix;
		lines.append(line);
	}

	QString prefix = m_prefix->text();
	if (prefix.isEmpty())
		return lines.join("\n");
	return prefix + "\n" + lines.join("\n");
}

QString NextStreamDock::render_file(const QVector<StreamEntry> &list)
{
	if (list.isEmpty())
		return module_text("NextStreamNoStreamPlanned");

	QString sep = m_file_separator->text();
	if (sep.isEmpty())
		sep = module_text("NextStreamDefaultSeparator");
	QString suffix = m_suffix->text();
	bool emojis = m_file_use_emojis->isChecked();
	bool show_cat = m_file_show_category->isChecked();
	QString base_mode = m_category_mode->currentData().toString();
	QString cat_mode = (base_mode == "hide") ? "show" : base_mode;
	bool multiline = m_file_multiline->isChecked();

	QStringList lines;
	for (const StreamEntry &e : list) {
		QString line = emojis ? (QString::fromUtf8("📅 ") + e.day_name) : e.day_name;
		if (show_cat && !e.category.isEmpty()) {
			QString cat = wrap_category(e.category, cat_mode);
			if (!cat.isEmpty())
				line += " " + sep + " " + cat;
		}
		line += " " + sep + " " + e.time_hhmm;
		if (!suffix.isEmpty())
			line += " " + suffix;
		lines.append(line);
	}

	if (multiline)
		return lines.join("\n");
	return lines.join(" " + sep + " ");
}

QString NextStreamDock::render_discord_file(const QVector<StreamEntry> &list, const QString &week_header)
{
	QStringList lines;
	const bool show_category = m_discord_show_category->isChecked();
	const bool spacious = (m_discord_layout->currentData().toString() == "spacious");

	lines << "**" + week_header + "**";
	if (spacious)
		lines << "";

	if (list.isEmpty()) {
		lines << module_text("NextStreamNoStreamPlanned");
		return lines.join("\n");
	}

	time_t now = time(nullptr);
	for (int i = 0; i < list.size(); i++) {
		const StreamEntry &e = list[i];
		QString marker = (e.start_time <= now) ? QString::fromUtf8("\xE2\x9C\x85")
						      : QString::fromUtf8("\xE2\x9E\xA1\xEF\xB8\x8F");
		QString date_line = marker + " **" + e.day_name + ", " +
				    QString("%1.%2.")
					    .arg(e.day, 2, 10, QLatin1Char('0'))
					    .arg(e.month, 2, 10, QLatin1Char('0')) +
				    "**";
		QString time_line = QString::fromUtf8("🕖 ") + e.time_hhmm;
		QString suffix = module_text("NextStreamClockSuffix");
		if (!suffix.isEmpty())
			time_line += " " + suffix;

		QString category = e.category.trimmed();
		if (show_category && !category.isEmpty()) {
			QString category_lower = category.toLower();
			if (category_lower == "vielleicht" || category_lower == "maybe")
				time_line += QString::fromUtf8(" · *") + category + "*";
			else
				time_line += QString::fromUtf8(" · **") + category + "**";
		}

		lines << date_line << time_line;

		if (spacious && e.day_index == 6) {
			lines << QString(20, QChar(0x2501));
			if (i + 1 < list.size())
				lines << "";
		} else if (spacious && i + 1 < list.size()) {
			lines << "";
		}
	}

	return lines.join("\n");
}

void NextStreamDock::on_settings_changed()
{
	if (!m_loading)
		save_settings();
}

void NextStreamDock::on_update_timer()
{
	if (m_shutdown)
		return;

	QVector<StreamEntry> list_obs;
	compute_next_streams(m_max_streams->value(), list_obs);
	QString obs_text = render_obs(list_obs);

	QString target = selected_target_source_name();
	if (!target.isEmpty() && (target != m_last_obs_target || obs_text != m_last_obs_text)) {
		obs_source_t *src = obs_get_source_by_name(target.toUtf8().constData());
		if (src) {
			obs_data_t *sd = obs_data_create();
			obs_data_set_string(sd, "text", obs_text.toUtf8().constData());
			obs_source_update(src, sd);
			obs_data_release(sd);
			obs_source_release(src);
			m_last_obs_text = obs_text;
			m_last_obs_target = target;
		}
	} else if (target.isEmpty()) {
		m_last_obs_target.clear();
	}

	QString file_path = m_file_path->text();
	if (!file_path.isEmpty()) {
		QVector<StreamEntry> list_file;
		compute_next_streams(m_file_max_streams->value(), list_file);
		QString file_text = render_file(list_file);
		if (file_path != m_last_file_path || file_text != m_last_file_text) {
			if (write_utf8_text_file(file_path, file_text, "free TXT")) {
				m_last_file_text = file_text;
				m_last_file_path = file_path;
			}
		}
	} else {
		m_last_file_path.clear();
		m_last_file_text.clear();
	}

	QString discord_path = m_discord_file_path->text();
	if (!discord_path.isEmpty()) {
		QVector<StreamEntry> list_discord;
		QString week_header;
		compute_discord_week_streams(list_discord, week_header);
		QString discord_text = render_discord_file(list_discord, week_header);
		if (discord_path != m_last_discord_path || discord_text != m_last_discord_file_text) {
			if (write_utf8_text_file(discord_path, discord_text, "Discord TXT")) {
				m_last_discord_file_text = discord_text;
				m_last_discord_path = discord_path;
			}
		}
	} else {
		m_last_discord_path.clear();
		m_last_discord_file_text.clear();
	}
}

void NextStreamDock::on_preview_timer()
{
	if (m_shutdown)
		return;

	QDateTime now = QDateTime::currentDateTime();
	if (m_status_clock) {
		m_status_clock->setText(now.toString("HH:mm:ss · dd.MM.yyyy"));
	}

	QVector<StreamEntry> next1;
	compute_next_streams(1, next1);
	if (m_status_next) {
		if (next1.isEmpty()) {
			m_status_next->setText(module_text("NextStreamNoStreamPlanned"));
			m_status_next->setStyleSheet("color: #888; font-weight: 600; font-size: 11pt;");
		} else {
			const StreamEntry &e = next1.first();
			QString cat = e.category.isEmpty() ? "" : (QString::fromUtf8(" · ") + e.category);
			QString suffix = module_text("NextStreamClockSuffix");
			QString time_text = e.time_hhmm + (suffix.isEmpty() ? QString() : (QString(" ") + suffix));
			m_status_next->setText(module_text("NextStreamNextStreamStatus")
						       .arg(e.day_name, time_text, cat,
							    humanize_seconds(e.seconds_from_now)));
			m_status_next->setStyleSheet("color: #f0c040; font-weight: 600; font-size: 11pt;");
		}
	}

	if (m_preview) {
		QVector<StreamEntry> list;
		compute_next_streams(m_max_streams->value(), list);
		m_preview->setPlainText(render_obs(list));
	}
}

// ---------------------------------------------------------------------------
// C-callable bridge for plugin-main.c
// ---------------------------------------------------------------------------

static NextStreamDock *g_dock = nullptr;
static bool g_dock_registered = false;
static bool g_frontend_callback_registered = false;
static bool g_frontend_api_closed = false;
static const char *DOCK_ID = "dynamic-texts.next-stream";

static void on_frontend_event(enum obs_frontend_event event, void *);

static void next_stream_dock_cleanup(bool frontend_available)
{
	if (!g_dock)
		return;

	if (frontend_available && g_frontend_callback_registered) {
		obs_frontend_remove_event_callback(on_frontend_event, nullptr);
		g_frontend_callback_registered = false;
	}

	g_dock->shutdown();

	if (frontend_available && g_dock_registered) {
		obs_frontend_remove_dock(DOCK_ID);
		g_dock_registered = false;
	} else if (!frontend_available) {
		g_frontend_callback_registered = false;
		g_dock_registered = false;
	}

	g_dock = nullptr;
}

static void on_frontend_event(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING && g_dock)
		QMetaObject::invokeMethod(g_dock, "on_refresh_sources_clicked", Qt::QueuedConnection);
	else if (event == OBS_FRONTEND_EVENT_EXIT) {
		next_stream_dock_cleanup(true);
		g_frontend_api_closed = true;
	}
}

extern "C" void next_stream_dock_register(void)
{
	if (g_dock)
		return;
	g_frontend_api_closed = false;
	g_dock = new NextStreamDock();
	if (!obs_frontend_add_dock_by_id(DOCK_ID, obs_module_text("NextStreamDockTitle"), g_dock)) {
		obs_log(LOG_WARNING, "dock registration failed: %s", DOCK_ID);
		delete g_dock;
		g_dock = nullptr;
		return;
	}
	g_dock_registered = true;
	obs_frontend_add_event_callback(on_frontend_event, nullptr);
	g_frontend_callback_registered = true;
	obs_log(LOG_INFO, "dock registered: %s", DOCK_ID);
}

extern "C" void next_stream_dock_unregister(void)
{
	next_stream_dock_cleanup(!g_frontend_api_closed);
}
