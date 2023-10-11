#include "window-basic-settings-appearance.hpp"
#include "window-basic-settings.hpp"
#include "window-basic-main.hpp"
#include "obs-app.hpp"
#include "qt-wrappers.hpp"

#include <QColorDialog>
#include <QDirIterator>
#include <QFile>
#include <QMetaEnum>
#include <QObject>
#include <QRandomGenerator>
#include <QPainter>
#include <QButtonGroup>

#include "platform.hpp"
#include "ui-config.h"
#include "util/profiler.hpp"

using namespace std;

static bool WidgetChanged(QWidget *widget)
{
	return widget->property("changed").toBool();
}

static void SetStyle(QPushButton *btn, QColor color)
{
	btn->setText(color.name(QColor::HexRgb));

	/* We use a palette to automatically generate matching text colour */
	color.setAlpha(255);
	QPalette palette = QPalette(color);
	btn->setPalette(palette);

	const QString background =
		palette.color(QPalette::Window).name(QColor::HexRgb);
	const QString text =
		palette.color(QPalette::WindowText).name(QColor::HexRgb);

	btn->setStyleSheet(QString("background-color: %1; color: %2;")
				   .arg(background)
				   .arg(text));
}

ColorPickingWidget::ColorPickingWidget(QColor color, QColor defaultColor,
				       QString name, QWidget *parent)
	: QWidget(parent),
	  m_name(std::move(name)),
	  m_defaultColor(defaultColor)
{
	m_color = color.isValid() ? color : defaultColor;

	layout = new QHBoxLayout(this);
	button = new QPushButton(this);

	/* Ensure button will fit largest possible string for our font */
	button->setText("#000000");
	button->adjustSize();
	layout->addWidget(button);
	SetStyle(button, m_color);

	connect(button, &QPushButton::clicked, this, [&](bool) {
		auto newColor = QColorDialog::getColor(m_color, this, m_name);
		if (newColor.isValid() && newColor != m_color)
			SetColor(newColor);
	});

	/* Create reset button */
	resetBtn = new QPushButton(QTStr("Reset"), this);
	resetBtn->setEnabled(m_color != m_defaultColor);
	layout->addWidget(resetBtn);

	connect(resetBtn, &QPushButton::clicked, this,
		&ColorPickingWidget::ResetClicked);
}

void ColorPickingWidget::SetColor(QColor color)
{
	SetStyle(button, color);
	m_color = color;
	setProperty("changed", true);
	resetBtn->setEnabled(true);
	emit colorChanged();
}

void ColorPickingWidget::ResetClicked()
{
	SetColor(m_defaultColor);
	resetBtn->setEnabled(false);
}

static QPixmap
SVGColourReplacement(const QHash<QString, OBSThemeVariable> &vars)
{
	QPixmap pm(128, 128);
	pm.fill(QColor(0, 0, 0, 0));

	QFile svgFile(":settings/images/settings/theme_preview.svg");

	if (!svgFile.open(QFile::ReadOnly)) {
		blog(LOG_WARNING, "Failed to open SVG D:");
		return pm;
	}

	QByteArray svg = svgFile.readAll();
	QString svgStr(svg);

	QString needleTemplate("${%1}");

	for (const OBSThemeVariable &var : vars) {
		OBSThemeVariable realVar;

		if (var.type == OBSThemeVariable::Alias ||
		    var.type == OBSThemeVariable::Preset)
			realVar = vars[var.value.toString()];
		else
			realVar = var;

		if (realVar.type != OBSThemeVariable::Color)
			continue;

		QString needle = needleTemplate.arg(realVar.name);
		QString replacement =
			realVar.value.value<QColor>().name(QColor::HexRgb);

		svgStr = svgStr.replace(needle, replacement);
	}

	QSvgRenderer renderer(svgStr.toUtf8());
	QPainter painter(&pm);
	renderer.render(&painter, pm.rect());

	return pm;
}

void OBSBasicSettings::InitAppearancePage()
{
	savedTheme = App()->GetTheme();
	const QString currentBaseTheme =
		savedTheme->isBaseTheme ? savedTheme->id : savedTheme->parent;

	for (const OBSTheme &theme : App()->GetThemes())
		if (theme.isVisible && theme.isBaseTheme)
			ui->theme->addItem(theme.name, theme.id);

	int idx = ui->theme->findData(currentBaseTheme);
	if (idx != -1)
		ui->theme->setCurrentIndex(idx);
}

