#include "window-basic-settings.hpp"
#include "window-basic-main.hpp"
#include "obs-frontend-api.h"
#include "obs-app.hpp"
#include "qt-wrappers.hpp"

#include <QColorDialog>
#include <QMetaEnum>

class ColorPickPreview : public QWidget {
	Q_OBJECT

public:
	ColorPickPreview(QString &name, QPalette::ColorGroup group,
		     QPalette::ColorRole role, QColor color,
		     QWidget *parent = nullptr);

	QColor getColor() const { return m_color; }
	QPalette::ColorRole getRole() const { return m_role; }
	QPalette::ColorGroup getGroup() const { return m_group; }

signals:
	void colorChanged();

private:
	void SetColor(QColor color);

	const QPalette::ColorRole m_role;
	const QPalette::ColorGroup m_group;
	const QString m_paletteName;
	QColor m_color;

	QHBoxLayout *m_layout = nullptr;
	QPushButton *m_button = nullptr;
	QLabel *m_colorPreview = nullptr;
};
