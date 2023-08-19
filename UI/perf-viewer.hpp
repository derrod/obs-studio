#pragma once

#include <QDialog>
#include <QAbstractItemModel>
#include <QVariant>
#include <QSortFilterProxyModel>

#include "util/source-profiler.h"

#include "ui_OBSPerfViewer.h"

class PerfTreeModel;
class PerfViewerProxyModel;

class OBSPerfViewer : public QDialog {
	Q_OBJECT

	std::unique_ptr<Ui::OBSPerfViewer> ui;
	PerfTreeModel *m_model = nullptr;
	PerfViewerProxyModel *m_proxy = nullptr;

public:
	OBSPerfViewer(QWidget *parent = 0);
	~OBSPerfViewer() override;

public slots:
	void sourceListUpdated();
};

class PerfTreeItem;

class PerfTreeModel : public QAbstractItemModel {
	Q_OBJECT

public:
	explicit PerfTreeModel(QObject *parent = nullptr);
	~PerfTreeModel() override;

	QVariant data(const QModelIndex &index, int role) const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;
	QVariant headerData(int section, Qt::Orientation orientation,
			    int role = Qt::DisplayRole) const override;
	QModelIndex
	index(int row, int column,
	      const QModelIndex &parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex &index) const override;
	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int
	columnCount(const QModelIndex &parent = QModelIndex()) const override;
	void itemChanged(PerfTreeItem *item);

	void setPrivateVisible(bool visible = false)
	{
		showPrivateSources = visible;
		refreshSources();
	}

	double targetFrameTime() const { return frameTime; }

	enum Columns {
		NAME,
		TICK,
		RENDER,
#ifndef __APPLE__
		RENDER_GPU,
#endif
		FPS,
		NUM_COLUMNS
	};

public slots:
	void refreshSources();

private slots:
	void updateData();

private:
	PerfTreeItem *rootItem = nullptr;
	QList<QVariant> header;
	std::unique_ptr<QThread> updater;

	bool showPrivateSources = false;
	bool refreshing = false;
	double frameTime = 0.0;
};

class PerfTreeItem {
public:
	explicit PerfTreeItem(obs_source_t *source,
			      PerfTreeItem *parentItem = nullptr,
			      PerfTreeModel *model = nullptr);
	explicit PerfTreeItem(const QString &name,
			      PerfTreeItem *parentItem = nullptr,
			      PerfTreeModel *model = nullptr);
	~PerfTreeItem();

	void appendChild(PerfTreeItem *child);
	void prependChild(PerfTreeItem *child);

	PerfTreeItem *child(int row);
	int childCount() const;
	int columnCount() const;
	QVariant data(int column) const;
	QVariant rawData(int column) const;
	int row() const;
	PerfTreeItem *parentItem();

	PerfTreeModel *model() const { return m_model; }

	void update();
	QIcon getIcon() const;
	QVariant getTextColour(int column) const;

	bool isRendered() const { return rendered; }

private:
	QList<PerfTreeItem *> m_childItems;
	PerfTreeItem *m_parentItem;
	PerfTreeModel *m_model;

	profiler_result_t *m_perf = nullptr;
	OBSSource m_source;
	QString name;
	bool rendered;
};

class PerfViewerProxyModel : public QSortFilterProxyModel {
	Q_OBJECT

public:
	PerfViewerProxyModel(QObject *parent = nullptr)
		: QSortFilterProxyModel(parent)
	{
		setRecursiveFilteringEnabled(true);
	}

public slots:
	void setFilterText(const QString &filter);

protected:
	bool filterAcceptsRow(int sourceRow,
			      const QModelIndex &sourceParent) const override;
	bool lessThan(const QModelIndex &left,
		      const QModelIndex &right) const override;
};
