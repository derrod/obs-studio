#include <QFile>
#include <QTextStream>
#include <QScrollBar>
#include <QFont>

#include "window-basic-main.hpp"
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

	m_model = new PerfTreeModel(this);

	m_proxy = new PerfViewerProxyModel(this);
	m_proxy->setSourceModel(m_model);

	ui->treeView->setModel(m_proxy);
	ui->treeView->setSortingEnabled(true);
	ui->treeView->sortByColumn(PerfTreeModel::NAME, Qt::AscendingOrder);
	ui->treeView->setAlternatingRowColors(true);

	connect(ui->resetButton, &QAbstractButton::clicked, m_model,
		&PerfTreeModel::refreshSources);
	connect(m_model, &PerfTreeModel::modelReset, this,
		&OBSPerfViewer::sourceListUpdated);
	connect(ui->showPrivate, &QAbstractButton::toggled, m_model,
		&PerfTreeModel::setPrivateVisible);
	connect(ui->searchBox, &QLineEdit::textChanged, this,
		[&](const QString &text) {
			m_proxy->setFilterText(text);
			ui->treeView->expandAll();
		});

	source_profiler_enable(true);
#ifndef __APPLE__
	source_profiler_gpu_enable(true);
#endif
	m_model->refreshSources();
}

OBSPerfViewer::~OBSPerfViewer()
{
	config_set_string(App()->GlobalConfig(), "PerfViewer", "geometry",
			  saveGeometry().toBase64().constData());
	delete m_model;
	source_profiler_enable(false);
}

void OBSPerfViewer::sourceListUpdated()
{
	ui->treeView->expandAll();
	if (loaded)
		return;

	ui->treeView->resizeColumnToContents(0);
	ui->treeView->resizeColumnToContents(1);
	ui->treeView->resizeColumnToContents(2);
	ui->treeView->resizeColumnToContents(3);
#ifndef __APPLE__
	ui->treeView->resizeColumnToContents(4);
#endif
	loaded = true;
}

/* Model */
// TODO: This is just based on the Qt Example and could be trimmed down
PerfTreeModel::PerfTreeModel(QObject *parent) : QAbstractItemModel(parent)
{
	header = {
		QTStr("PerfViewer.Name"),      QTStr("PerfViewer.Tick"),
		QTStr("PerfViewer.Render"),
#ifndef __APPLE__
		QTStr("PerfViewer.RenderGPU"),
#endif
		QTStr("PerfViewer.FPS"),
	};

	updater.reset(CreateQThread([this] {
		while (true) {
			QThread::sleep(1);
			updateData();
		}
	}));

	updater->start();
}

PerfTreeModel::~PerfTreeModel()
{
	if (updater)
		updater->terminate();

	delete rootItem;
}
int PerfTreeModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return static_cast<PerfTreeItem *>(parent.internalPointer())
			->columnCount();
	return (int)header.count();
}

QVariant PerfTreeModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())
		return {};

	if (role == Qt::DisplayRole) {
		auto item =
			static_cast<PerfTreeItem *>(index.internalPointer());
		return item->data(index.column());
	} else if (role == Qt::DecorationRole) {
		if (index.column() != PerfTreeModel::NAME)
			return {};

		auto item =
			static_cast<PerfTreeItem *>(index.internalPointer());
		return item->getIcon();
	} else if (role == Qt::ForegroundRole) {
		if (index.column() != PerfTreeModel::TICK &&
		    index.column() != PerfTreeModel::RENDER
#ifndef __APPLE__
		    && index.column() != PerfTreeModel::RENDER_GPU
#endif
		)
			return {};

		auto item =
			static_cast<PerfTreeItem *>(index.internalPointer());
		return item->getTextColour(index.column());
	} else if (role == SortRole) {
		auto item =
			static_cast<PerfTreeItem *>(index.internalPointer());
		return item->sortData(index.column());
	} else if (role == RawDataRole) {
		auto item =
			static_cast<PerfTreeItem *>(index.internalPointer());
		return item->rawData(index.column());
	}

	return {};
}

Qt::ItemFlags PerfTreeModel::flags(const QModelIndex &index) const
{
	if (!index.isValid())
		return Qt::NoItemFlags;

	return QAbstractItemModel::flags(index);
}

