#pragma once

#include <QCheckbox>
#include <QLabel>
#include <QWidget>

class OBSSettingsContainer : public QFrame {
	Q_OBJECT

public:
	OBSSettingsContainer(QWidget *parent = nullptr, const QString &name = nullptr,
			     const QString &desc = nullptr);

private:
	QLabel *nameLbl;
	QLabel *descLbl;

	QCheckBox *cbox;
};
