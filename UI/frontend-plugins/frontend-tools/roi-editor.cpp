#include "roi-editor.hpp"

#include "display-helpers.hpp"
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

/// ToDo cleanup this whole refresh mess, just rebuild data always when necessary,
/// and then update preview if visible, always run encoder update.

RoiEditor::RoiEditor(QWidget *parent) : QDialog(parent), ui(new Ui_ROIEditor)
{
	ui->setupUi(this);

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	geometry = QByteArray();

	// Hide properties until needed
	ui->roiPropertiesStack->setVisible(false);
	ui->roiCommonPropertiesGroupBox->setVisible(false);

	// Fill block size list
	ui->roiBlockSize->addItem(obs_module_text("ROI.BlockSize.16"), 16);
	ui->roiBlockSize->addItem(obs_module_text("ROI.BlockSize.32"), 32);
	ui->roiBlockSize->addItem(obs_module_text("ROI.BlockSize.64"), 64);
	ui->roiBlockSize->addItem(obs_module_text("ROI.BlockSize.128"), 128);

	auto addSmoothingItem = [&](const char *text, int val) {
		ui->roiPropManualSmoothing->addItem(text, val);
		ui->roiPropSceneItemSmoothing->addItem(text, val);
	};

	addSmoothingItem(obs_module_text("ROI.Property.Smoothing.None"),
			 Smoothing::None);
	addSmoothingItem(obs_module_text("ROI.Property.Smoothing.Inside"),
			 Smoothing::Inside);
	addSmoothingItem(obs_module_text("ROI.Property.Smoothing.Outside"),
			 Smoothing::Outside);
	addSmoothingItem(obs_module_text("ROI.Property.Smoothing.Edge"),
			 Smoothing::Edge);

	connect(ui->close, &QPushButton::clicked, this, &RoiEditor::close);

	connect(ui->roiList, &QListWidget::currentItemChanged, this,
		&RoiEditor::ItemSelected);

	connect(ui->roiBlockSize, &QComboBox::currentIndexChanged, this,
		&RoiEditor::RebuildPreview);
	connect(ui->previewOpacity, &QSlider::valueChanged, this,
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
	connect(ui->roiPropPrioritySlider, &QSlider::valueChanged, this,
		&RoiEditor::PropertiesChanges);
	connect(ui->roiPropSceneItem, &QComboBox::currentIndexChanged, this,
		&RoiEditor::PropertiesChanges);
	connect(ui->roiPropEnabled, &QCheckBox::stateChanged, this,
		&RoiEditor::PropertiesChanges);
	connect(ui->roiPropOuterPrioritySlider, &QSlider::valueChanged, this,
		&RoiEditor::PropertiesChanges);
	connect(ui->roiPropStepsInnerSb, &QSpinBox::valueChanged, this,
		&RoiEditor::PropertiesChanges);
	connect(ui->roiPropStepsOuterSb, &QSpinBox::valueChanged, this,
		&RoiEditor::PropertiesChanges);
	connect(ui->roiPropRadiusInnerSb, &QSpinBox::valueChanged, this,
		&RoiEditor::PropertiesChanges);
	connect(ui->roiPropRadiusOuterSb, &QSpinBox::valueChanged, this,
		&RoiEditor::PropertiesChanges);
	connect(ui->roiPropRadiusOuterAspect, &QCheckBox::stateChanged, this,
		&RoiEditor::PropertiesChanges);
	connect(ui->roiPropRadiusInnerAspect, &QCheckBox::stateChanged, this,
		&RoiEditor::PropertiesChanges);
	connect(ui->roiPropManualSmoothingSteps, &QSpinBox::valueChanged, this,
		&RoiEditor::PropertiesChanges);
	connect(ui->roiPropManualSmoothing, &QComboBox::currentIndexChanged,
		this, &RoiEditor::PropertiesChanges);
	connect(ui->roiPropManualSmoothingPriority, &QSlider::valueChanged,
		this, &RoiEditor::PropertiesChanges);
	connect(ui->roiPropCenterPosX, &QSpinBox::valueChanged, this,
		&RoiEditor::PropertiesChanges);
	connect(ui->roiPropCenterPosY, &QSpinBox::valueChanged, this,
		&RoiEditor::PropertiesChanges);

	CreateDisplay();
}

void RoiEditor::CreateDisplay(bool recreate)
{
	// Need to recreate the display because once it's destroyed there's no way to un-destroy it.
	if (recreate) {
		int idx = ui->previewLayout->indexOf(ui->preview);
		delete ui->preview;

		/* Copied from auto-generated MOC code */
		ui->preview = new OBSQTDisplay(this);
		ui->preview->setObjectName("preview");
		QSizePolicy sizePolicy(QSizePolicy::Expanding,
				       QSizePolicy::Expanding);
		sizePolicy.setHorizontalStretch(0);
		sizePolicy.setVerticalStretch(1);
		sizePolicy.setHeightForWidth(
			ui->preview->sizePolicy().hasHeightForWidth());
		ui->preview->setSizePolicy(sizePolicy);

		ui->previewLayout->insertWidget(idx, ui->preview);
	}

	auto addDrawCallback = [this]() {
		obs_display_add_draw_callback(ui->preview->GetDisplay(),
					      RoiEditor::DrawPreview, this);
	};
	connect(ui->preview, &OBSQTDisplay::DisplayCreated, addDrawCallback);
}

void RoiEditor::closeEvent(QCloseEvent *)
{
	geometry = saveGeometry();
	obs_frontend_save();
}

void RoiEditor::ShowHideDialog()
{
	if (!isVisible()) {
		setVisible(true);
		if (!geometry.isEmpty())
			restoreGeometry(geometry);

		CreateDisplay(true);
		RefreshSceneList();
		RefreshRoiList();
		RebuildPreview(true);
	} else {
		close();
	}
}

void RoiEditor::PropertiesChanges()
{
	if (!currentItem)
		return;

	RoiData data = {};
	data.priority = (float)ui->roiPropPrioritySlider->value() / 100.0f;
	data.enabled = ui->roiPropEnabled->isChecked();
	// ToDo link those to the other ones
	data.smoothing_steps = ui->roiPropManualSmoothingSteps->value();
	data.smoothing_type = ui->roiPropManualSmoothing->currentData().toInt();
	data.smoothing_priority =
		(float)ui->roiPropManualSmoothingPriority->value() / 100.0f;

	if (currentItem->type() == RoiListItem::SceneItem) {
		auto sceneVar = ui->sceneSelect->currentData();
		data.scene_uuid = sceneVar.toString().toStdString();
		data.scene_item_id =
			ui->roiPropSceneItem->currentData().toLongLong();
	} else if (currentItem->type() == RoiListItem::Manual) {
		data.posX = ui->roiPropPosX->value();
		data.posY = ui->roiPropPosY->value();
		data.width = ui->roiPropSizeX->value();
		data.height = ui->roiPropSizeY->value();
	} else if (currentItem->type() == RoiListItem::CenterFocus) {
		data.inner_radius = ui->roiPropRadiusInnerSb->value();
		data.inner_aspect = ui->roiPropRadiusInnerAspect->isChecked();
		data.outer_radius = ui->roiPropRadiusOuterSb->value();
		data.outer_aspect = ui->roiPropRadiusOuterAspect->isChecked();
		data.inner_steps = ui->roiPropStepsInnerSb->value();
		data.outer_steps = ui->roiPropStepsOuterSb->value();
		data.outer_priority =
			(float)ui->roiPropOuterPrioritySlider->value() / 100.0f;
		data.center_x = ui->roiPropCenterPosX->value();
		data.center_y = ui->roiPropCenterPosY->value();
	}

	currentItem->setData(ROIData, QVariant::fromValue<RoiData>(data));
	RebuildPreview(true);
	UpdateEncoderRois();
}

void RoiEditor::RefreshSceneItems(bool signal)
{
	if (!currentItem && signal)
		return;

	auto sceneVar = ui->sceneSelect->currentData();
	const string scene_uuid = sceneVar.toString().toStdString();
	OBSSourceAutoRelease source =
		obs_get_source_by_uuid(scene_uuid.c_str());

	if (!source)
		return;

	int64_t scene_item_id = 0;
	if (signal) {
		ui->roiPropSceneItem->blockSignals(true);
		scene_item_id =
			ui->roiPropSceneItem->currentData().toLongLong();
	}

	ui->roiPropSceneItem->clear();
	obs_scene_enum_items(
		obs_scene_from_source(source),
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) -> bool {
			auto cb = static_cast<QComboBox *>(param);
			cb->addItem(obs_source_get_name(
					    obs_sceneitem_get_source(item)),
				    obs_sceneitem_get_id(item));
			return true;
		},
		ui->roiPropSceneItem);

	if (signal) {
		int idx = ui->roiPropSceneItem->findData(scene_item_id);
		if (idx != -1)
			ui->roiPropSceneItem->setCurrentIndex(idx);
		ui->roiPropSceneItem->blockSignals(false);
	}
}

