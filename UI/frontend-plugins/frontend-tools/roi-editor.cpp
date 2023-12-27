#include "roi-editor.hpp"

#include "qt-wrappers.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <graphics/matrix4.h>
#include <util/profiler.hpp>

#include <QAction>
#include <QMainWindow>
#include <QObject>
#include <QMenu>

using namespace std;

RoiEditor *roi_edit;

RoiEditor::RoiEditor(QWidget *parent)
	: QDialog(parent),
	  ui(new Ui_ROIEditor),
	  previewScene(new QGraphicsScene)
{
	ui->setupUi(this);

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	// Hide properties until needed
	ui->roiPropertiesGroupBox->setVisible(false);

	// Fill block size list
	ui->roiBlockSize->addItem(obs_module_text("ROI.BlockSize.16"), 16);
	ui->roiBlockSize->addItem(obs_module_text("ROI.BlockSize.32"), 32);
	ui->roiBlockSize->addItem(obs_module_text("ROI.BlockSize.64"), 64);
	ui->roiBlockSize->addItem(obs_module_text("ROI.BlockSize.128"), 128);

	connect(ui->close, &QPushButton::clicked, this, &RoiEditor::close);

	connect(ui->roiList, &QListWidget::currentItemChanged, this,
		&RoiEditor::ItemSelected);

	connect(ui->roiBlockSize, &QComboBox::currentIndexChanged, this,
		&RoiEditor::RebuildPreview);
	connect(ui->sceneSelect, &QComboBox::currentIndexChanged, this,
		&RoiEditor::SceneSelectionChanged);
	connect(ui->enableRoi, &QCheckBox::stateChanged, this,
		&RoiEditor::UpdateEncoderRois);

	connect(ui->roiPropPosX, &QSpinBox::valueChanged, this,
		&RoiEditor::PropertiesChanges);
	connect(ui->roiPropPosY, &QSpinBox::valueChanged, this,
		&RoiEditor::PropertiesChanges);
	connect(ui->roiPropSizeX, &QSpinBox::valueChanged, this,
		&RoiEditor::PropertiesChanges);
	connect(ui->roiPropSizeY, &QSpinBox::valueChanged, this,
		&RoiEditor::PropertiesChanges);
	connect(ui->prioritySlider, &QSlider::valueChanged, this,
		&RoiEditor::PropertiesChanges);
	connect(ui->roiPropSceneItem, &QComboBox::currentIndexChanged, this,
		&RoiEditor::PropertiesChanges);
}

void RoiEditor::closeEvent(QCloseEvent *)
{
	obs_frontend_save();
}

void RoiEditor::ShowHideDialog()
{
	if (!isVisible()) {
		setVisible(true);
		RefreshSceneList();
		RefreshRoiList();
		RebuildPreview(true);
	} else {
		setVisible(false);
	}
}

void RoiEditor::PropertiesChanges()
{
	if (!currentItem)
		return;

	auto sceneVar = ui->sceneSelect->currentData();

	RoiData data = {};
	data.priority = (float)ui->prioritySlider->value() / 100.0f;
	data.scene_uuid = sceneVar.toString().toStdString();

	if (currentItem->type() == RoiListItem::SceneItem) {
		data.scene_item_id =
			ui->roiPropSceneItem->currentData().toLongLong();
	} else {
		data.posX = ui->roiPropPosX->value();
		data.posY = ui->roiPropPosY->value();
		data.width = ui->roiPropSizeX->value();
		data.height = ui->roiPropSizeY->value();
		blog(LOG_DEBUG, "Data changed: %d %d %d %d", data.posX,
		     data.posY, data.width, data.height);
	}

	currentItem->setData(ROIData, QVariant::fromValue<RoiData>(data));
	RebuildPreview(true);
}

