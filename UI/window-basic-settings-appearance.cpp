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

#include "platform.hpp"
#include "ui-config.h"

using namespace std;

static bool WidgetChanged(QWidget *widget)
{
	return widget->property("changed").toBool();
}

static void SetStyle(QPushButton *btn, QColor color)
{
	color.setAlpha(255);
	QPalette palette = QPalette(color);

	/* Ensure button will fit largest possible string for our font */
	btn->setText("#000000");
	btn->adjustSize();

	btn->setText(color.name(QColor::HexRgb));
	btn->setPalette(palette);
	btn->setStyleSheet(QString("background-color: %1; color: %2;")
				   .arg(palette.color(QPalette::Window)
						.name(QColor::HexRgb))
				   .arg(palette.color(QPalette::WindowText)
						.name(QColor::HexRgb)));
}

ColorPickingWidget::ColorPickingWidget(QColor color, const QString &name,
				       QWidget *parent)
	: QWidget(parent)
{
	m_name = name;
	m_color = color;
	m_button = new QPushButton(this);
	SetStyle(m_button, color);

	connect(m_button, &QPushButton::clicked, this, [&](bool) {
		auto newColor = QColorDialog::getColor(m_color, this, m_name);
		if (newColor.isValid() && newColor != m_color)
			SetColor(newColor);
	});
}

void ColorPickingWidget::SetColor(QColor color)
{
	SetStyle(m_button, color);
	m_color = color;
	emit colorChanged();
}

PaletteColorPicker::PaletteColorPicker(const QPalette *palette,
				       const QPalette *themePalette,
				       QPalette::ColorRole role,
				       QWidget *parent)
	: QWidget(parent),
	  m_role(role)
{
	m_activeColor = palette->color(QPalette::Active, m_role);
	m_inactiveColor = palette->color(QPalette::Inactive, m_role);
	m_disabledColor = palette->color(QPalette::Disabled, m_role);

	m_defaultActiveColor = themePalette->color(QPalette::Active, m_role);
	m_defaultInactiveColor =
		themePalette->color(QPalette::Inactive, m_role);
	m_defaultDisabledColor =
		themePalette->color(QPalette::Disabled, m_role);

	/* Create layout */
	layout = new QHBoxLayout(this);

	/* Create picker widgets */
	QString groupName = QTStr("Basic.Settings.Appearance.Group.Active");
	pickerActive = new ColorPickingWidget(m_activeColor, groupName, this);
	pickerActive->setToolTip(groupName);
	layout->addWidget(pickerActive);

	groupName = QTStr("Basic.Settings.Appearance.Group.Inctive");
	pickerInactive =
		new ColorPickingWidget(m_inactiveColor, groupName, this);
	pickerInactive->setToolTip(groupName);
	layout->addWidget(pickerInactive);

	groupName = QTStr("Basic.Settings.Appearance.Group.Disabled");
	pickerDisabled =
		new ColorPickingWidget(m_disabledColor, groupName, this);
	pickerDisabled->setToolTip(groupName);
	layout->addWidget(pickerDisabled);

	/* Create reset button */
	resetBtn = new QPushButton(QTStr("Reset"), this);
	resetBtn->setEnabled(m_activeColor != m_defaultActiveColor ||
			     m_inactiveColor != m_defaultActiveColor ||
			     m_disabledColor != m_defaultDisabledColor);
	layout->addWidget(resetBtn);

	/* Hook it all up! */
	connect(pickerActive, &ColorPickingWidget::colorChanged, this,
		&PaletteColorPicker::PickerChanged);
	connect(pickerInactive, &ColorPickingWidget::colorChanged, this,
		&PaletteColorPicker::PickerChanged);
	connect(pickerDisabled, &ColorPickingWidget::colorChanged, this,
		&PaletteColorPicker::PickerChanged);

	connect(resetBtn, &QPushButton::clicked, this,
		&PaletteColorPicker::ResetClicked);
}

QColor PaletteColorPicker::getColor(QPalette::ColorGroup group) const
{
	switch (group) {
	case QPalette::Active:
		return m_activeColor;
	case QPalette::Inactive:
		return m_inactiveColor;
	case QPalette::Disabled:
		return m_disabledColor;
	default:
		return {};
	}
}

void PaletteColorPicker::PickerChanged()
{
	resetBtn->setEnabled(true);
	m_activeColor = pickerActive->getColor();
	m_inactiveColor = pickerInactive->getColor();
	m_disabledColor = pickerDisabled->getColor();
	emit colorChanged();
}