void RoiEditor::ItemSelected(QListWidgetItem *item, QListWidgetItem *)
{
	currentItem = nullptr;

	auto new_item = dynamic_cast<RoiListItem *>(item);
	if (!new_item) {
		ui->roiCommonPropertiesGroupBox->setVisible(false);
		ui->roiPropertiesStack->setVisible(false);
		return;
	}

	ui->roiCommonPropertiesGroupBox->setVisible(true);
	ui->roiPropertiesStack->setVisible(true);

	QVariant var = item->data(ROIData);
	RoiData data = var.value<RoiData>();

	// Generic properties
	ui->roiPropEnabled->setChecked(data.enabled);
	ui->roiPropPrioritySlider->setValue((int)(100 * data.priority));

	// The "Manual" widgets act as a master for all other ones for the same values
	ui->roiPropManualSmoothingPriority->setValue(
		(int)(100 * data.smoothing_priority));
	ui->roiPropManualSmoothingSteps->setValue(data.smoothing_steps);

	int idx = ui->roiPropManualSmoothing->findData(data.smoothing_type);
	if (idx != -1)
		ui->roiPropManualSmoothing->setCurrentIndex(idx);

	// Type specific properties
	if (item->type() == RoiListItem::SceneItem) {
		ui->roiPropertiesStack->setCurrentWidget(
			ui->roiSceneItemPropertiesGroupBox);

		RefreshSceneItems(false);
		int idx = ui->roiPropSceneItem->findData(data.scene_item_id);
		if (idx != -1)
			ui->roiPropSceneItem->setCurrentIndex(idx);

	} else if (item->type() == RoiListItem::Manual) {
		ui->roiPropertiesStack->setCurrentWidget(
			ui->roiManualPropertiesGroupBox);

		ui->roiPropPosX->setValue(data.posX);
		ui->roiPropPosY->setValue(data.posY);
		ui->roiPropSizeX->setValue(data.width);
		ui->roiPropSizeY->setValue(data.height);

	} else if (item->type() == RoiListItem::CenterFocus) {
		ui->roiPropertiesStack->setCurrentWidget(
			ui->roiCenterFocusPropertiesGroupBox);

		ui->roiPropOuterPrioritySlider->setValue(
			(int)(100 * data.outer_priority));
		ui->roiPropRadiusInnerSb->setValue(data.inner_radius);
		ui->roiPropRadiusInnerAspect->setChecked(data.inner_aspect);
		ui->roiPropRadiusOuterSb->setValue(data.outer_radius);
		ui->roiPropRadiusOuterAspect->setChecked(data.outer_aspect);
		ui->roiPropStepsInnerSb->setValue(data.inner_steps);
		ui->roiPropStepsOuterSb->setValue(data.outer_steps);
		ui->roiPropCenterPosX->setValue(data.center_x);
		ui->roiPropCenterPosY->setValue(data.center_y);
	}

	/* Only set after loading so any signals to PropertiesChanged are no-ops */
	currentItem = new_item;
}