void OBSBasicSettings::LoadThemeList(bool reload)
{
	ProfileScope("OBSBasicSettings::LoadThemeList");

	const OBSTheme *currentTheme = App()->GetTheme();
	const QString currentBaseTheme = currentTheme->isBaseTheme
						 ? currentTheme->id
						 : currentTheme->parent;

	/* Nothing to do if current and last base theme were the same */
	const QString baseThemeId = ui->theme->currentData().toString();
	if (reload && baseThemeId == currentBaseTheme)
		return;

	/* Clear previous entries */
	QLayoutItem *item;
	while ((item = ui->themevariantLayout->takeAt(0))) {
		item->widget()->deleteLater();
		delete item;
	}

	if (variantButtonGroup)
		variantButtonGroup->deleteLater();

	variantButtonGroup = new QButtonGroup(this);
	variantButtonGroup->setExclusive(true);
	connect(variantButtonGroup, &QButtonGroup::buttonClicked, this,
		&OBSBasicSettings::LoadAppearanceSettings);
	connect(variantButtonGroup, &QButtonGroup::buttonClicked, this,
		&OBSBasicSettings::AppearanceChanged);

	int btnId = 1;
	variantButtonIdMap.clear();

	for (const OBSTheme &theme : App()->GetThemes()) {
		/* Skip non-visible themes */
		if (!theme.isVisible || theme.isHighContrast)
			continue;
		/* Skip non-child themes */
		if (theme.isBaseTheme || theme.parent != baseThemeId)
			continue;

		QPixmap pixmap =
			SVGColourReplacement(App()->GetThemeVariables(theme));
		auto *btn = new QPushButton(pixmap, theme.name, this);
		btn->setFlat(true);
		btn->setIconSize(QSize(64, 64));
		btn->setCheckable(true);
		btn->setChecked(savedTheme->id == theme.id);

		variantButtonIdMap[btnId] = theme.id;
		variantButtonGroup->addButton(btn, btnId++);

		ui->themevariantLayout->addWidget(btn);
	}
}

void OBSBasicSettings::LoadAppearanceSettings(bool reload)
{
	LoadThemeList(reload);

	if (reload) {
		QString themeId = ui->theme->currentData().toString();
		int variantBtnId = variantButtonGroup->checkedId();
		if (variantBtnId != -1)
			themeId = variantButtonIdMap[variantBtnId];

		if (App()->GetTheme()->id != themeId)
			App()->SetTheme(themeId);
	}

	QFormLayout *form =
		dynamic_cast<QFormLayout *>(ui->paletteGroupBox->layout());
	QLayoutItem *item;
	while ((item = form->takeAt(0))) {
		item->widget()->deleteLater();
		delete item;
	}

	ui->paletteGroupBox->setChecked(config_get_bool(
		GetGlobalConfig(), "Appearance", "Customisation"));

	auto &userVars = App()->GetThemeUserVars();
	auto &userVarOrdered = App()->GetThemeVariableOrder();

	for (auto &key : userVarOrdered) {
		const OBSThemeVariable &var = userVars[key];
		auto label = new QLabel(var.name, ui->paletteGroupBox);

		QWidget *control;
		QLineEdit *le;
		QComboBox *cb;
		QDoubleSpinBox *sb;
		ColorPickingWidget *cp;

		switch (var.type) {
		case OBSThemeVariable::Alias:
		case OBSThemeVariable::Calc:
			/* Not user-editable */
			continue;
		case OBSThemeVariable::Number:
			sb = new QDoubleSpinBox(ui->paletteGroupBox);
			sb->setSingleStep(1.0);
			sb->setDecimals(0);
			sb->setRange(-10000.0, 10000.0);
			sb->setValue(var.userValue.isValid()
					     ? var.userValue.toDouble()
					     : var.value.toDouble());
			control = sb;
			connect(sb, &QDoubleSpinBox::valueChanged, this,
				&OBSBasicSettings::AppearanceChanged);
			break;
		case OBSThemeVariable::Size:
			sb = new QDoubleSpinBox(ui->paletteGroupBox);
			sb->setSingleStep(0.5);
			sb->setRange(-10000.0, 10000.0);
			sb->setValue(var.userValue.isValid()
					     ? var.userValue.toDouble()
					     : var.value.toDouble());
			sb->setSuffix(var.suffix);
			control = sb;
			connect(sb, &QDoubleSpinBox::valueChanged, this,
				&OBSBasicSettings::AppearanceChanged);
			break;
		case OBSThemeVariable::Range:
			sb = new QDoubleSpinBox(ui->paletteGroupBox);
			sb->setSingleStep(var.rangeStep);
			sb->setRange(var.rangeMin, var.rangeMax);
			sb->setValue(var.userValue.isValid()
					     ? var.userValue.toDouble()
					     : var.value.toDouble());
			sb->setSuffix(var.suffix);
			control = sb;
			connect(sb, &QDoubleSpinBox::valueChanged, this,
				&OBSBasicSettings::AppearanceChanged);
			break;
		case OBSThemeVariable::Preset:
			cb = new QComboBox(ui->paletteGroupBox);

			for (const auto &[key, desc] : var.presets)
				cb->addItem(desc, key);

			cb->setCurrentIndex(
				cb->findData(var.userValue.isValid()
						     ? var.userValue.toString()
						     : var.value.toString()));

			connect(cb, &QComboBox::currentIndexChanged, this,
				&OBSBasicSettings::AppearanceChanged);
			control = cb;
			break;
		case OBSThemeVariable::Color:
			cp = new ColorPickingWidget(
				var.userValue.value<QColor>(),
				var.value.value<QColor>(), var.name,
				ui->paletteGroupBox);
			connect(cp, &ColorPickingWidget::colorChanged, this,
				&OBSBasicSettings::AppearanceChanged);
			control = cp;
			break;
		case OBSThemeVariable::String:
			le = new QLineEdit(var.value.toString(),
					   ui->paletteGroupBox);
			connect(le, &QLineEdit::textChanged, this,
				&OBSBasicSettings::AppearanceChanged);
			control = le;
			break;
		default:
			blog(LOG_ERROR, "Unknown variable format: %d",
			     var.type);
			label->deleteLater();
			continue;
		}
		label->setBuddy(control);
		form->addRow(label, control);
	}
}