void PaletteColorPicker::ResetClicked()
{
	pickerActive->SetColor(m_defaultActiveColor);
	pickerInactive->SetColor(m_defaultInactiveColor);
	pickerDisabled->SetColor(m_defaultDisabledColor);
	resetBtn->setEnabled(false);
}

static QPixmap FuglyRecolouringHack(const QPalette &palette)
{
	QFile svgFile(":settings/images/settings/theme_preview.svg");

	if (!svgFile.open(QFile::ReadOnly))
		blog(LOG_WARNING, "Failed to open SVG D:");

	QByteArray svg = svgFile.readAll();

	QColor previewBackground = palette.color(QPalette::ColorRole::Mid);
	svg = svg.replace("#a5a5a5", 7, QT_TO_UTF8(previewBackground.name()),
			  7);
	QColor windowFrame = palette.color(QPalette::ColorRole::Light);
	svg = svg.replace("#ff00e4", 7, QT_TO_UTF8(windowFrame.name()), 7);
	QColor docks = palette.color(QPalette::ColorRole::Dark);
	svg = svg.replace("#555555", 7, QT_TO_UTF8(docks.name()), 7);
	QColor button = palette.color(QPalette::ColorRole::Button);
	svg = svg.replace("#929292", 7, QT_TO_UTF8(button.name()), 7);

	QSvgRenderer renderer(svg);
	QPixmap pm(128, 128);
	pm.fill(QColor(0, 0, 0, 0));
	QPainter painter(&pm);
	renderer.render(&painter, pm.rect());

	return pm;
}

ThemePreviewButton::ThemePreviewButton(const QString &name,
				       const QPalette &palette, QWidget *parent)
	: QPushButton(parent)
{
	setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

	layout = new QVBoxLayout(this);

	label = new QLabel(name, this);
	label->setAlignment(Qt::AlignHCenter);

	preview = new QLabel(this);
	image = FuglyRecolouringHack(palette);
	preview->setPixmap(image);
	preview->setFixedSize(preview->pixmap().size());
	preview->setAlignment(Qt::AlignCenter);

	layout->addWidget(preview);
	layout->addWidget(label);
	layout->setAlignment(Qt::AlignCenter);

	setFlat(true);
	setLayout(layout);
	// setFixedSize(512, 512);
}

QSize ThemePreviewButton::sizeHint() const
{
	QSize ret;
	auto labelSize = label->minimumSizeHint();
	auto imageSize = preview->sizeHint();

	ret.setHeight(labelSize.height() + imageSize.height() + 24);
	ret.setWidth(imageSize.width() + 12);

	return ret;
}

void OBSBasicSettings::InitAppearancePage() {}

void OBSBasicSettings::LoadThemeList()
{
	// Todo clean this up
	QFormLayout *form =
		dynamic_cast<QFormLayout *>(ui->appearanceGeneral->layout());

	QScrollArea *test = new QScrollArea(this);
	form->addRow(test);
	test->setWidgetResizable(true);

	QFrame *testFrame = new QFrame(test);
	testFrame->setLayout(new QHBoxLayout(testFrame));
	test->setWidget(testFrame);

	/* Save theme if user presses Cancel */
	savedTheme = string(App()->GetTheme());

	ui->theme->hide();

	ui->theme->clear();
	QSet<QString> uniqueSet;
	string themeDir;
	char userThemeDir[512];
	int ret = GetConfigPath(userThemeDir, sizeof(userThemeDir),
				"obs-studio/themes/");
	GetDataFilePath("themes/", themeDir);

	/* Check user dir first. */
	if (ret > 0) {
		QDirIterator it(QString(userThemeDir), QStringList() << "*.qss",
				QDir::Files);
		while (it.hasNext()) {
			it.next();
			QString name = it.fileInfo().completeBaseName();
			ui->theme->addItem(name, name);
			// Todo add to image list
			uniqueSet.insert(name);
		}
	}

	/* Check shipped themes. */
	QDirIterator uIt(QString(themeDir.c_str()), QStringList() << "*.qss",
			 QDir::Files);
	while (uIt.hasNext()) {
		uIt.next();
		QString name = uIt.fileInfo().completeBaseName();
		QString value = name;

		if (name == DEFAULT_THEME)
			name += " " + QTStr("Default");

		if (!uniqueSet.contains(value) && name != "Default") {
			ui->theme->addItem(name, value);
			string path = App()->GetTheme(value.toStdString(), "");
			QPalette pal = name == "System"
					       ? App()->GetDefaultPalette()
					       : App()->ParseExtraThemeData(
							 path.c_str());

			testFrame->layout()->addWidget(
				new ThemePreviewButton(name, pal, testFrame));
		}
	}

	auto item = testFrame->layout()->itemAt(0);
	if (item) {
		int height = item->widget()->sizeHint().height();
		testFrame->setMinimumHeight(height);
		test->setMinimumHeight(testFrame->minimumSizeHint().height());
	}

	int idx = ui->theme->findData(QT_UTF8(App()->GetTheme()));
	if (idx != -1)
		ui->theme->setCurrentIndex(idx);
}