void RoiEditor::RefreshSceneList()
{
	QVariant var = ui->sceneSelect->currentData();
	ui->sceneSelect->clear();

	obs_enum_scenes(
		[](void *param, obs_source_t *src) -> bool {
			auto cb = static_cast<QComboBox *>(param);
			// screw groups
			if (obs_source_is_group(src))
				return true;

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

		auto var = item->data(ROIData);
		auto roi = var.value<RoiData>();

		obs_data_t *data = obs_data_create();
		obs_data_set_double(data, "priority", roi.priority);
		obs_data_set_bool(data, "enabled", roi.enabled);
		obs_data_set_int(data, "type", item->type());

		if (item->type() == RoiListItem::SceneItem) {
			obs_data_set_int(data, "scene_item_id",
					 roi.scene_item_id);
			obs_data_set_int(data, "smoothing_steps",
					 roi.smoothing_steps);
			obs_data_set_int(data, "smoothing_type",
					 roi.smoothing_type);
			obs_data_set_double(data, "smoothing_priority",
					    roi.smoothing_priority);
		} else if (item->type() == RoiListItem::Manual) {
			obs_data_set_int(data, "x", roi.posX);
			obs_data_set_int(data, "y", roi.posY);
			obs_data_set_int(data, "width", roi.width);
			obs_data_set_int(data, "height", roi.height);
			obs_data_set_int(data, "smoothing_steps",
					 roi.smoothing_steps);
			obs_data_set_double(data, "smoothing_priority",
					    roi.smoothing_priority);
			obs_data_set_int(data, "smoothing_type",
					 roi.smoothing_type);
		} else if (item->type() == RoiListItem::CenterFocus) {
			obs_data_set_int(data, "center_radius_inner",
					 roi.inner_radius);
			obs_data_set_int(data, "center_radius_outer",
					 roi.outer_radius);
			obs_data_set_bool(data, "center_aspect_inner",
					  roi.inner_aspect);
			obs_data_set_bool(data, "center_aspect_outer",
					  roi.outer_aspect);
			obs_data_set_int(data, "center_steps_inner",
					 roi.inner_steps);
			obs_data_set_int(data, "center_steps_outer",
					 roi.outer_steps);
			obs_data_set_double(data, "center_priority_outer",
					    roi.outer_priority);
			obs_data_set_int(data, "center_x", roi.center_x);
			obs_data_set_double(data, "center_y", roi.center_y);
		}

		roi_data[scene_uuid].emplace_back(data);
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

static constexpr int32_t kMinBlockSize = 16; // Use H.264 as a baseline

static void BuildInnerRegions(vector<region_of_interest> &rois, float priority,
			      int64_t steps, int64_t radius,
			      bool correct_aspect, int32_t center_x,
			      int32_t center_y, uint32_t width, uint32_t height)
{
	if (!radius || height < radius || width < radius ||
	    radius < kMinBlockSize / 2 || priority == 0.0 || !steps)
		return;
	int32_t interval = radius / steps;

	if (interval < kMinBlockSize) {
		/* Clamp interval size and step count to the smallest block size */
		interval = kMinBlockSize;
		steps = std::max(radius / interval, 1LL);
	} else if (interval % kMinBlockSize) {
		/* Round interval to nearest multiple of kMinBlockSize */
		interval =
			(int32_t)round((float)interval / float(kMinBlockSize)) *
			kMinBlockSize;
		steps = std::max((radius + kMinBlockSize) / interval, 1LL);
	}

	double priority_interval = priority / (double)steps;
	double aspect = 1.0;
	if (correct_aspect)
		aspect = (double)width / (double)height;

	int32_t middle_x = center_x >= 0 ? center_x : width / 2;
	int32_t middle_y = center_y >= 0 ? center_y : height / 2;

	for (int32_t i = 1; i <= steps; i++) {
		// Configurable center point means we have to clamp these.
		uint32_t top = std::clamp(middle_y - interval * i, 0, 16384);
		uint32_t bottom = std::clamp(middle_y + interval * i, 0, 16384);
		uint32_t left = std::clamp(
			(int32_t)(middle_x - interval * i * aspect), 0, 16384);
		uint32_t right = std::clamp(
			(int32_t)(middle_x + interval * i * aspect), 0, 16384);
		float region_priority =
			(float)(priority - priority_interval * (i - 1));

		region_of_interest roi = {top, bottom, left, right,
					  region_priority};
		rois.push_back(roi);
	}
}

static void BuildOuterRegions(vector<region_of_interest> &rois, float priority,
			      int64_t steps, int64_t radius,
			      bool correct_aspect, uint32_t width,
			      uint32_t height)
{
	if (!radius || height / 2 < radius || width / 2 < radius ||
	    radius < kMinBlockSize || priority == 0.0 || !steps)
		return;

	int64_t interval = radius / steps;

	if (interval < kMinBlockSize) {
		/* Clamp interval size and step count to the smallest block size */
		interval = kMinBlockSize;
		steps = std::max(radius / interval, 1LL);
	} else if (interval % kMinBlockSize) {
		/* Round interval to nearest multiple of kMinBlockSize */
		interval =
			(int64_t)round((float)interval / float(kMinBlockSize)) *
			kMinBlockSize;
		steps = std::max((radius + kMinBlockSize) / interval, 1LL);
	}

	double priority_interval = priority / (double)steps;
	double aspect = 1.0;
	if (correct_aspect)
		aspect = (double)width / (double)height;

	/* Add neutral baseline */
	region_of_interest neutral = {
		(uint32_t)radius, (uint32_t)(height - radius),
		(uint32_t)((double)radius * aspect),
		(uint32_t)(width - (double)radius * aspect), 0.0f};
	rois.push_back(neutral);

	for (int i = 1; steps > 1 && i < steps; i++) {
		region_of_interest roi = {
			(uint32_t)(radius - interval * i),
			(uint32_t)(height - radius + interval * i),
			(uint32_t)((double)(radius - interval * i) * aspect),
			(uint32_t)(width -
				   (double)(radius - interval * i) * aspect),
			(float)(priority_interval * i)};
		rois.push_back(roi);
	}

	/* Ensure last region always goes to frame edges */
	region_of_interest final = {0, height, 0, width, (float)priority};
	rois.push_back(final);
}

static void BuildCenterFocusROI(vector<region_of_interest> &rois,
				obs_data_t *data, uint32_t width,
				uint32_t height)
{
	int64_t inner_radius = obs_data_get_int(data, "center_radius_inner");
	bool aspect_inner = obs_data_get_bool(data, "center_aspect_inner");
	int64_t outer_radius = obs_data_get_int(data, "center_radius_outer");
	bool aspect_outer = obs_data_get_bool(data, "center_aspect_outer");
	int64_t steps_inner = obs_data_get_int(data, "center_steps_inner");
	int64_t steps_outer = obs_data_get_int(data, "center_steps_outer");
	int32_t center_x = obs_data_get_int(data, "center_x");
	int32_t center_y = obs_data_get_int(data, "center_y");
	double priority_outer =
		obs_data_get_double(data, "center_priority_outer");
	double priority = obs_data_get_double(data, "priority");

	/* Inner regions (if any) */
	BuildInnerRegions(rois, priority, steps_inner, inner_radius,
			  aspect_inner, center_x, center_y, width, height);
	BuildOuterRegions(rois, priority_outer, steps_outer, outer_radius,
			  aspect_outer, width, height);
}

/// Split specified ROI up into multiple based on given mode
static void SmoothROI(vector<region_of_interest> &regions,
		      const region_of_interest &roi, RoiEditor::Smoothing type,
		      int steps, const double edge_priority)
{
	int max_steps = 0;
	uint32_t width = roi.right - roi.left;
	uint32_t height = roi.bottom - roi.top;

	// Figure out how many steps we can even do
	if (type == RoiEditor::Inside) {
		max_steps = std::min(width / kMinBlockSize / 2,
				     height / kMinBlockSize / 2);
	} else if (type == RoiEditor::Outside) {
		max_steps = 64; // limit to something reasonable
	} else if (type == RoiEditor::Edge) {
		// Effectively gives us inside + outside
		max_steps = std::min(width / kMinBlockSize + 1,
				     height / kMinBlockSize + 1);
	}

	steps = std::min(steps, max_steps);

	if (type == RoiEditor::None || steps < 2) {
		regions.push_back(roi);
		return;
	}

	// Just create a bunch of additional zones fading to outside priority
	double interval = (roi.priority - edge_priority) / (double)(steps - 1);

	int32_t step_offset = 0;
	if (type == RoiEditor::Edge)
		step_offset = -steps / 2 + 1;
	else if (type == RoiEditor::Inside)
		step_offset = -steps + 1;

	for (int32_t step = 0; step < steps; step++) {
		float region_priority = std::clamp(
			roi.priority - (float)(interval * step), -1.0f, 1.0f);

		int32_t mul = step + step_offset;
		uint32_t top = std::clamp(
			(int32_t)roi.top - kMinBlockSize * mul, 0, 16384);
		uint32_t bottom = std::clamp(
			(int32_t)roi.bottom + kMinBlockSize * mul, 0, 16384);
		uint32_t left = std::clamp(
			(int32_t)roi.left - kMinBlockSize * mul, 0, 16384);
		uint32_t right = std::clamp(
			(int32_t)roi.right + kMinBlockSize * mul, 0, 16384);

		region_of_interest step_region = {
			top, bottom, left, right, region_priority,
		};
		regions.push_back(step_region);
	}
}

void RoiEditor::RegionsFromData(vector<region_of_interest> &regions,
				const string &uuid)
{
	const auto &region_data = roi_data[uuid];
	if (region_data.empty())
		return;
	OBSSourceAutoRelease source = obs_get_source_by_uuid(uuid.c_str());
	if (!source)
		return;

	for (obs_data_t *data : region_data) {
		float priority = obs_data_get_double(data, "priority");

		if (!obs_data_get_bool(data, "enabled"))
			continue;

		auto type = (RoiListItem::RoiItemType)obs_data_get_int(data,
								       "type");
		auto smoothing_type = (RoiEditor::Smoothing)obs_data_get_int(
			data, "smoothing_type");
		int smoothing_steps = obs_data_get_int(data, "smoothing_steps");
		double smoothing_priority =
			obs_data_get_double(data, "smoothing_priority");

		if (type == RoiListItem::SceneItem) {
			/* Scene Item ROI */
			int64_t id = obs_data_get_int(data, "scene_item_id");
			OBSSceneItem sceneItem = obs_scene_find_sceneitem_by_id(
				obs_scene_from_source(source), id);

			if (sceneItem && obs_sceneitem_visible(sceneItem)) {
				auto roi = GetItemROI(sceneItem, priority);
				if (roi.bottom == 0 || roi.right == 0)
					continue;

				if (smoothing_type != Smoothing::None &&
				    smoothing_steps > 1 &&
				    smoothing_priority != priority) {
					SmoothROI(regions, roi, smoothing_type,
						  smoothing_steps,
						  smoothing_priority);
				} else {
					regions.push_back(roi);
				}
			}

		} else if (type == RoiListItem::Manual) {
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

			if (smoothing_type != Smoothing::None &&
			    smoothing_steps > 1 &&
			    smoothing_priority != priority) {
				SmoothROI(regions, roi, smoothing_type,
					  smoothing_steps, smoothing_priority);
			} else {
				regions.push_back(roi);
			}

		} else if (type == RoiListItem::CenterFocus) {
			/* Center-focus ROI */
			uint32_t cx = obs_source_get_width(source);
			uint32_t cy = obs_source_get_height(source);
			BuildCenterFocusROI(regions, data, cx, cy);
		}
	}
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

	roi_mutex.lock();
	rois.clear();
	RegionsFromData(rois, scene_uuid);
	roi_mutex.unlock();

	OBSSourceAutoRelease program = obs_frontend_get_current_scene();
	const char *program_uuid = obs_source_get_uuid(program);

	if (scene_uuid != program_uuid) {
		OBSSourceAutoRelease selected =
			obs_get_source_by_uuid(scene_uuid.c_str());
		previewSource = obs_source_get_weak_source(selected);
	} else {
		previewSource = nullptr;
	}

	texBlockSize = blockSize;
	texOpacity = ui->previewOpacity->value();
	rebuild_texture = true;
}

static void DrawROI(const region_of_interest &roi, const float opacity,
		    gs_eparam_t *colour_param, const uint32_t blockSize)
{
	const uint32_t roi_left = roi.left / blockSize;
	const uint32_t roi_top = roi.top / blockSize;
	const uint32_t roi_right = (roi.right + blockSize - 1) / blockSize;
	const uint32_t roi_bottom = (roi.bottom + blockSize - 1) / blockSize;

	float red = roi.priority < 0.0f ? -roi.priority : 0.0f;
	float green = roi.priority > 0.0f ? roi.priority : 0.0f;
	float blue = std::max(0.5f - std::abs(roi.priority), 0.0f);

	vec4 fillColor;
	vec4_set(&fillColor, red, green, blue, opacity);

	gs_matrix_push();
	gs_matrix_identity();

	gs_matrix_translate3f(roi_left, roi_top, 0.0f);
	gs_matrix_scale3f(roi_right - roi_left, roi_bottom - roi_top, 1.0f);

	gs_effect_set_vec4(colour_param, &fillColor);
	gs_draw(GS_TRISTRIP, 0, 0);

	gs_matrix_pop();
}

void RoiEditor::CreatePreviewTexture(RoiEditor *editor, uint32_t cx,
				     uint32_t cy)
{
	if (!editor->texRender)
		editor->texRender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);

	if (!editor->rectFill) {
		gs_render_start(true);

		gs_vertex2f(0.0f, 0.0f);
		gs_vertex2f(1.0f, 0.0f);
		gs_vertex2f(0.0f, 1.0f);
		gs_vertex2f(1.0f, 1.0f);

		editor->rectFill = gs_render_save();
	}

	const uint32_t block_width =
		(cx + (editor->texBlockSize - 1)) / editor->texBlockSize;
	const uint32_t block_height =
		(cy + (editor->texBlockSize - 1)) / editor->texBlockSize;
	const float opacity = (float)editor->texOpacity / 100.0f;

	gs_texrender_reset(editor->texRender);

	if (gs_texrender_begin(editor->texRender, block_width, block_height)) {
		vec4 clear_color;
		vec4_zero(&clear_color);
		// Set background to black if opacity is 100%
		if (opacity == 1.0f)
			clear_color.w = 1.0f;

		gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);

		gs_ortho(0.0f, (float)block_width, 0.0f, (float)block_height,
			 -100.0f, 100.0f);

		gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_SOLID);
		gs_technique_t *tech = gs_effect_get_technique(effect, "Solid");
		gs_eparam_t *colour_param =
			gs_effect_get_param_by_name(effect, "color");

		gs_blend_state_push();
		gs_enable_blending(false);

		gs_technique_begin(tech);
		gs_technique_begin_pass(tech, 0);
		gs_load_vertexbuffer(editor->rectFill);

		editor->roi_mutex.lock();
		// Regions have to be drawn back to front
		for (auto it = editor->rois.rbegin(); it != editor->rois.rend();
		     ++it) {
			const region_of_interest &roi = *it;
			DrawROI(roi, opacity, colour_param,
				editor->texBlockSize);
		}
		editor->roi_mutex.unlock();

		gs_load_vertexbuffer(nullptr);
		gs_technique_end_pass(tech);
		gs_technique_end(tech);

		gs_blend_state_pop();

		gs_texrender_end(editor->texRender);
	}

	editor->rebuild_texture = false;
}