QVariant PerfTreeModel::headerData(int section, Qt::Orientation orientation,
				   int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
		return header.at(section);
	}

	return {};
}

QModelIndex PerfTreeModel::index(int row, int column,
				 const QModelIndex &parent) const
{
	if (!hasIndex(row, column, parent))
		return {};

	PerfTreeItem *parentItem;

	if (!parent.isValid())
		parentItem = rootItem;
	else
		parentItem =
			static_cast<PerfTreeItem *>(parent.internalPointer());

	if (auto childItem = parentItem->child(row))
		return createIndex(row, column, childItem);

	return {};
}

QModelIndex PerfTreeModel::parent(const QModelIndex &index) const
{
	if (!index.isValid())
		return {};

	auto childItem = static_cast<PerfTreeItem *>(index.internalPointer());
	auto parentItem = childItem->parentItem();

	if (parentItem == rootItem)
		return {};

	return createIndex(parentItem->row(), 0, parentItem);
}

int PerfTreeModel::rowCount(const QModelIndex &parent) const
{
	PerfTreeItem *parentItem;
	if (parent.column() > 0)
		return 0;

	if (!parent.isValid())
		parentItem = rootItem;
	else
		parentItem =
			static_cast<PerfTreeItem *>(parent.internalPointer());

	if (!parentItem)
		return 0;

	return parentItem->childCount();
}

static bool EnumSource(void *data, obs_source_t *source);

static void EnumFilter(obs_source_t *, obs_source_t *child, void *data)
{
	EnumSource(data, child);
}

static bool EnumSceneItem(obs_scene_t *, obs_sceneitem_t *item, void *data)
{
	auto parent = static_cast<PerfTreeItem *>(data);
	obs_source_t *source = obs_sceneitem_get_source(item);

	auto treeItem = new PerfTreeItem(source, parent, parent->model());
	parent->prependChild(treeItem);

	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(item);
		obs_scene_enum_items(scene, EnumSceneItem, treeItem);
	}

	if (obs_source_filter_count(source) > 0) {
		obs_source_enum_filters(source, EnumFilter, treeItem);
	}

	return true;
}

static bool EnumSource(void *data, obs_source_t *source)
{
	auto root = static_cast<PerfTreeItem *>(data);
	auto item = new PerfTreeItem(source, root, root->model());
	root->appendChild(item);

	if (obs_scene_t *scene = obs_scene_from_source(source)) {
		obs_scene_enum_items(scene, EnumSceneItem, item);
	}

	return true;
}

static bool EnumScene(void *data, obs_source_t *source)
{
	if (obs_source_is_group(source))
		return true;

	return EnumSource(data, source);
}

static bool EnumPrivateSource(void *data, obs_source_t *source)
{
	if (!obs_obj_is_private(source))
		return true;
	if (obs_source_get_type(source) == OBS_SOURCE_TYPE_FILTER)
		return true;

	return EnumSource(data, source);
}

void PerfTreeModel::refreshSources()
{
	if (refreshing)
		return;

	refreshing = true;
	beginResetModel();

	delete rootItem;
	rootItem = new PerfTreeItem(nullptr, nullptr, this);

	/* Enum scenes and their sources */
	obs_enum_scenes(EnumScene, rootItem);

	if (showPrivateSources) {
		auto privateRoot = new PerfTreeItem(QTStr("PerfViewer.Private"),
						    rootItem, this);
		rootItem->appendChild(privateRoot);
		obs_enum_all_sources(EnumPrivateSource, privateRoot);
	}

	endResetModel();
	refreshing = false;
}

static double ns_to_ms(uint64_t ns)
{
	return (double)ns / 1000000.0;
}

void PerfTreeModel::updateData()
{
	// Set target frame time in ms
	frameTime = ns_to_ms(obs_get_frame_interval_ns());

	if (rootItem)
		rootItem->update();
}

void PerfTreeModel::itemChanged(PerfTreeItem *item)
{
	QModelIndex left = createIndex(item->row(), 0, item);
	QModelIndex right =
		createIndex(item->row(), item->columnCount() - 1, item);
	emit dataChanged(left, right);
}

