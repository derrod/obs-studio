#include "window-basic-settings-appearance.hpp"
#include "window-basic-settings.hpp"
#include "window-basic-main.hpp"
#include "obs-app.hpp"
#include "qt-wrappers.hpp"

#include <QColorDialog>
#include <QMetaEnum>
#include <QObject>
#include <QRandomGenerator>

static void SetStyle(QLabel *label, QColor color)
{
	color.setAlpha(255);
	QPalette palette = QPalette(color);
	label->setFrameStyle(QFrame::Sunken | QFrame::Panel);
	label->setText(color.name(QColor::HexRgb));
	label->setPalette(palette);
	label->setStyleSheet(QString("background-color: %1; color: %2;")
				     .arg(palette.color(QPalette::Window)
						  .name(QColor::HexRgb))
				     .arg(palette.color(QPalette::WindowText)
						  .name(QColor::HexRgb)));
	label->setAutoFillBackground(true);
	label->setAlignment(Qt::AlignCenter);
}

ColorPickPreview::ColorPickPreview(QString &name, QPalette::ColorGroup group,
				   QPalette::ColorRole role, QColor color,
				   QWidget *parent)
	: QWidget(parent),
	  m_role(role),
	  m_group(group),
	  m_paletteName(name)
{
	m_color = color;
	m_button = new QPushButton(this);
	m_colorPreview = new QLabel(this);
	m_layout = new QHBoxLayout(this);

	m_layout->addWidget(m_colorPreview);
	m_layout->addWidget(m_button);

	m_colorPreview->setBuddy(m_button);
	m_button->setText(QTStr("Basic.Settings.Appearance.ColorPicker"));

	SetStyle(m_colorPreview, color);

	connect(m_button, &QPushButton::clicked, this, [&](bool) {
		QColorDialog::ColorDialogOptions options;
		auto newColor = QColorDialog::getColor(m_color, nullptr,
						       m_paletteName, options);
		if (newColor.isValid() && newColor != m_color)
			SetColor(newColor);
	});
}

void ColorPickPreview::SetColor(QColor color)
{
	SetStyle(m_colorPreview, color);
	m_color = color;
	emit colorChanged();
}

void OBSBasicSettings::InitAppearancePage()
{
	QMetaEnum groupMeta = QMetaEnum::fromType<QPalette::ColorGroup>();

	auto addGroup = [&](QPalette::ColorGroup group) -> void {
		ui->paletteGroupComboBox->addItem(
			QString("Basic.Settings.Appearance.PaletteGroup.%1")
				.arg(groupMeta.valueToKey(group)),
			group);
	};

	addGroup(QPalette::ColorGroup::All);
	addGroup(QPalette::ColorGroup::Active);
	addGroup(QPalette::ColorGroup::Inactive);
	addGroup(QPalette::ColorGroup::Disabled);

	connect(ui->paletteGroupComboBox, &QComboBox::currentIndexChanged, this,
		[&](int idx) {
			QVariant data = ui->paletteGroupComboBox->itemData(idx);
			auto group = QPalette::ColorGroup(data.toInt());

			QFormLayout *form = dynamic_cast<QFormLayout *>(
				ui->paletteGroupBox->layout());
			for (int i = 0; i < form->rowCount(); i++) {
				QLayoutItem *field =
					form->itemAt(i, QFormLayout::FieldRole);
				if (!field)
					continue;

				auto picker = dynamic_cast<ColorPickPreview *>(
					field->widget());
				if (!picker)
					continue;

				QLayoutItem *labelItem =
					form->itemAt(i, QFormLayout::LabelRole);
				auto label = dynamic_cast<QLabel *>(
					labelItem->widget());

				if (group != QPalette::ColorGroup::All &&
				    picker->getGroup() != group) {
					label->hide();
					picker->hide();
				} else {
					label->show();
					picker->show();
				}
			}
		});
}

void OBSBasicSettings::LoadAppearanceSettings()
{
	QPalette palette = qApp->palette();
	QFormLayout *form =
		dynamic_cast<QFormLayout *>(ui->paletteGroupBox->layout());

	// Todo this needs to clear the form otherwise things get ugly
	QString translation("Basic.Settings.Appearance.Palette.%1.%2");

	QMetaEnum roleMeta = QMetaEnum::fromType<QPalette::ColorRole>();
	QMetaEnum groupMeta = QMetaEnum::fromType<QPalette::ColorGroup>();

	for (int i = 0; i < roleMeta.keyCount(); i++) {
		auto role = QPalette::ColorRole(i);
		if (role == QPalette::NColorRoles)
			break;

		for (int j = 0; j < groupMeta.keyCount(); j++) {
			auto group = QPalette::ColorGroup(j);
			if (group == QPalette::ColorGroup::NColorGroups)
				break;

			QString name = translation.arg(roleMeta.valueToKey(i),
						       groupMeta.valueToKey(j));
			auto label = new QLabel(name, ui->paletteGroupBox);
			auto picker = new ColorPickPreview(
				name, group, role, palette.color(group, role),
				this);
			label->setBuddy(picker);
			form->addRow(label, picker);

			connect(picker, &ColorPickPreview::colorChanged, this,
				[&] { AppearanceChanged(); });
		}
	}
}

void OBSBasicSettings::SaveAppearanceSettings()
{
	config_t *config = GetGlobalConfig();

	QMetaEnum roleMeta = QMetaEnum::fromType<QPalette::ColorRole>();
	QMetaEnum groupMeta = QMetaEnum::fromType<QPalette::ColorGroup>();

	QPalette palette = App()->GetThemePalette();
	QString sectionName =
		QString("CustomPalette.%1").arg(App()->GetTheme());

	// auto rand = QRandomGenerator::securelySeeded();

	QFormLayout *form =
		dynamic_cast<QFormLayout *>(ui->paletteGroupBox->layout());
	for (int i = 0; i < form->rowCount(); i++) {
		QLayoutItem *item = form->itemAt(i, QFormLayout::FieldRole);
		if (!item)
			continue;

		auto picker = dynamic_cast<ColorPickPreview *>(item->widget());
		if (!picker)
			continue;

		QColor color = picker->getColor();
		QPalette::ColorRole role = picker->getRole();
		QPalette::ColorGroup group = picker->getGroup();

		// lmao
		// color = QColor(rand.bounded(256), rand.bounded(256), rand.bounded(256));

		QColor current = palette.color(group, role);
		if (current != color) {
			palette.setColor(group, role, color);
			// Save to config
			QString colorName = color.name(QColor::HexArgb);
			QString configKey = QString("%1.%2").arg(
				roleMeta.valueToKey(role),
				groupMeta.valueToKey(group));
			config_set_string(config, QT_TO_UTF8(sectionName),
					  QT_TO_UTF8(configKey),
					  QT_TO_UTF8(colorName));
		}
	}

	App()->setCustomPalette(palette);
	/* Force re-evaluating the stylesheet to apply new palette. */
	App()->setStyleSheet(App()->styleSheet());
}