void RoiEditor::DrawPreview(void *data, uint32_t cx, uint32_t cy)
{
	RoiEditor *editor = static_cast<RoiEditor *>(data);

	obs_video_info ovi;
	obs_get_video_info(&ovi);

	int viewport_x, viewport_y;
	float scale;

	GetScaleAndCenterPos(ovi.base_width, ovi.base_height, cx, cy,
			     viewport_x, viewport_y, scale);

	int viewport_width = int(scale * float(ovi.base_width));
	int viewport_height = int(scale * float(ovi.base_height));

	/* Rebuild preview texture if necessary */
	if (editor->rebuild_texture)
		CreatePreviewTexture(editor, ovi.base_width, ovi.base_height);

	if (!editor->pointSampler) {
		gs_sampler_info point_sampler = {};
		point_sampler.max_anisotropy = 1;
		editor->pointSampler = gs_samplerstate_create(&point_sampler);
	}

	gs_viewport_push();
	gs_projection_push();

	gs_ortho(0.0f, float(ovi.base_width), 0.0f, float(ovi.base_height),
		 -100.0f, 100.0f);
	gs_set_viewport(viewport_x, viewport_y, viewport_width,
			viewport_height);

	if (editor->texOpacity < 100) {
		if (editor->previewSource) {
			OBSSourceAutoRelease source =
				obs_weak_source_get_source(
					editor->previewSource);
			obs_source_video_render(source);
		} else {
			obs_render_main_texture_src_color_only();
		}
	}

	/* Draw map texture if we have it */
	if (editor->texRender) {
		const bool previous = gs_framebuffer_srgb_enabled();
		gs_enable_framebuffer_srgb(true);

		gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
		gs_eparam_t *param =
			gs_effect_get_param_by_name(effect, "image");
		gs_texture_t *tex = gs_texrender_get_texture(editor->texRender);

		gs_effect_set_next_sampler(param, editor->pointSampler);
		gs_effect_set_texture_srgb(param, tex);

		const float texScale = (float)editor->texBlockSize;

		gs_matrix_push();
		gs_matrix_scale3f(texScale, texScale, 1.0f);

		while (gs_effect_loop(effect, "Draw"))
			gs_draw_sprite(tex, 0, 0, 0);

		gs_matrix_pop();

		gs_enable_framebuffer_srgb(previous);
	}

	gs_projection_pop();
	gs_viewport_pop();
}