void RoiEditor::ItemSelected(QListWidgetItem *item, QListWidgetItem *)
{
	currentItem = dynamic_cast<RoiListItem *>(item);
	if (!currentItem) {
		ui->roiPropertiesGroupBox->setVisible(false);
		return;
	}

	ui->roiPropertiesGroupBox->setVisible(true);

	auto sceneVar = ui->sceneSelect->currentData();
	const string scene_uuid = sceneVar.toString().toStdString();
	OBSSourceAutoRelease source =
		obs_get_source_by_uuid(scene_uuid.c_str());

	bool manual = false;
	bool sceneitem = false;
	bool center = false;

	QVariant var = item->data(ROIData);
	RoiData data = var.value<RoiData>();

	if (item->type() == RoiListItem::SceneItem) {
		sceneitem = true;
		QSignalBlocker sb(ui->roiPropSceneItem);
		ui->roiPropSceneItem->clear();

		if (source) {
			obs_scene_enum_items(
				obs_scene_from_source(source),
				[](obs_scene_t *, obs_sceneitem_t *item,
				   void *param) -> bool {
					auto cb =
						static_cast<QComboBox *>(param);
					cb->addItem(
						obs_source_get_name(
							obs_sceneitem_get_source(
								item)),
						obs_sceneitem_get_id(item));
					return true;
				},
				ui->roiPropSceneItem);
		}

		int idx = ui->roiPropSceneItem->findData(data.scene_item_id);
		if (idx != -1)
			ui->roiPropSceneItem->setCurrentIndex(idx);

		ui->prioritySlider->setValue((int)(100 * data.priority));
	} else if (item->type() == RoiListItem::Manual) {
		manual = true;
		ui->roiPropPosX->setValue(data.posX);
		ui->roiPropPosY->setValue(data.posY);
		ui->roiPropSizeX->setValue(data.width);
		ui->roiPropSizeY->setValue(data.height);
		ui->prioritySlider->setValue((int)(100 * data.priority));
	}

	ui->roiPropPosX->setVisible(manual);
	ui->roiPropPosY->setVisible(manual);
	ui->roiPropPosLabel->setVisible(manual);

	ui->roiPropSizeLabel->setVisible(manual);
	ui->roiPropSizeX->setVisible(manual);
	ui->roiPropSizeY->setVisible(manual);

	ui->roiPropSceneItem->setVisible(sceneitem);
	ui->roiPropSceneItemLabel->setVisible(sceneitem);
	ui->priorityLabel->setVisible(!center);
	ui->prioritySlider->setVisible(!center);

	// ToDo CenterFocus
}

void RoiEditor::RefreshSceneList()
{
	QVariant var = ui->sceneSelect->currentData();
	ui->sceneSelect->clear();

	obs_enum_scenes(
		[](void *param, obs_source_t *src) -> bool {
			auto cb = static_cast<QComboBox *>(param);
			cb->addItem(obs_source_get_name(src),
				    obs_source_get_uuid(src));
			return true;
		},
		ui->sceneSelect);

	if (var.isValid()) {
		QSignalBlocker sb(ui->sceneSelect);
		int idx = ui->sceneSelect->findData(var);
		ui->sceneSelect->setCurrentIndex(idx);
	}
}

static region_of_interest GetItemROI(obs_sceneitem_t *item, float priority)
{
	region_of_interest roi;

	matrix4 boxTransform;
	obs_sceneitem_get_box_transform(item, &boxTransform);

	vec3 tl, br;
	vec3_set(&tl, M_INFINITE, M_INFINITE, 0.0f);
	vec3_set(&br, -M_INFINITE, -M_INFINITE, 0.0f);

	auto GetMinPos = [&](float x, float y) {
		vec3 pos;
		vec3_set(&pos, x, y, 0.0f);
		vec3_transform(&pos, &pos, &boxTransform);
		vec3_min(&tl, &tl, &pos);
		vec3_max(&br, &br, &pos);
	};

	GetMinPos(0.0f, 0.0f);
	GetMinPos(1.0f, 0.0f);
	GetMinPos(0.0f, 1.0f);
	GetMinPos(1.0f, 1.0f);

	roi.left = static_cast<uint32_t>(std::max(tl.x, 0.0f));
	roi.top = static_cast<uint32_t>(std::max(tl.y, 0.0f));
	roi.right = static_cast<uint32_t>(std::max(br.x, 0.0f));
	roi.bottom = static_cast<uint32_t>(std::max(br.y, 0.0f));
	roi.priority = priority;

	return roi;
}

