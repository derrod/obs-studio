#include <QFile>
#include <QTextStream>
#include <QScrollBar>
#include <QFont>
#include <QFontDatabase>
#include <QPushButton>
#include <QCheckBox>
#include <QLayout>
#include <QDesktopServices>
#include <string>

#include "perf-viewer.hpp"
#include "qt-wrappers.hpp"

OBSPerfViewer::OBSPerfViewer(QWidget *parent)
	: QDialog(parent),
	  ui(new Ui::OBSPerfViewer)
{
	setWindowFlags(windowFlags() & Qt::WindowMaximizeButtonHint &
		       ~Qt::WindowContextHelpButtonHint);
	setAttribute(Qt::WA_DeleteOnClose);

	ui->setupUi(this);

	const char *geom = config_get_string(App()->GlobalConfig(),
					     "PerfViewer", "geometry");

	if (geom != nullptr) {
		QByteArray ba = QByteArray::fromBase64(QByteArray(geom));
		restoreGeometry(ba);
	}

	updater.reset(CreateQThread([this] {
		while (true) {
			auto cb_sources = [](void *data, obs_source_t *source) {
				if (obs_obj_is_private(source))
					return true;

				auto text = static_cast<QString *>(data);
				BPtr bPerf = obs_source_get_perf_info(source);
				obs_source_perf_t *perf = bPerf.Get();

				if ((obs_source_get_output_flags(source) &
				     OBS_SOURCE_ASYNC_VIDEO) ==
				    OBS_SOURCE_ASYNC_VIDEO) {
					*text += QString::asprintf(
						"tick ms (min/max/avg): %.02f / %.02f / %.02f, "
						"render ms (min/max/avg): %.02f / %.02f / %.02f, "
						"frames (last second/avg): %llu / %.02f - type: %s, name: \"%s\"\n",
						(double)perf->min_tick /
							1000000.0,
						(double)perf->max_tick /
							1000000.0,
						(double)perf->avg_tick /
							1000000.0,
						(double)perf->min_render /
							1000000.0,
						(double)perf->max_render /
							1000000.0,
						(double)perf->avg_render /
							1000000.0,
						perf->frames, perf->avg_fps,
						obs_source_get_id(source),
						obs_source_get_name(source));
				} else {
					*text += QString::asprintf(
						"tick ms (min/max/avg): %.02f / %.02f / %.02f, "
						"render ms (min/max/avg): %.02f / %.02f / %.02f"
						" - type: %s, name: \"%s\"\n",
						(double)perf->min_tick /
							1000000.0,
						(double)perf->max_tick /
							1000000.0,
						(double)perf->avg_tick /
							1000000.0,
						(double)perf->min_render /
							1000000.0,
						(double)perf->max_render /
							1000000.0,
						(double)perf->avg_render /
							1000000.0,
						obs_source_get_id(source),
						obs_source_get_name(source));
				}

				return true;
			};

			QString text;
			obs_enum_all_sources(cb_sources, &text);
			QMetaObject::invokeMethod(this, "Update",
						  Q_ARG(QString, text));
			QThread::sleep(1);
		}
	}));
	updater->start();
}

void OBSPerfViewer::Update(const QString &str)
{
	ui->textArea->clear();
	ui->textArea->insertPlainText(str);
}

OBSPerfViewer::~OBSPerfViewer()
{
	config_set_string(App()->GlobalConfig(), "PerfViewer", "geometry",
			  saveGeometry().toBase64().constData());
	updater->terminate();
}
