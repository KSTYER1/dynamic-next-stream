#pragma once

#include <QFrame>
#include <QString>
#include <QVector>

class QComboBox;
class QLineEdit;
class QSpinBox;
class QCheckBox;
class QLabel;
class QPlainTextEdit;
class QGroupBox;
class QTimer;
class QPushButton;
class QFormLayout;
class QTabWidget;
class QTimeEdit;
struct obs_data;
typedef struct obs_data obs_data_t;

struct DayConfig {
	bool enabled = false;
	bool auto_time = true;
	QString custom_time;
	QString category;
};

struct StreamEntry {
	QString day_name;
	QString time_hhmm;
	QString category;
	long long seconds_from_now = 0;
};

class NextStreamDock : public QFrame {
	Q_OBJECT

public:
	explicit NextStreamDock(QWidget *parent = nullptr);
	~NextStreamDock() override;

public slots:
	void on_refresh_sources_clicked();

private slots:
	void on_settings_changed();
	void on_browse_file_clicked();
	void on_update_timer();
	void on_preview_timer();
	void on_use_even_odd_toggled(bool checked);
	void on_day_mode_changed(int day_index);

private:
	void build_ui();
	void connect_signals();
	void load_settings();
	void save_settings();
	void apply_settings_to_widgets();
	void apply_widgets_to_settings();
	void refresh_source_combo();

	void compute_next_streams(int max_count, QVector<StreamEntry> &out);
	QString render_obs(const QVector<StreamEntry> &list);
	QString render_file(const QVector<StreamEntry> &list);
	QString wrap_category(const QString &cat, const QString &mode);
	QString humanize_seconds(long long secs);

	QString config_file_path() const;

	bool m_loading = false;

	// Sticky top
	QLabel *m_status_clock = nullptr;
	QLabel *m_status_next = nullptr;
	QPlainTextEdit *m_preview = nullptr;
	QPushButton *m_preview_copy_btn = nullptr;

	// Source row
	QComboBox *m_target_source = nullptr;
	QPushButton *m_refresh_btn = nullptr;

	// Tabs
	QTabWidget *m_tabs = nullptr;

	// Format tab
	QLineEdit *m_prefix = nullptr;
	QLineEdit *m_separator = nullptr;
	QLineEdit *m_suffix = nullptr;
	QComboBox *m_day_format = nullptr;
	QSpinBox *m_max_streams = nullptr;
	QComboBox *m_category_mode = nullptr;

	QCheckBox *m_use_even_odd = nullptr;
	QTimeEdit *m_time_fixed = nullptr;
	QTimeEdit *m_time_odd = nullptr;
	QTimeEdit *m_time_even = nullptr;
	QSpinBox *m_update_interval = nullptr;
	QFormLayout *m_times_form = nullptr;

	// File tab
	QLineEdit *m_file_path = nullptr;
	QPushButton *m_file_browse = nullptr;
	QCheckBox *m_file_use_emojis = nullptr;
	QCheckBox *m_file_show_category = nullptr;
	QSpinBox *m_file_max_streams = nullptr;
	QLineEdit *m_file_separator = nullptr;
	QCheckBox *m_file_multiline = nullptr;

	// Days
	QCheckBox *m_day_enabled[7] = {nullptr};
	QComboBox *m_day_time_choice[7] = {nullptr};
	QTimeEdit *m_day_custom_time[7] = {nullptr};
	QLineEdit *m_day_category[7] = {nullptr};

	// Timers
	QTimer *m_update_timer_obj = nullptr;
	QTimer *m_preview_timer_obj = nullptr;

	QString m_last_obs_text;
	QString m_last_file_text;

	obs_data_t *m_pending_load = nullptr;
};