/// Builds the list of actual regions of interest that will be passed to OBS
void RoiEditor::RegionItemsToData()
{
	auto var = ui->sceneSelect->currentData();
	if (!var.isValid())
		return;

	const string scene_uuid = var.toString().toStdString();
	roi_data[scene_uuid].clear();

	int count = ui->roiList->count();
	for (int idx = 0; idx < count; idx++) {
		auto item = dynamic_cast<RoiListItem *>(ui->roiList->item(idx));
		if (!item)
			continue;

		if (item->type() == RoiListItem::SceneItem ||
		    item->type() == RoiListItem::Manual) {
			auto var = item->data(ROIData);
			auto roi = var.value<RoiData>();

			obs_data_t *data = obs_data_create();
			obs_data_set_double(data, "priority", roi.priority);

			if (item->type() == RoiListItem::SceneItem) {
				obs_data_set_int(data, "scene_item_id",
						 roi.scene_item_id);
			} else {
				obs_data_set_int(data, "x", roi.posX);
				obs_data_set_int(data, "y", roi.posY);
				obs_data_set_int(data, "width", roi.width);
				obs_data_set_int(data, "height", roi.height);
			}

			roi_data[scene_uuid].emplace_back(data);
		}
		// ToDo RoiListItem::CenterFocus / MultiROIData
	}
}

void RoiEditor::RegionsFromData(vector<region_of_interest> &rois,
				const string &uuid)
{
	const auto &regions = roi_data[uuid];
	if (regions.empty())
		return;
	OBSSourceAutoRelease source = obs_get_source_by_uuid(uuid.c_str());

	for (obs_data_t *data : regions) {
		float priority = obs_data_get_double(data, "priority");

		if (obs_data_has_user_value(data, "scene_item_id")) {
			/* Scene Item ROI */
			uint64_t id = obs_data_get_int(data, "scene_item_id");
			OBSSceneItem sceneItem = obs_scene_find_sceneitem_by_id(
				obs_scene_from_source(source), id);

			if (sceneItem && obs_sceneitem_visible(sceneItem)) {
				auto roi = GetItemROI(sceneItem, priority);
				if (roi.bottom != 0 && roi.right != 0)
					rois.push_back(roi);
			}

		} else if (obs_data_has_user_value(data, "x")) {
			/* Fixed ROI */
			uint32_t left = (uint32_t)obs_data_get_int(data, "x");
			uint32_t top = (uint32_t)obs_data_get_int(data, "y");
			uint32_t right = left + (uint32_t)obs_data_get_int(
							data, "width");
			uint32_t bottom = top + (uint32_t)obs_data_get_int(
							data, "height");

			// Invalid ROI
			if (right == 0 || bottom == 0)
				continue;

			region_of_interest roi{top, bottom, left, right,
					       priority};
			rois.push_back(roi);
		} else if (obs_data_has_user_value(data, "radius")) {
			/* Center-focus ROI */
			// ToDo
		}
	}
}

static void DrawROI(QPainter *painter, const region_of_interest *roi,
		    const uint32_t blockSize)
{
	const uint32_t roi_left = roi->left / blockSize;
	const uint32_t roi_top = roi->top / blockSize;
	const uint32_t roi_right = (roi->right - 1) / blockSize;
	const uint32_t roi_bottom = (roi->bottom - 1) / blockSize;

	QPoint tl(roi_left, roi_top);
	QPoint br(roi_right, roi_bottom);
	QRect rect(tl, br);
	QColor colour(Qt::black);

	if (roi->priority > 0.0f)
		colour.setGreenF(roi->priority);
	else if (roi->priority < 0.0f)
		colour.setRedF(-roi->priority);

	colour.setBlueF(std::max(0.5f - std::abs(roi->priority), 0.0f));

	painter->setBrush(colour);
	painter->drawRect(rect);
}

