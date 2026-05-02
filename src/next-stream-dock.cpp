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

#include <ctime>
#include <vector>

static const char *DAYS_FULL_DE[7] = {"Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag", "Sonntag"};
static const char *DAYS_SHORT_DE[7] = {"Mo", "Di", "Mi", "Do", "Fr", "Sa", "So"};

static QTime parse_hhmm(const QString &s)
{
	QTime t = QTime::fromString(s, "H:mm");
	if (!t.isValid())
		t = QTime::fromString(s, "HH:mm");
	if (!t.isValid())
		t = QTime(20, 0);
	return t;
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

NextStreamDock::~NextStreamDock() = default;

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
	m_target_source->setToolTip(QString::fromUtf8(
		"OBS-Textquelle, in die der Plan geschrieben wird.\n"
		"Liste enthält alle Text (GDI+) und Text (FreeType 2) Quellen."));
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

		auto *help = new QLabel(QString::fromUtf8(
			"Hier den Wochenplan editieren. 'Auto' nimmt die globale "
			"Stream-Zeit aus dem Tab Format. 'Manuell' = eigene Zeit pro Tag."));
		help->setWordWrap(true);
		help->setStyleSheet("color: #888; font-size: 9pt; padding: 4px;");
		tab_lay->addWidget(help);

		// Header row
		auto *hdr = new QGridLayout();
		hdr->setContentsMargins(4, 0, 4, 0);
		hdr->setHorizontalSpacing(8);
		auto add_hdr = [&](int col, const QString &text, int stretch = 0) {
			auto *l = new QLabel(text);
			l->setStyleSheet("color: #888; font-size: 9pt; font-weight: 600;");
			hdr->addWidget(l, 0, col);
			if (stretch)
				hdr->setColumnStretch(col, stretch);
		};
		add_hdr(0, QString::fromUtf8("Tag"));
		add_hdr(1, QString::fromUtf8("Modus"));
		add_hdr(2, QString::fromUtf8("Zeit"));
		add_hdr(3, QString::fromUtf8("Kategorie"), 1);
		tab_lay->addLayout(hdr);

		auto *days_grid = new QGridLayout();
		days_grid->setHorizontalSpacing(8);
		days_grid->setVerticalSpacing(4);

		for (int i = 0; i < 7; i++) {
			m_day_enabled[i] = new QCheckBox(DAYS_FULL_DE[i]);
			m_day_enabled[i]->setMinimumWidth(110);
			m_day_enabled[i]->setToolTip(
				QString::fromUtf8("Diesen Wochentag in den Plan aufnehmen"));

			m_day_time_choice[i] = new QComboBox();
			m_day_time_choice[i]->addItem(obs_module_text("NextStreamTimeAuto"), "auto");
			m_day_time_choice[i]->addItem(obs_module_text("NextStreamTimeManual"), "custom");
			m_day_time_choice[i]->setMinimumWidth(100);
			m_day_time_choice[i]->setToolTip(QString::fromUtf8(
				"Auto = nimmt die globale Stream-Zeit aus Tab Format\n"
				"Manuell = eigene Zeit nur für diesen Tag"));

			m_day_custom_time[i] = new QTimeEdit();
			m_day_custom_time[i]->setDisplayFormat("HH:mm");
			m_day_custom_time[i]->setTime(QTime(20, 0));
			m_day_custom_time[i]->setToolTip(QString::fromUtf8("Manuelle Stream-Zeit für diesen Tag"));
			m_day_custom_time[i]->setMinimumWidth(80);

			m_day_category[i] = new QLineEdit();
			m_day_category[i]->setPlaceholderText(QString::fromUtf8("optional, z. B. Just Chatting"));
			m_day_category[i]->setToolTip(QString::fromUtf8(
				"Kategorie/Spiel für diesen Tag — wird im Output mit angezeigt\n"
				"(siehe Tab Format -> Kategorie-Anzeige)"));

			days_grid->addWidget(m_day_enabled[i], i, 0);
			days_grid->addWidget(m_day_time_choice[i], i, 1);
			days_grid->addWidget(m_day_custom_time[i], i, 2);
			days_grid->addWidget(m_day_category[i], i, 3);
		}
		days_grid->setColumnStretch(3, 1);
		tab_lay->addLayout(days_grid);
		tab_lay->addStretch(1);

		m_tabs->addTab(tab, "Plan");
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
			m_prefix->setPlaceholderText(QString::fromUtf8("z. B. Nächster Stream"));
			m_prefix->setToolTip(QString::fromUtf8(
				"Text vor dem Plan — landet in einer eigenen Zeile in OBS.\n"
				"Leer = kein Präfix."));

			m_separator = new QLineEdit();
			m_separator->setPlaceholderText(QString::fromUtf8("—"));
			m_separator->setToolTip(QString::fromUtf8(
				"Zeichen zwischen Tagesname, Kategorie und Zeit.\n"
				"Beispiel mit '-': Montag - Just Chatting - 20:00 Uhr"));

			m_suffix = new QLineEdit();
			m_suffix->setPlaceholderText(QString::fromUtf8("Uhr"));
			m_suffix->setToolTip(QString::fromUtf8("Zusatztext nach der Uhrzeit. Leer = nichts dahinter."));

			m_day_format = new QComboBox();
			m_day_format->addItem(obs_module_text("NextStreamDayFormatFull"), "full");
			m_day_format->addItem(obs_module_text("NextStreamDayFormatShort"), "short");
			m_day_format->setToolTip(QString::fromUtf8(
				"Voll: 'Montag', 'Dienstag', ...\n"
				"Kurz: 'Mo', 'Di', ...\n"
				"'Heute' und 'Morgen' werden in beiden Modi automatisch verwendet."));

			m_max_streams = new QSpinBox();
			m_max_streams->setRange(1, 7);
			m_max_streams->setToolTip(QString::fromUtf8(
				"Wie viele zukünftige Streams in der OBS-Textquelle angezeigt werden."));

			m_category_mode = new QComboBox();
			m_category_mode->addItem(obs_module_text("NextStreamCatShow"), "show");
			m_category_mode->addItem(obs_module_text("NextStreamCatRound"), "show_round");
			m_category_mode->addItem(obs_module_text("NextStreamCatSquare"), "show_square");
			m_category_mode->addItem(obs_module_text("NextStreamCatHide"), "hide");
			m_category_mode->setToolTip(QString::fromUtf8(
				"Wie die Kategorie eines Tages dargestellt wird:\n"
				"Zeigen = klar Text\n"
				"( ) = in Klammern\n"
				"[ ] = in eckigen Klammern\n"
				"Ausblenden = Kategorie nicht in OBS-Output (Datei kann separat)"));

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
			m_use_even_odd->setToolTip(QString::fromUtf8(
				"Aktivieren, um in geraden und ungeraden Kalenderwochen unterschiedliche\n"
				"Stream-Zeiten zu verwenden (z. B. KW 1 = 20:00, KW 2 = 21:00).\n"
				"Berechnung nach ISO 8601."));

			m_time_fixed = new QTimeEdit(QTime(20, 0));
			m_time_fixed->setDisplayFormat("HH:mm");
			m_time_fixed->setToolTip(QString::fromUtf8(
				"Globale Stream-Zeit für alle Tage im Auto-Modus.\n"
				"Wird ignoriert, wenn 'gerade/ungerade Woche' aktiv ist."));

			m_time_odd = new QTimeEdit(QTime(20, 0));
			m_time_odd->setDisplayFormat("HH:mm");
			m_time_odd->setToolTip(QString::fromUtf8("Stream-Zeit für KW 1, 3, 5, 7, …"));

			m_time_even = new QTimeEdit(QTime(21, 0));
			m_time_even->setDisplayFormat("HH:mm");
			m_time_even->setToolTip(QString::fromUtf8("Stream-Zeit für KW 2, 4, 6, 8, …"));

			m_update_interval = new QSpinBox();
			m_update_interval->setRange(10, 3600);
			m_update_interval->setSuffix(" s");
			m_update_interval->setToolTip(QString::fromUtf8(
				"Wie oft die Textquelle (und ggf. die Datei) neu geschrieben werden.\n"
				"30 s ist ein guter Wert — kürzer macht nur Sinn, wenn Stream gleich startet."));

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
		m_tabs->addTab(tab, "Format");
		m_tabs->setTabIcon(1, style()->standardIcon(QStyle::SP_FileDialogDetailedView));
	}

	// ---------- Tab 3: DATEI ----------
	{
		auto *tab = new QWidget();
		auto *vbox = new QVBoxLayout(tab);
		vbox->setContentsMargins(6, 6, 6, 6);

		auto *grp = new QGroupBox(obs_module_text("NextStreamGroupFile"));
		auto *form = new QFormLayout(grp);
		form->setLabelAlignment(Qt::AlignRight);

		auto *path_row = new QHBoxLayout();
		m_file_path = new QLineEdit();
		m_file_path->setPlaceholderText(QString::fromUtf8("z. B. C:\\stream\\next.txt — leer lassen für kein Datei-Export"));
		m_file_path->setToolTip(QString::fromUtf8(
			"Pfad zu einer .txt-Datei, in die der Plan geschrieben wird.\n"
			"Leer = nichts in Datei schreiben."));
		m_file_browse = new QPushButton("…");
		m_file_browse->setMaximumWidth(34);
		m_file_browse->setToolTip(QString::fromUtf8("Datei auswählen"));
		path_row->addWidget(m_file_path, 1);
		path_row->addWidget(m_file_browse);

		m_file_use_emojis = new QCheckBox(obs_module_text("NextStreamFileEmojis"));
		m_file_use_emojis->setToolTip(QString::fromUtf8(
			"Mit Emojis: '\xf0\x9f\x93\x85 Montag - 20:00 Uhr'\n"
			"Ohne: 'Montag - 20:00 Uhr'"));

		m_file_show_category = new QCheckBox(obs_module_text("NextStreamFileShowCat"));
		m_file_show_category->setToolTip(QString::fromUtf8(
			"Kategorie auch in Datei mitschreiben.\n"
			"Wenn Kategorie-Anzeige in OBS auf 'Ausblenden' steht, "
			"kann die Datei sie trotzdem zeigen."));

		m_file_max_streams = new QSpinBox();
		m_file_max_streams->setRange(1, 7);
		m_file_max_streams->setToolTip(QString::fromUtf8(
			"Wie viele Streams in die Datei geschrieben werden.\n"
			"Kann sich von der OBS-Anzahl unterscheiden — z. B. OBS = 1, Datei = 3 für eine Übersicht."));

		m_file_separator = new QLineEdit();
		m_file_separator->setPlaceholderText(QString::fromUtf8("—"));
		m_file_separator->setToolTip(QString::fromUtf8(
			"Trennzeichen in der Datei. Bei einzeiliger Anzeige auch zwischen den Streams."));

		m_file_multiline = new QCheckBox(obs_module_text("NextStreamFileMultiline"));
		m_file_multiline->setToolTip(QString::fromUtf8(
			"Aktiv: jeder Stream in eigener Zeile.\n"
			"Inaktiv: alles in einer Zeile mit Trennzeichen dazwischen."));

		form->addRow(obs_module_text("NextStreamFilePath"), path_row);
		form->addRow(QString(), m_file_use_emojis);
		form->addRow(QString(), m_file_show_category);
		form->addRow(obs_module_text("NextStreamFileMaxStreams"), m_file_max_streams);
		form->addRow(obs_module_text("NextStreamFileSeparator"), m_file_separator);
		form->addRow(QString(), m_file_multiline);
		vbox->addWidget(grp);
		vbox->addStretch(1);
		m_tabs->addTab(tab, "Datei");
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
		m_day_time_choice[6],
	};
	for (QComboBox *c : combos)
		c->setStyleSheet(c->styleSheet() + comboPopupQss);
}