void RoiEditor::SceneItemTransform(void *param, calldata_t *)
{
	RoiEditor *window = reinterpret_cast<RoiEditor *>(param);
	QMetaObject::invokeMethod(window, "RebuildPreview");
	QMetaObject::invokeMethod(window, "UpdateEncoderRois");
}

void RoiEditor::SceneItemVisibility(void *param, calldata_t *, bool)
{
	RoiEditor *window = reinterpret_cast<RoiEditor *>(param);
	QMetaObject::invokeMethod(window, "RebuildPreview");
	QMetaObject::invokeMethod(window, "UpdateEncoderRois");
}

void RoiEditor::ItemRemovedOrAdded(void *param, calldata_t *)
{
	RoiEditor *window = reinterpret_cast<RoiEditor *>(param);
	// The "item_remove" signal comes in before the item is actually removed,
	// so defer the refresh to avoid getting the list from libobs before it is updated.
	QMetaObject::invokeMethod(window, "RefreshSceneItems",
				  Qt::QueuedConnection);
}

void RoiEditor::ConnectSceneSignals()
{
	sceneSignals.clear();

	OBSSourceAutoRelease source = obs_frontend_get_current_scene();
	signal_handler_t *signal = obs_source_get_signal_handler(source);
	if (!signal)
		return;

	sceneSignals.emplace_back(signal, "item_transform", SceneItemTransform,
				  this);
	sceneSignals.emplace_back(signal, "item_visible", SceneItemTransform,
				  this);
	sceneSignals.emplace_back(signal, "item_add", ItemRemovedOrAdded, this);
	sceneSignals.emplace_back(signal, "item_remove", ItemRemovedOrAdded,
				  this);
	sceneSignals.emplace_back(signal, "refresh", ItemRemovedOrAdded, this);
}

