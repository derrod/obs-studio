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
	ui->treeView->setModel(m_model);

	connect(ui->resetButton, &QAbstractButton::clicked, m_model,
		&PerfTreeModel::refreshSources);
	connect(m_model, &PerfTreeModel::modelReset, this,
		&OBSPerfViewer::sourceListUpdated);
	connect(ui->showPrivate, &QAbstractButton::toggled, m_model,
		&PerfTreeModel::setPrivateVisible);

	m_model->refreshSources();
}

OBSPerfViewer::~OBSPerfViewer()
{
	config_set_string(App()->GlobalConfig(), "PerfViewer", "geometry",
			  saveGeometry().toBase64().constData());
	delete m_model;
}

void OBSPerfViewer::sourceListUpdated()
{
	ui->treeView->expandToDepth(0);
	ui->treeView->resizeColumnToContents(0);
	ui->treeView->resizeColumnToContents(1);
	ui->treeView->resizeColumnToContents(2);
}

/* Model */
// TODO: This is just based on the Qt Example and could be trimmed down
PerfTreeModel::PerfTreeModel(QObject *parent) : QAbstractItemModel(parent)
{
	header = {
		QTStr("PerfViewer.Name"),
		QTStr("PerfViewer.Tick"),
		QTStr("PerfViewer.Render"),
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
	parent->appendChild(treeItem);

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

void PerfTreeModel::updateData()
{
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
	  m_source(source),
	  m_model(model)
{
	m_perf = new obs_source_perf_t;
}

/* Fake item (e.g. to group private sources) */
PerfTreeItem::PerfTreeItem(const QString &name, PerfTreeItem *parent,
			   PerfTreeModel *model)
	: m_parentItem(parent),
	  name(name),
	  m_model(model)
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
	// TODO do not hardcode this maybe?
	return 4;
}

static double ns_to_ms(uint64_t ns)
{
	return (double)ns / 1000000.0;
}

static QString GetSourceName(obs_source_t *source)
{
	const char *name = obs_source_get_name(source);
	return name ? name : QTStr("PerfViewer.NoName");
}

constexpr double kRenderWarningThreshold = 1.0;
constexpr double kTickWarningThreshold = 0.1;

static QString GetTickStr(obs_source_perf_t *perf, uint64_t long_term_max)
{
	auto str = QString::asprintf("%.02f ms (%.02f ms)",
				     ns_to_ms(perf->avg_tick),
				     ns_to_ms(long_term_max));

	// TODO custom item delegate to colour text or render HTML
	if (ns_to_ms(perf->max_tick) >= kTickWarningThreshold) {
		str.prepend("! ");
	}

	return str;
}

static QString GetRenderStr(obs_source_perf_t *perf, uint64_t long_term_max)
{
	auto str = QString::asprintf("%.02f ms (%.02f ms)",
				     ns_to_ms(perf->avg_render),
				     ns_to_ms(long_term_max));

	if (ns_to_ms(perf->max_render) >= kRenderWarningThreshold) {
		str.prepend("! ");
	}

	return str;
}

static QString GetSourceFPS(obs_source_perf_t *perf, obs_source_t *source)
{
	if ((obs_source_get_output_flags(source) & OBS_SOURCE_ASYNC_VIDEO) ==
	    OBS_SOURCE_ASYNC_VIDEO) {
		return QString::asprintf("%.02f", perf->avg_fps);
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
		return GetSourceName(m_source);
	case PerfTreeModel::TICK:
		return GetTickStr(m_perf, maxTick);
	case PerfTreeModel::RENDER:
		return GetRenderStr(m_perf, maxRender);
	case PerfTreeModel::FPS:
		return GetSourceFPS(m_perf, m_source);
	default:
		return {};
	}
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
	if (!m_childItems.empty()) {
		for (auto item : m_childItems)
			item->update();
	}

	if (m_source) {
		obs_source_fill_perf_info(m_source, m_perf);
		/* Update lifetime max values */
		if (m_perf->max_render > maxRender)
			maxRender = m_perf->max_render;
		if (m_perf->max_tick > maxTick)
			maxTick = m_perf->max_tick;

		if (m_model)
			m_model->itemChanged(this);
	}
}

QIcon PerfTreeItem::getIcon() const
{
	// ToDo icons for root?
	if (!m_source)
		return {};

	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	const char *id = obs_source_get_id(m_source);

	if (strcmp(id, "scene") == 0)
		return main->GetSceneIcon();
	else if (strcmp(id, "group") == 0)
		return main->GetGroupIcon();
	else
		return main->GetSourceIcon(id);
}