void NextStreamDock::connect_signals()
{
	auto onChange = [this]() {
		if (!m_loading) {
			save_settings();
			on_update_timer();
			on_preview_timer();
		}
	};

	connect(m_prefix, &QLineEdit::textChanged, this, onChange);
	connect(m_separator, &QLineEdit::textChanged, this, onChange);
	connect(m_suffix, &QLineEdit::textChanged, this, onChange);
	connect(m_file_path, &QLineEdit::textChanged, this, onChange);
	connect(m_file_separator, &QLineEdit::textChanged, this, onChange);

	connect(m_target_source, &QComboBox::currentTextChanged, this, onChange);
	connect(m_day_format, &QComboBox::currentIndexChanged, this, onChange);
	connect(m_category_mode, &QComboBox::currentIndexChanged, this, onChange);

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

void NextStreamDock::load_settings()
{
	m_loading = true;

	QString path = config_file_path();
	obs_data_t *d = nullptr;
	if (!path.isEmpty() && QFile::exists(path))
		d = obs_data_create_from_json_file(path.toUtf8().constData());
	if (!d)
		d = obs_data_create();

	obs_data_set_default_string(d, "target_source_name", "");
	obs_data_set_default_string(d, "prefix", "Nächster Stream");
	obs_data_set_default_string(d, "separator", "—");
	obs_data_set_default_string(d, "suffix", "Uhr");
	obs_data_set_default_string(d, "day_format", "full");
	obs_data_set_default_int(d, "max_streams_displayed", 1);
	obs_data_set_default_string(d, "category_display_mode", "show");
	obs_data_set_default_bool(d, "use_even_odd_schedule", false);
	obs_data_set_default_string(d, "stream_time_fixed", "20:00");
	obs_data_set_default_string(d, "stream_time_odd", "20:00");
	obs_data_set_default_string(d, "stream_time_even", "21:00");
	obs_data_set_default_int(d, "update_interval_sec", 30);
	obs_data_set_default_string(d, "file_path", "");
	obs_data_set_default_bool(d, "file_use_emojis", true);
	obs_data_set_default_bool(d, "file_show_category", true);
	obs_data_set_default_int(d, "file_max_streams_displayed", 1);
	obs_data_set_default_string(d, "file_separator", "—");
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
	if (t_idx < 0)
		m_target_source->addItem(target, target);
	m_target_source->setCurrentText(target);

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

	obs_data_set_string(d, "target_source_name", m_target_source->currentText().toUtf8().constData());
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
		QDir().mkpath(fi.absolutePath());
		obs_data_save_json(d, path.toUtf8().constData());
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
	QString prev = m_target_source->currentText();
	QStringList names;
	obs_enum_sources(enum_text_sources_cb, &names);
	names.sort(Qt::CaseInsensitive);

	bool was_loading = m_loading;
	m_loading = true;
	m_target_source->clear();
	m_target_source->addItem(QString::fromUtf8("— keine —"), "");
	for (const QString &n : names)
		m_target_source->addItem(n, n);

	if (!prev.isEmpty()) {
		int idx = m_target_source->findData(prev);
		if (idx >= 0)
			m_target_source->setCurrentIndex(idx);
		else {
			m_target_source->addItem(prev, prev);
			m_target_source->setCurrentText(prev);
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
		parts << QString::number(days) + (days == 1 ? " Tag" : " Tagen");
	if (hours > 0)
		parts << QString::number(hours) + (hours == 1 ? " Stunde" : " Stunden");
	if (days == 0 && mins > 0)
		parts << QString::number(mins) + (mins == 1 ? " Minute" : " Minuten");
	if (parts.isEmpty())
		return QString::fromUtf8("gleich");
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
					day_name = QString::fromUtf8("Heute");
				else if (offset == 1)
					day_name = QString::fromUtf8("Morgen");
				else {
					QString fmt = m_day_format->currentData().toString();
					day_name = QString::fromUtf8(fmt == "full" ? DAYS_FULL_DE[mon_first]
										   : DAYS_SHORT_DE[mon_first]);
				}
				StreamEntry e;
				e.day_name = day_name;
				e.time_hhmm = chosen.toString("HH:mm");
				e.category = m_day_category[mon_first]->text();
				e.seconds_from_now = (long long)(stream_start - now);
				out.append(e);
				count++;
			}
		}
		offset++;
	}
}