void OBSBasicSettings::LoadAppearanceSettings(bool skip_themes)
{
	if (!skip_themes)
		LoadThemeList();

	ui->paletteGroupBox->setChecked(config_get_bool(
		GetGlobalConfig(), "Appearance", "UseCustomPalette"));

	QPalette palette = qApp->palette();
	QPalette themePalette = App()->GetThemePalette();
	QFormLayout *form =
		dynamic_cast<QFormLayout *>(ui->paletteGroupBox->layout());

	if (!form->isEmpty()) {
		QLayoutItem *item;
		while ((item = form->takeAt(0))) {
			delete item->widget();
			delete item;
		}
	}

	// Todo this needs to clear the form otherwise things get ugly
	QString translation("Basic.Settings.Appearance.Palette.%1");

	QMetaEnum roleMeta = QMetaEnum::fromType<QPalette::ColorRole>();
	QMetaEnum groupMeta = QMetaEnum::fromType<QPalette::ColorGroup>();

	for (int i = 0; i < roleMeta.keyCount(); i++) {
		auto role = QPalette::ColorRole(i);
		if (role == QPalette::NColorRoles)
			break;

		auto label = new QLabel(translation.arg(roleMeta.valueToKey(i)),
					ui->paletteGroupBox);
		auto picker = new PaletteColorPicker(&palette, &themePalette,
						     role, ui->paletteGroupBox);
		label->setBuddy(picker);
		form->addRow(label, picker);

		connect(picker, &PaletteColorPicker::colorChanged, this,
			[&] { AppearanceChanged(); });
	}
}

void OBSBasicSettings::SaveAppearanceSettings()
{
	config_t *config = GetGlobalConfig();

	int themeIndex = ui->theme->currentIndex();
	QString themeData = ui->theme->itemData(themeIndex).toString();

	if (WidgetChanged(ui->theme)) {
		savedTheme = themeData.toStdString();
		config_set_string(GetGlobalConfig(), "Appearance", "Theme",
				  QT_TO_UTF8(themeData));
	}

	bool enabled = ui->paletteGroupBox->isChecked();
	config_set_bool(config, "Appearance", "UseCustomPalette", enabled);
	if (!enabled)
		return;

	QMetaEnum roleMeta = QMetaEnum::fromType<QPalette::ColorRole>();
	QMetaEnum groupMeta = QMetaEnum::fromType<QPalette::ColorGroup>();

	QPalette palette = App()->GetThemePalette();
	QString sectionName =
		QString("Appearance.CustomPalette.%1").arg(App()->GetTheme());

	constexpr QPalette::ColorGroup groups[] = {
		QPalette::Active, QPalette::Inactive, QPalette::Disabled};

	QFormLayout *form =
		dynamic_cast<QFormLayout *>(ui->paletteGroupBox->layout());
	for (int i = 0; i < form->rowCount(); i++) {
		QLayoutItem *item = form->itemAt(i, QFormLayout::FieldRole);
		if (!item)
			continue;

		auto picker =
			dynamic_cast<PaletteColorPicker *>(item->widget());
		if (!picker)
			continue;

		for (const QPalette::ColorGroup group : groups) {
			QColor color = picker->getColor(group);
			QPalette::ColorRole role = picker->getRole();

			QString configKey = QString("%1.%2").arg(
				roleMeta.valueToKey(role),
				groupMeta.valueToKey(group));

			if (palette.color(group, role) != color) {
				config_set_string(
					config, QT_TO_UTF8(sectionName),
					QT_TO_UTF8(configKey),
					QT_TO_UTF8(color.name(QColor::HexRgb)));
			} else {
				config_remove_value(config,
						    QT_TO_UTF8(sectionName),
						    QT_TO_UTF8(configKey));
			}
		}
	}

	/* Set the theme again to force reloading customisations */
	App()->SetTheme(themeData.toStdString());
}
