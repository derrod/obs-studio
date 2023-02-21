#pragma once

#include <QLayout>
#include <QLabel>
#include <QWidget>
#include <QMouseEvent>

#include "obs-actionrow.hpp"
#include "obs-propertieslist.hpp"
#include "obs-toggleswitch.hpp"

class OBSGroupBox : public QFrame {
	Q_OBJECT

public:
	OBSGroupBox(QWidget *parent = nullptr);
	OBSGroupBox(const QString &name, QWidget *parent = nullptr);
	OBSGroupBox(const QString &name, bool checkable,
		    QWidget *parent = nullptr);
	OBSGroupBox(const QString &name, const QString &desc,
		    QWidget *parent = nullptr);
	OBSGroupBox(const QString &name, const QString &desc,
		    bool checkable = false, QWidget *parent = nullptr);

	OBSPropertiesList *properties() const { return plist; }

	// ToDo add event for checkable group being enabled/disabled
	// ToDo allow setting enabled state
	// (Maybe) ToDo add option for hiding properties list when disabled

	void addProperty(OBSActionBaseClass *ar);

private:
	QGridLayout *layout = nullptr;

	QLabel *nameLbl = nullptr;
	QLabel *descLbl = nullptr;

	OBSPropertiesList *plist = nullptr;

	OBSToggleSwitch *toggleSwitch = nullptr;
};