QString NextStreamDock::render_obs(const QVector<StreamEntry> &list)
{
	if (list.isEmpty())
		return QString::fromUtf8("Kein Stream geplant");

	QString sep = m_separator->text();
	if (sep.isEmpty())
		sep = "—";
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
		return QString::fromUtf8("Kein Stream geplant");

	QString sep = m_file_separator->text();
	if (sep.isEmpty())
		sep = "—";
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

void NextStreamDock::on_settings_changed()
{
	if (!m_loading)
		save_settings();
}

void NextStreamDock::on_update_timer()
{
	QVector<StreamEntry> list_obs;
	compute_next_streams(m_max_streams->value(), list_obs);
	QString obs_text = render_obs(list_obs);

	QString target = m_target_source->currentText();
	if (!target.isEmpty() && obs_text != m_last_obs_text) {
		obs_source_t *src = obs_get_source_by_name(target.toUtf8().constData());
		if (src) {
			obs_data_t *sd = obs_data_create();
			obs_data_set_string(sd, "text", obs_text.toUtf8().constData());
			obs_source_update(src, sd);
			obs_data_release(sd);
			obs_source_release(src);
			m_last_obs_text = obs_text;
		}
	}

	if (!m_file_path->text().isEmpty()) {
		QVector<StreamEntry> list_file;
		compute_next_streams(m_file_max_streams->value(), list_file);
		QString file_text = render_file(list_file);
		if (file_text != m_last_file_text) {
			QFile f(m_file_path->text());
			if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
				f.write(file_text.toUtf8());
				f.close();
				m_last_file_text = file_text;
			} else {
				obs_log(LOG_WARNING, "next-stream: cannot write %s",
					m_file_path->text().toUtf8().constData());
			}
		}
	}
}

