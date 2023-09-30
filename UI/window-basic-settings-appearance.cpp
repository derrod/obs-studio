#include "window-basic-settings-appearance.hpp"

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

		if (var.type == OBSThemeVariable::Alias)
			realVar = vars[var.name];
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
}

void OBSBasicSettings::SaveAppearanceSettings()
{
	config_t *config = GetGlobalConfig();

	OBSTheme *currentTheme = App()->GetTheme();
	if (savedTheme != currentTheme) {
		config_set_string(config, "Appearance", "Theme",
				  QT_TO_UTF8(currentTheme->id));
	}
}

void OBSBasicSettings::on_theme_activated(int)
{
	LoadAppearanceSettings(true);
}
