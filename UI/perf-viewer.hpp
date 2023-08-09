#pragma once

#include <QDialog>
#include <QPlainTextEdit>
#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>

#include "obs-app.hpp"

#include "ui_OBSPerfViewer.h"

class PerfTreeModel;

class OBSPerfViewer : public QDialog {
	Q_OBJECT

	std::unique_ptr<Ui::OBSPerfViewer> ui;
	PerfTreeModel *m_model = nullptr;

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

	enum Columns { NAME, TICK, RENDER, FPS };

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

	PerfTreeItem *child(int row);
	int childCount() const;
	int columnCount() const;
	QVariant data(int column) const;
	int row() const;
	PerfTreeItem *parentItem();

	PerfTreeModel *model() const { return m_model; }

	void update();
	QIcon getIcon() const;

private:
	QList<PerfTreeItem *> m_childItems;
	PerfTreeItem *m_parentItem;
	PerfTreeModel *m_model;

	obs_source_perf_t *m_perf = nullptr;
	OBSSource m_source;
	QString name;

	// ToDo figure out if such a long-term max is even useful
	uint64_t maxRender = 0;
	uint64_t maxTick = 0;
};
