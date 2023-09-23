#pragma once

#include "window-basic-settings.hpp"
#include "window-basic-main.hpp"
#include "obs-frontend-api.h"
#include "obs-app.hpp"
#include "qt-wrappers.hpp"

#include <QColorDialog>
#include <QMetaEnum>

class ColorPickingWidget : public QWidget {
	Q_OBJECT

public:
	ColorPickingWidget(QColor color, const QString &name,
			   QWidget *parent = nullptr);

	QColor getColor() const { return m_color; }

	void SetColor(QColor color);

signals:
	void colorChanged();

private:
	QString m_name;
	QColor m_color;
	QPushButton *m_button = nullptr;
};

class PaletteColorPicker : public QWidget {
	Q_OBJECT

public:
	PaletteColorPicker(const QPalette *palette,
			   const QPalette *themePalette,
			   QPalette::ColorRole role, QWidget *parent = nullptr);

	QPalette::ColorRole getRole() const { return m_role; };
	QColor getColor(QPalette::ColorGroup group) const;

signals:
	void colorChanged();

private slots:
	void PickerChanged();
	void ResetClicked();

private:
	QColor m_activeColor;
	QColor m_inactiveColor;
	QColor m_disabledColor;

	QColor m_defaultActiveColor;
	QColor m_defaultInactiveColor;
	QColor m_defaultDisabledColor;

	QPalette::ColorRole m_role;

	QHBoxLayout *layout = nullptr;
	QPushButton *resetBtn = nullptr;

	ColorPickingWidget *pickerActive = nullptr;
	ColorPickingWidget *pickerInactive = nullptr;
	ColorPickingWidget *pickerDisabled = nullptr;
};

class ThemePreviewButton : public QPushButton {
	Q_OBJECT

public:
	ThemePreviewButton(const QString &name, const QPalette &palette,
			   QWidget *parent = nullptr);

	QSize sizeHint() const override;

private:
	QPixmap image;
	QVBoxLayout *layout = nullptr;
	QLabel *label = nullptr;
	QLabel *preview = nullptr;
};