/* Item */
PerfTreeItem::PerfTreeItem(obs_source_t *source, PerfTreeItem *parent,
			   PerfTreeModel *model)
	: m_parentItem(parent),
	  m_model(model),
	  m_source(source)
{
	m_perf = new profiler_result_t;
	memset(m_perf, 0, sizeof(profiler_result_t));
}

/* Fake item (e.g. to group private sources) */
PerfTreeItem::PerfTreeItem(const QString &name, PerfTreeItem *parent,
			   PerfTreeModel *model)
	: m_parentItem(parent),
	  m_model(model),
	  name(name)
{
	m_perf = nullptr;
}

PerfTreeItem::~PerfTreeItem()
{
	delete m_perf;
	qDeleteAll(m_childItems);
}

void PerfTreeItem::appendChild(PerfTreeItem *item)
{
	m_childItems.append(item);
}

void PerfTreeItem::prependChild(PerfTreeItem *item)
{
	m_childItems.prepend(item);
}

PerfTreeItem *PerfTreeItem::child(int row)
{
	if (row < 0 || row >= m_childItems.size())
		return nullptr;
	return m_childItems.at(row);
}

int PerfTreeItem::childCount() const
{
	return m_childItems.count();
}

int PerfTreeItem::columnCount() const
{
	return PerfTreeModel::Columns::NUM_COLUMNS;
}

static QString GetSourceName(obs_source_t *source, bool rendered)
{
	const char *name = obs_source_get_name(source);
	QString qName = name ? name : QTStr("PerfViewer.NoName");
#ifdef _DEBUG
	qName += " (";
	qName += obs_source_get_id(source);
	qName += ")";
#endif

	if (!rendered)
		qName += " " + QTStr("PerfViewer.Inactive");
	return qName;
}

static QString GetTickStr(profiler_result_t *perf)
{
	return QString::asprintf("%.02f / %.02f", ns_to_ms(perf->tick_avg),
				 ns_to_ms(perf->tick_max));
}

static QString GetRenderStr(profiler_result_t *perf)
{
	return QString::asprintf("%.02f / %.02f / %0.02f",
				 ns_to_ms(perf->render_avg),
				 ns_to_ms(perf->render_max),
				 ns_to_ms(perf->render_sum));
}

#ifndef __APPLE__
static QString GetRenderGPUStr(profiler_result_t *perf)
{
	return QString::asprintf("%.02f / %.02f / %0.02f",
				 ns_to_ms(perf->render_gpu_avg),
				 ns_to_ms(perf->render_gpu_max),
				 ns_to_ms(perf->render_gpu_sum));
}
#endif

static QString GetSourceFPS(profiler_result_t *perf, obs_source_t *source)
{
	if (obs_source_get_type(source) != OBS_SOURCE_TYPE_FILTER &&
	    (obs_source_get_output_flags(source) & OBS_SOURCE_ASYNC_VIDEO) ==
		    OBS_SOURCE_ASYNC_VIDEO) {
		return QString::asprintf("%.02f", perf->async_fps);
	} else {
		return QTStr("PerfViewer.NA");
	}
}

QVariant PerfTreeItem::data(int column) const
{
	// Fake items have a name but nothing else
	if (!name.isEmpty()) {
		if (column == 0)
			return name;
		return {};
	}

	// Root item has no data or if perf is null we can't return anything
	if (!m_source || !m_perf)
		return {};

	switch (column) {
	case PerfTreeModel::NAME:
		return GetSourceName(m_source, rendered);
	case PerfTreeModel::TICK:
		return GetTickStr(m_perf);
	case PerfTreeModel::RENDER:
		return GetRenderStr(m_perf);
#ifndef __APPLE__
	case PerfTreeModel::RENDER_GPU:
		return GetRenderGPUStr(m_perf);
#endif
	case PerfTreeModel::FPS:
		return GetSourceFPS(m_perf, m_source);
	default:
		return {};
	}
}

QVariant PerfTreeItem::rawData(int column) const
{
	switch (column) {
	case PerfTreeModel::NAME:
		return m_source ? QString(obs_source_get_name(m_source)) : name;
	case PerfTreeModel::TICK:
		return (qulonglong)m_perf->tick_max;
	case PerfTreeModel::RENDER:
		return (qulonglong)m_perf->render_max;
#ifndef __APPLE__
	case PerfTreeModel::RENDER_GPU:
		return (qulonglong)m_perf->render_gpu_max;
#endif
	case PerfTreeModel::FPS:
		return m_perf->async_fps;
	default:
		return {};
	}
}