void OBSBasicSettings::SaveAppearanceSettings()
{
	config_t *config = GetGlobalConfig();

	OBSTheme *currentTheme = App()->GetTheme();
	if (savedTheme != currentTheme) {
		config_set_string(config, "Appearance", "Theme",
				  QT_TO_UTF8(currentTheme->id));
	}

	bool enabled = ui->paletteGroupBox->isChecked();
	config_set_bool(config, "Appearance", "Customisation", enabled);

	if (enabled) {
		QString sectionName =
			"Appearance.Customisation." + currentTheme->id;

		auto form = dynamic_cast<QFormLayout *>(
			ui->paletteGroupBox->layout());
		for (int i = 0; i < form->rowCount(); i++) {
			QLayoutItem *litem =
				form->itemAt(i, QFormLayout::LabelRole);
			if (!litem)
				continue;
			auto label = dynamic_cast<QLabel *>(litem->widget());

			QLayoutItem *item =
				form->itemAt(i, QFormLayout::FieldRole);
			if (!item)
				continue;

			QWidget *w = item->widget();
			if (w->inherits("ColorPickingWidget")) {
				auto picker =
					dynamic_cast<ColorPickingWidget *>(w);

				QColor def = picker->getDefaultColor();
				QColor col = picker->getColor();

				if (col != def) {
					config_set_string(
						config, QT_TO_UTF8(sectionName),
						QT_TO_UTF8(label->text()),
						QT_TO_UTF8(col.name(
							QColor::HexRgb)));
				} else {
					config_remove_value(
						config, QT_TO_UTF8(sectionName),
						QT_TO_UTF8(label->text()));
				}
			} else if (w->inherits("QDoubleSpinBox") &&
				   WidgetChanged(w)) {
				auto dsb = dynamic_cast<QDoubleSpinBox *>(w);
				config_set_double(config,
						  QT_TO_UTF8(sectionName),
						  QT_TO_UTF8(label->text()),
						  dsb->value());
			} else if (w->inherits("QLineEdit") &&
				   WidgetChanged(w)) {
				auto le = dynamic_cast<QLineEdit *>(w);
				config_set_string(config,
						  QT_TO_UTF8(sectionName),
						  QT_TO_UTF8(label->text()),
						  QT_TO_UTF8(le->text()));
			} else if (w->inherits("QComboBox") &&
				   WidgetChanged(w)) {
				auto cb = dynamic_cast<QComboBox *>(w);
				config_set_string(
					config, QT_TO_UTF8(sectionName),
					QT_TO_UTF8(label->text()),
					QT_TO_UTF8(
						cb->currentData().toString()));
			} else if (w->inherits("QLineEdit") &&
				   w->inherits("QComboBox") &&
				   w->inherits("QDoubleSpinBox") &&
				   w->inherits("ColorPickingWidget")) {
				blog(LOG_WARNING,
				     "Unknown theme customisation field type: %s",
				     w->metaObject()->className());
			}
		}
	}

	/* Set the theme again to force reloading customisations */
	App()->SetTheme(currentTheme->id);
}

void OBSBasicSettings::on_theme_activated(int)
{
	LoadAppearanceSettings(true);
}