void RoiEditor::RefreshRoiList()
{
	auto var = ui->sceneSelect->currentData();
	if (!var.isValid())
		return;

	ui->roiList->clear();

	const string scene_uuid = var.toString().toStdString();
	for (obs_data_t *roi : roi_data[scene_uuid]) {
		auto type =
			(RoiListItem::RoiItemType)obs_data_get_int(roi, "type");

		RoiData data{};

		data.scene_uuid = scene_uuid;
		data.scene_item_id = obs_data_get_int(roi, "scene_item_id");
		data.posX = obs_data_get_int(roi, "x");
		data.posY = obs_data_get_int(roi, "y");
		data.width = obs_data_get_int(roi, "width");
		data.height = obs_data_get_int(roi, "height");
		data.inner_radius =
			obs_data_get_int(roi, "center_radius_inner");
		data.inner_steps = obs_data_get_int(roi, "center_steps_inner");
		data.inner_aspect =
			obs_data_get_bool(roi, "center_aspect_inner");
		data.outer_radius =
			obs_data_get_int(roi, "center_radius_outer");
		data.outer_steps = obs_data_get_int(roi, "center_steps_outer");
		data.outer_priority = (float)obs_data_get_double(
			roi, "center_priority_outer");
		data.outer_aspect =
			obs_data_get_bool(roi, "center_aspect_outer");
		data.center_x = -1;
		data.center_y = -1;
		data.smoothing_steps = obs_data_get_int(roi, "smoothing_steps");
		data.smoothing_type = obs_data_get_int(roi, "smoothing_type");
		data.smoothing_priority =
			obs_data_get_double(roi, "smoothing_priority");
		data.enabled = obs_data_get_bool(roi, "enabled");
		data.priority = (float)obs_data_get_double(roi, "priority");

		// Special case, these have to be -1 if unset
		if (obs_data_has_user_value(roi, "center_x"))
			data.center_x = obs_data_get_int(roi, "center_x");
		if (obs_data_has_user_value(roi, "center_y"))
			data.center_y = obs_data_get_int(roi, "center_y");

		RoiListItem *item = new RoiListItem(type);
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

void RoiEditor::UpdateEncoderRois()
{
	OBSSourceAutoRelease scene = obs_frontend_get_current_scene();
	if (!scene)
		return;

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
	for (obs_encoder_t *enc : encoders)
		obs_encoder_clear_roi(enc);

	if (!ui->enableRoi->isChecked())
		return;
	if (!roi_data.count(uuid))
		return;

	std::vector<region_of_interest> regions;
	RegionsFromData(regions, uuid);

	if (regions.empty())
		return;

	for (obs_encoder_t *enc : encoders) {
		/* We might have already set the ROI (e.g. shared streaming/recording encoder) */
		if (obs_encoder_has_roi(enc) || !obs_encoder_active(enc))
			continue;
		blog(LOG_DEBUG, "Adding ROI to encoder: %s",
		     obs_encoder_get_name(enc));
		for (const region_of_interest &roi : regions)
			obs_encoder_add_roi(enc, &roi);
	}
}

void RoiEditor::AddRegionItem(int type)
{
	auto item = new RoiListItem(type);
	RoiData roi = {};
	roi.height = 100;
	roi.width = 100;
	roi.enabled = true;
	item->setData(ROIData, QVariant::fromValue<RoiData>(roi));

	ui->roiList->insertItem(0, item);
	ui->roiList->setCurrentItem(item);

	RegionItemsToData();
	RebuildPreview(true);
}

void RoiEditor::on_actionAddRoi_triggered()
{
	auto popup = QMenu(obs_module_text("ROI.AddMenu"), this);

	QAction *addSceneItemRoi =
		new QAction(obs_module_text("ROI.AddMenu.SceneItem"), this);
	QAction *addManualRoi =
		new QAction(obs_module_text("ROI.AddMenu.Manual"), this);
	QAction *addCenterRoi =
		new QAction(obs_module_text("ROI.AddMenu.CenterFocus"), this);

	connect(addSceneItemRoi, &QAction::triggered,
		[this] { AddRegionItem(RoiListItem::SceneItem); });
	connect(addManualRoi, &QAction::triggered,
		[this] { AddRegionItem(RoiListItem::Manual); });
	connect(addCenterRoi, &QAction::triggered,
		[this] { AddRegionItem(RoiListItem::CenterFocus); });

	popup.insertAction(nullptr, addSceneItemRoi);
	popup.insertAction(addSceneItemRoi, addManualRoi);
	popup.insertAction(addManualRoi, addCenterRoi);

	popup.exec(QCursor::pos());
}

void RoiEditor::on_actionRemoveRoi_triggered()
{
	if (!currentItem)
		return;

	RoiListItem *item = currentItem;
	currentItem = nullptr;
	delete item;

	RegionItemsToData();
	RebuildPreview(true);
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
	RebuildPreview(true);
	UpdateEncoderRois();
}
void RoiEditor::on_actionRoiDown_triggered()
{
	MoveRoiItem(Direction::Down);
	RebuildPreview(true);
	UpdateEncoderRois();
}

void RoiEditor::resizeEvent(QResizeEvent *event)
{
	RebuildPreview();
	QDialog::resizeEvent(event);
}

void RoiEditor::LoadRoisFromOBSData(obs_data_t *obj)
{
	ui->enableRoi->setChecked(obs_data_get_bool(obj, "enabled"));
	if (obs_data_has_user_value(obj, "opacity"))
		ui->previewOpacity->setValue(obs_data_get_int(obj, "opacity"));

	const char *geo = obs_data_get_string(obj, "window_geometry");
	if (geo)
		geometry = QByteArray::fromBase64(geo);

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
	obs_data_set_int(obj, "opacity", ui->previewOpacity->value());
	obs_data_set_string(obj, "window_geometry",
			    saveGeometry().toBase64().constData());
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

		QString desc;

		// This needs to be prettier at some point
		if (!roi.enabled) {
			desc += "[";
			desc += obs_module_text("ROI.Item.DisabledPrefix");
			desc += "] ";
		}

		if (type() == Manual) {
			desc += QString(obs_module_text(
						"ROI.Item.ManualRegion"))
					.arg(roi.width)
					.arg(roi.height)
					.arg(roi.posX)
					.arg(roi.posY);
		} else if (type() == SceneItem) {
			OBSSourceAutoRelease source =
				obs_get_source_by_uuid(roi.scene_uuid.c_str());
			obs_scene_item *sceneItem =
				obs_scene_find_sceneitem_by_id(
					obs_scene_from_source(source),
					roi.scene_item_id);
			const char *name = obs_source_get_name(
				obs_sceneitem_get_source(sceneItem));

			desc += QString(obs_module_text("ROI.Item.SceneItem"))
					.arg(name)
					.arg(roi.scene_item_id);
		} else {
			desc += obs_module_text("ROI.Item.CenterFocus");
		}

		setText(desc);
		return;
	}

	QListWidgetItem::setData(role, value);
}

/*
 * Frontend callbacks
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

/*
 * C stuff
 */

extern "C" void FreeRoiEditor() {}

extern "C" void InitRoiEditor()
{
	auto action =
		static_cast<QAction *>(obs_frontend_add_tools_menu_qaction(
			obs_module_text("ROIEditor")));
	auto window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());

	/* Push translation function so that strings in .ui file are translated */
	obs_frontend_push_ui_translation(obs_module_get_string);
	roi_edit = new RoiEditor(window);
	obs_frontend_pop_ui_translation();

	obs_frontend_add_save_callback(SaveRoiEditor, nullptr);
	obs_frontend_add_event_callback(OBSEvent, nullptr);

	QAction::connect(action, &QAction::triggered, roi_edit,
			 &RoiEditor::ShowHideDialog);
}