void NextStreamDock::on_preview_timer()
{
	QDateTime now = QDateTime::currentDateTime();
	if (m_status_clock) {
		m_status_clock->setText(now.toString("HH:mm:ss · dd.MM.yyyy"));
	}

	QVector<StreamEntry> next1;
	compute_next_streams(1, next1);
	if (m_status_next) {
		if (next1.isEmpty()) {
			m_status_next->setText(QString::fromUtf8("Kein Stream geplant"));
			m_status_next->setStyleSheet("color: #888; font-weight: 600; font-size: 11pt;");
		} else {
			const StreamEntry &e = next1.first();
			QString cat = e.category.isEmpty() ? "" : (QString::fromUtf8(" · ") + e.category);
			m_status_next->setText(
				QString::fromUtf8("Nächster Stream: %1 um %2 Uhr%3  —  in %4")
					.arg(e.day_name, e.time_hhmm, cat, humanize_seconds(e.seconds_from_now)));
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
static const char *DOCK_ID = "dynamic-texts.next-stream";

static void on_finished_loading(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING && g_dock)
		QMetaObject::invokeMethod(g_dock, "on_refresh_sources_clicked", Qt::QueuedConnection);
}

extern "C" void next_stream_dock_register(void)
{
	if (g_dock)
		return;
	g_dock = new NextStreamDock();
	obs_frontend_add_dock_by_id(DOCK_ID, obs_module_text("NextStreamDockTitle"), g_dock);
	obs_frontend_add_event_callback(on_finished_loading, nullptr);
	obs_log(LOG_INFO, "dock registered: %s", DOCK_ID);
}

extern "C" void next_stream_dock_unregister(void)
{
	if (!g_dock)
		return;
	obs_frontend_remove_event_callback(on_finished_loading, nullptr);
	g_dock = nullptr;
}