void RoiEditor::RebuildPreview(bool rebuildData)
{
	if (!isVisible())
		return;
	const uint32_t blockSize = ui->roiBlockSize->currentData().toInt();
	if (!blockSize)
		return;
	auto var = ui->sceneSelect->currentData();
	if (!var.isValid())
		return;

	const string scene_uuid = var.toString().toStdString();
	if (rebuildData)
		RegionItemsToData();

	vector<region_of_interest> rois;
	RegionsFromData(rois, scene_uuid);

	obs_video_info ovi;
	obs_get_video_info(&ovi);

	uint32_t blocks_w = (ovi.base_width + (blockSize - 1)) / blockSize;
	uint32_t blocks_h = (ovi.base_height + (blockSize - 1)) / blockSize;

	// Draw new pixmap
	previewPixmap = QPixmap(blocks_w, blocks_h);
	previewPixmap.fill(Qt::black);
	QPainter paint(&previewPixmap);
	paint.setPen(Qt::PenStyle::NoPen);

	for (auto it = rois.rbegin(); it != rois.rend(); ++it) {
		const region_of_interest &roi = *it;
		DrawROI(&paint, &roi, blockSize);
	}

	previewScene->clear();
	previewScene->addPixmap(previewPixmap);
	previewScene->setSceneRect(previewPixmap.rect());

	ui->preview->setScene(previewScene);
	ui->preview->fitInView(ui->preview->scene()->sceneRect(),
			       Qt::KeepAspectRatio);
}

void RoiEditor::SceneItemTransform(void *param, calldata_t *)
{
	RoiEditor *window = reinterpret_cast<RoiEditor *>(param);
	window->RebuildPreview();
	window->UpdateEncoderRois();
}

void RoiEditor::SceneItemVisibility(void *param, calldata_t *, bool)
{
	RoiEditor *window = reinterpret_cast<RoiEditor *>(param);
	window->RebuildPreview();
	window->UpdateEncoderRois();
}

void RoiEditor::ConnectSceneSignals()
{
	OBSSourceAutoRelease source = obs_frontend_get_current_scene();
	signal_handler_t *signal = obs_source_get_signal_handler(source);
	transformSignal.Connect(signal, "item_transform", SceneItemTransform,
				this);
	visibilitySignal.Connect(signal, "item_visible", SceneItemTransform,
				 this);
}

void RoiEditor::RefreshRoiList()
{
	auto var = ui->sceneSelect->currentData();
	if (!var.isValid())
		return;

	ui->roiList->clear();

	const string scene_uuid = var.toString().toStdString();
	for (obs_data_t *roi : roi_data[scene_uuid]) {
		int type = obs_data_has_user_value(roi, "scene_item_id")
				   ? RoiListItem::SceneItem
				   : RoiListItem::Manual;

		RoiData data = {
			scene_uuid,
			obs_data_get_int(roi, "scene_item_id"),
			(uint32_t)obs_data_get_int(roi, "x"),
			(uint32_t)obs_data_get_int(roi, "y"),
			(uint32_t)obs_data_get_int(roi, "width"),
			(uint32_t)obs_data_get_int(roi, "height"),
			(float)obs_data_get_double(roi, "priority"),
		};

		RoiListItem *item = new RoiListItem(ui->roiList, type);
		ui->roiList->addItem(item);
		item->setData(ROIData, QVariant::fromValue<RoiData>(data));
	}
}

void RoiEditor::SceneSelectionChanged()
{
	RefreshRoiList();
	RebuildPreview();
	// ToDo refresh list of actual items
}

