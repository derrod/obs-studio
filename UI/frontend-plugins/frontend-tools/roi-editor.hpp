#pragma once

#include "ui_roi-editor.h"

#include <obs.hpp>

class RoiListItem;
class QCloseEvent;

struct RoiData {
	/* Scene item type*/
	std::string scene_uuid;
	int64_t scene_item_id;
	/* Manual type */
	uint32_t posX, posY, width, height;
	/* Center focus type */
	int64_t inner_radius;
	int64_t inner_steps;
	bool inner_aspect;
	int64_t outer_radius;
	int64_t outer_steps;
	float outer_priority;
	bool outer_aspect;
	/* Shared attributes */
	bool enabled;
	float priority;
};
Q_DECLARE_METATYPE(RoiData)

class RoiEditor : public QDialog {
	Q_OBJECT

	enum Direction { Up, Down };

public:
	std::unique_ptr<Ui_ROIEditor> ui;
	RoiEditor(QWidget *parent);

	void closeEvent(QCloseEvent *event) override;

public slots:
	void ShowHideDialog();
	void LoadRoisFromOBSData(obs_data_t *obj);
	void SaveRoisToOBSData(obs_data_t *obj);
	void ConnectSceneSignals();
	void UpdateEncoderRois();

private slots:
	void on_actionAddRoi_triggered();
	void on_actionRemoveRoi_triggered();
	void on_actionRoiUp_triggered();
	void on_actionRoiDown_triggered();

	void resizeEvent(QResizeEvent *event) override;

	void ItemSelected(QListWidgetItem *item, QListWidgetItem *);

	void RefreshSceneItems(bool signal = true);
	void RebuildPreview(bool rebuildData = false);
	void AddRegionItem(int type);

private:
	void RefreshSceneList();
	void RefreshRoiList();
	void SceneSelectionChanged();
	void RegionItemsToData();
	void RegionsFromData(std::vector<region_of_interest> &rois,
			     const std::string &uuid);
	void PropertiesChanges();
	void MoveRoiItem(Direction direction);

	static void SceneItemTransform(void *param, calldata_t *data);
	static void SceneItemVisibility(void *param, calldata_t *data,
					bool visible);
	static void ItemRemovedOrAdded(void *param, calldata_t *data);

	// key is scene UUID
	std::unordered_map<std::string, std::vector<OBSDataAutoRelease>>
		roi_data;

	// All signals are added/cleared at once, so just store them in a vector somewhere
	std::vector<OBSSignal> obsSignals;

	QGraphicsScene *previewScene;
	QPixmap previewPixmap;
	RoiListItem *currentItem = nullptr;
};

enum ROIDataRoles { ROIData = Qt::UserRole };

class RoiListItem : public QListWidgetItem {

public:
	enum RoiItemType {
		SceneItem = QListWidgetItem::UserType,
		Manual,
		CenterFocus,
	};

	RoiListItem(int type) : QListWidgetItem(nullptr, type) {}

	QVariant data(int role) const override;
	void setData(int role, const QVariant &value) override;

private:
	RoiData roi;
};
