#pragma once
#pragma once

#include "ui_roi-editor.h"

#include <obs.hpp>

class RoiListItem;
class QCloseEvent;

struct RoiData {
	std::string scene_uuid;
	int64_t scene_item_id;
	uint32_t posX, posY, width, height;
	float priority;
};
Q_DECLARE_METATYPE(RoiData)

class RoiEditor : public QDialog {
	Q_OBJECT

public:
	std::unique_ptr<Ui_ROIEditor> ui;
	RoiEditor(QWidget *parent);

	void closeEvent(QCloseEvent *event) override;

public slots:
	void ShowHideDialog();
	void ItemSelected(QListWidgetItem *item, QListWidgetItem *);
	void LoadRoisFromOBSData(obs_data_t *obj);
	void SaveRoisToOBSData(obs_data_t *obj);
	void UpdateEncoderRois();
	void ConnectSceneSignals();

	// ToDo event listeners for output start to apply ROI
	// ToDo events for scene item and active scene changes (movement/visibility)
private slots:
	void on_actionAddRoi_triggered();
	void on_actionRemoveRoi_triggered();
	// void on_actionRoiUp_triggered();
	// void on_actionRoiDown_triggered();

	void resizeEvent(QResizeEvent *event);

private:
	void RefreshSceneList();
	void RefreshRoiList();
	void RebuildPreview(bool rebuilData = false);
	void SceneSelectionChanged();
	void RegionItemsToData();
	void RegionsFromData(std::vector<region_of_interest> &rois,
			     const std::string &uuid);
	void PropertiesChanges();

	static void SceneItemTransform(void *param, calldata_t *data);
	static void SceneItemVisibility(void *param, calldata_t *data,
					bool visible);

	// key is scene UUID
	std::unordered_map<std::string, std::vector<OBSDataAutoRelease>>
		roi_data;

	OBSSignal transformSignal;
	OBSSignal visibilitySignal;

	QGraphicsScene *previewScene;
	QPixmap previewPixmap;
	RoiListItem *currentItem = nullptr;
};

enum ROIDataRoles {
	ROIData = Qt::UserRole,
	CenterFocusData,
};

class RoiListItem : public QListWidgetItem {

public:
	enum RoiItemType {
		SceneItem = QListWidgetItem::UserType,
		Manual,
		CenterFocus,
	};

	RoiListItem(QListWidget *parent, int type)
		: QListWidgetItem(parent, type)
	{
	}

	QVariant data(int role) const override;
	void setData(int role, const QVariant &value) override;

private:
	RoiData roi;
};