void RoiEditor::LoadRoisFromOBSData(obs_data_t *obj)
{
	ui->enableRoi->setChecked(obs_data_get_bool(obj, "enabled"));
	OBSDataAutoRelease scenes = obs_data_get_obj(obj, "scenes");
	obs_data_item *item = obs_data_first(scenes);

	roi_data.clear();

	while (item) {
		const char *uuid = obs_data_item_get_name(item);

		OBSDataArrayAutoRelease arr = obs_data_item_get_array(item);
		size_t count = obs_data_array_count(arr);

		for (size_t idx = 0; idx < count; idx++) {
			roi_data[uuid].emplace_back(
				obs_data_array_item(arr, idx));
		}

		obs_data_item_next(&item);
	}
}

void RoiEditor::SaveRoisToOBSData(obs_data_t *obj)
{
	OBSDataAutoRelease scenes = obs_data_create();

	for (const auto &item : roi_data) {
		obs_data_array_t *scene = obs_data_array_create();

		for (const auto &roi : item.second)
			obs_data_array_push_back(scene, roi);

		obs_data_set_array(scenes, item.first.c_str(), scene);
		obs_data_array_release(scene);
	}

	obs_data_set_obj(obj, "scenes", scenes);
	obs_data_set_bool(obj, "enabled", ui->enableRoi->isChecked());
}

void RoiEditor::UpdateEncoderRois()
{
	ProfileScope("RoiEditor::UpdateEncoderRois");

	OBSSourceAutoRelease scene = obs_frontend_get_current_scene();
	const string uuid = obs_source_get_uuid(scene);

	std::vector<OBSEncoder> encoders;

	// Find all video encoders that could reasonably be in use
	for (OBSOutputAutoRelease output :
	     {obs_frontend_get_recording_output(),
	      obs_frontend_get_streaming_output(),
	      obs_frontend_get_replay_buffer_output()}) {
		if (!output)
			continue;

		for (size_t idx = 0; idx < MAX_OUTPUT_VIDEO_ENCODERS; idx++) {
			obs_encoder_t *enc =
				obs_output_get_video_encoder2(output, idx);
			if (enc)
				encoders.emplace_back(enc);
		}
	}

	if (encoders.empty())
		return;

	// Clear any ROIs that might exist
	blog(LOG_DEBUG, "Clearing ROIs...");
	for (obs_encoder_t *enc : encoders)
		obs_encoder_clear_roi(enc);

	if (!ui->enableRoi->isChecked())
		return;
	if (!roi_data.count(uuid))
		return;

	std::vector<region_of_interest> rois;
	RegionsFromData(rois, uuid);

	if (rois.empty())
		return;

	for (obs_encoder_t *enc : encoders) {
		/* We might have already set the ROI (e.g. shared streaming/recording encoder) */
		if (obs_encoder_has_roi(enc) || !obs_encoder_active(enc))
			continue;
		blog(LOG_DEBUG, "Adding ROI to encoder: %s",
		     obs_encoder_get_name(enc));
		for (const region_of_interest &roi : rois)
			obs_encoder_add_roi(enc, &roi);
	}
}

void RoiEditor::on_actionAddRoi_triggered()
{
	auto popup = QMenu(obs_module_text("AddROI"), this);

	QAction *addSceneItemRoi =
		new QAction(obs_module_text("ROI.AddSceneItem"), this);
	connect(addSceneItemRoi, &QAction::triggered, [this] {
		auto item =
			new RoiListItem(ui->roiList, RoiListItem::SceneItem);
		ui->roiList->addItem(item);
		RoiData roi = {};
		item->setData(ROIData, QVariant::fromValue<RoiData>(roi));
		RegionItemsToData();
	});
	popup.insertAction(nullptr, addSceneItemRoi);

	QAction *addManualRoi =
		new QAction(obs_module_text("ROI.AddManual"), this);
	connect(addManualRoi, &QAction::triggered, [this] {
		auto item = new RoiListItem(ui->roiList, RoiListItem::Manual);
		ui->roiList->addItem(item);
		RoiData roi = {"", 0, 0, 0, 100, 100, 0.0f};
		item->setData(ROIData, QVariant::fromValue<RoiData>(roi));
		RegionItemsToData();
	});
	popup.insertAction(addSceneItemRoi, addManualRoi);

	popup.exec(QCursor::pos());
}

