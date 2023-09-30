#pragma once

#include "window-basic-settings.hpp"
#include "window-basic-main.hpp"
#include "obs-frontend-api.h"
#include "obs-app.hpp"
#include "qt-wrappers.hpp"

class ColorPickingWidget : public QWidget {
	Q_OBJECT

public:
	ColorPickingWidget(QColor color, QColor defaultColor, QString name,
			   QWidget *parent = nullptr);

	QColor getColor() const { return m_color; }
	QColor getDefaultColor() const { return m_defaultColor; }

	void SetColor(QColor color);

signals:
	void colorChanged();

private slots:
	void ResetClicked();

private:
	QString m_name;
	QColor m_color;
	QColor m_defaultColor;

	QHBoxLayout *layout = nullptr;
	QPushButton *button = nullptr;
	QPushButton *resetBtn = nullptr;
};
