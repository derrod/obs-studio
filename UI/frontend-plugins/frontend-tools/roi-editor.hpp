#pragma once

#include <mutex>

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
	int smoothing_type;
	int smoothing_steps;
	float smoothing_priority;
	bool enabled;
	float priority;
};
Q_DECLARE_METATYPE(RoiData)

class RoiEditor : public QDialog {
	Q_OBJECT

	enum Direction { Up, Down };

public:
	enum Smoothing { None, Inside, Outside, Edge };

	std::unique_ptr<Ui_ROIEditor> ui;
	RoiEditor(QWidget *parent);
	~RoiEditor()
	{
		obs_enter_graphics();
		gs_texrender_destroy(texRender);
		gs_samplerstate_destroy(pointSampler);
		gs_vertexbuffer_destroy(rectFill);
		obs_leave_graphics();
	}

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
	void CreateDisplay(bool recreate = false);

	static void SceneItemTransform(void *param, calldata_t *data);
	static void SceneItemVisibility(void *param, calldata_t *data,
					bool visible);
	static void ItemRemovedOrAdded(void *param, calldata_t *data);
	static void DrawPreview(void *data, uint32_t cx, uint32_t cy);
	static void CreatePreviewTexture(RoiEditor *editor, uint32_t cx,
					 uint32_t cy);

	// All signals are added/cleared at once, so just store them in a vector somewhere
	std::vector<OBSSignal> sceneSignals;

	// key is scene UUID
	std::unordered_map<std::string, std::vector<OBSDataAutoRelease>>
		roi_data;

	// Rendering stuff
	std::mutex roi_mutex;
	std::vector<region_of_interest> rois;
	OBSWeakSourceAutoRelease previewSource;

	bool rebuild_texture = false;
	uint32_t texOpacity;
	uint32_t texBlockSize;

	gs_texrender_t *texRender = nullptr;
	gs_samplerstate_t *pointSampler = nullptr;
	gs_vertbuffer_t *rectFill = nullptr;

	// Qt stuff
	RoiListItem *currentItem = nullptr;
	QByteArray geometry;
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