void RoiEditor::on_actionRemoveRoi_triggered()
{
	if (!currentItem)
		return;

	RoiListItem *item = currentItem;
	currentItem = nullptr;
	delete item;
}

void RoiEditor::MoveRoiItem(Direction direction)
{
	int idx = ui->roiList->currentRow();
	if (idx == -1)
		return;
	if (idx == 0 && direction == Up)
		return;
	if (idx == ui->roiList->count() - 1 && direction == Down)
		return;

	QSignalBlocker sb(ui->roiList);

	QListWidgetItem *item = ui->roiList->takeItem(idx);

	int offset = direction == Up ? -1 : 1;
	ui->roiList->insertItem(idx + offset, item);
	ui->roiList->setCurrentRow(idx + offset);
	item->setSelected(true);
}

void RoiEditor::on_actionRoiUp_triggered()
{
	MoveRoiItem(Direction::Up);
}
void RoiEditor::on_actionRoiDown_triggered()
{
	MoveRoiItem(Direction::Down);
}

void RoiEditor::resizeEvent(QResizeEvent *event)
{
	RebuildPreview();
	QDialog::resizeEvent(event);
}

/*
 * RoiListItem
 */
QVariant RoiListItem::data(int role) const
{
	if (role == ROIData)
		return QVariant::fromValue<RoiData>(roi);

	return QListWidgetItem::data(role);
}

void RoiListItem::setData(int role, const QVariant &value)
{
	if (role == ROIData) {
		roi = value.value<RoiData>();

		if (type() == Manual) {
			setText(QString(obs_module_text("ROI.ManualRegion"))
					.arg(roi.width)
					.arg(roi.height)
					.arg(roi.posX)
					.arg(roi.posY));
		} else {
			// ToDo get scene item name?
			OBSSourceAutoRelease source =
				obs_get_source_by_uuid(roi.scene_uuid.c_str());
			obs_scene_item *sceneItem =
				obs_scene_find_sceneitem_by_id(
					obs_scene_from_source(source),
					roi.scene_item_id);
			const char *name = obs_source_get_name(
				obs_sceneitem_get_source(sceneItem));

			setText(QString(obs_module_text("ROI.SceneItem"))
					.arg(name)
					.arg(roi.scene_item_id));
		}

		return;
	}

	QListWidgetItem::setData(role, value);
}

/*
 * C stuff
 */

static void SaveRoiEditor(obs_data_t *save_data, bool saving, void *)
{
	if (saving) {
		OBSDataAutoRelease obj = obs_data_create();
		roi_edit->SaveRoisToOBSData(obj);
		obs_data_set_obj(save_data, "roi", obj);
	} else {
		OBSDataAutoRelease obj = obs_data_get_obj(save_data, "roi");
		if (obj)
			roi_edit->LoadRoisFromOBSData(obj);
	}
}

static void OBSEvent(obs_frontend_event event, void *)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
		roi_edit->UpdateEncoderRois();
		roi_edit->ConnectSceneSignals();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED:
		roi_edit->UpdateEncoderRois();
		break;
	default:
		break;
	}
}

extern "C" void FreeRoiEditor() {}

extern "C" void InitRoiEditor()
{
	QAction *action = (QAction *)obs_frontend_add_tools_menu_qaction(
		obs_module_text("ROI.Editor"));

	obs_frontend_push_ui_translation(obs_module_get_string);

	QMainWindow *window = (QMainWindow *)obs_frontend_get_main_window();

	roi_edit = new RoiEditor(window);

	auto cb = []() {
		roi_edit->ShowHideDialog();
	};

	obs_frontend_pop_ui_translation();

	obs_frontend_add_save_callback(SaveRoiEditor, nullptr);
	obs_frontend_add_event_callback(OBSEvent, nullptr);

	action->connect(action, &QAction::triggered, cb);
}