QVariant PerfTreeItem::sortData(int column) const
{
	if (column == PerfTreeModel::NAME)
		return row();

	return rawData(column);
}

PerfTreeItem *PerfTreeItem::parentItem()
{
	return m_parentItem;
}

int PerfTreeItem::row() const
{
	if (m_parentItem)
		return m_parentItem->m_childItems.indexOf(
			const_cast<PerfTreeItem *>(this));

	return 0;
}

void PerfTreeItem::update()
{
	if (m_source) {
		if (obs_source_get_type(m_source) == OBS_SOURCE_TYPE_FILTER)
			rendered = m_parentItem->isRendered();
		else
			rendered = obs_source_showing(m_source);

		if (rendered)
			source_profiler_fill_result(m_source, m_perf);
		else
			memset(m_perf, 0, sizeof(profiler_result_t));

		if (m_model)
			m_model->itemChanged(this);
	}

	if (!m_childItems.empty()) {
		for (auto item : m_childItems)
			item->update();
	}
}

QIcon PerfTreeItem::getIcon() const
{
	// ToDo icons for root?
	if (!m_source)
		return {};

	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	const char *id = obs_source_get_id(m_source);

	// Todo filter icon from source toolbar
	if (strcmp(id, "scene") == 0)
		return main->GetSceneIcon();
	else if (strcmp(id, "group") == 0)
		return main->GetGroupIcon();
	else if (obs_source_get_type(m_source) == OBS_SOURCE_TYPE_FILTER)
		return main->GetFilterIcon();
	else
		return main->GetSourceIcon(id);
}

QVariant PerfTreeItem::getTextColour(int column) const
{
	// root/fake elements don't have performance data
	if (!m_perf)
		return {};

	double target = m_model->targetFrameTime();
	double frameTime;

	switch (column) {
	case PerfTreeModel::TICK:
		frameTime = ns_to_ms(m_perf->tick_max);
		break;
	case PerfTreeModel::RENDER:
		frameTime = ns_to_ms(m_perf->render_max);
		break;
#ifndef __APPLE__
	case PerfTreeModel::RENDER_GPU:
		frameTime = ns_to_ms(m_perf->render_gpu_max);
		break;
#endif
	default:
		return {};
	}

	if (frameTime >= target)
		return QBrush(Qt::red);
	if (frameTime >= target * 0.5)
		return QBrush(Qt::yellow);
	if (frameTime >= target * 0.25)
		return QBrush(Qt::darkYellow);

	return {};
}

void PerfViewerProxyModel::setFilterText(const QString &filter)
{
	QRegularExpression regex(filter,
				 QRegularExpression::CaseInsensitiveOption);
	setFilterRegularExpression(regex);
}

bool PerfViewerProxyModel::filterAcceptsRow(
	int sourceRow, const QModelIndex &sourceParent) const
{
	QModelIndex itemIndex =
		sourceModel()->index(sourceRow, 0, sourceParent);

	auto name = sourceModel()
			    ->data(itemIndex, PerfTreeModel::RawDataRole)
			    .toString();
	return name.contains(filterRegularExpression());
}

bool PerfViewerProxyModel::lessThan(const QModelIndex &left,
				    const QModelIndex &right) const
{
	QVariant leftData = sourceModel()->data(left, PerfTreeModel::SortRole);
	QVariant rightData =
		sourceModel()->data(right, PerfTreeModel::SortRole);

	if (leftData.userType() == QMetaType::Double)
		return leftData.toDouble() < rightData.toDouble();
	if (leftData.userType() == QMetaType::ULongLong)
		return leftData.toULongLong() < rightData.toULongLong();
	if (leftData.userType() == QMetaType::Int)
		return leftData.toInt() < rightData.toInt();
	if (leftData.userType() == QMetaType::QString)
		return QString::localeAwareCompare(leftData.toString(),
						   rightData.toString()) < 0;

	return false;
}
