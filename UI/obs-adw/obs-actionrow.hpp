#pragma once

#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <QLayout>
#include <QScrollArea>
#include <QMouseEvent>
#include <QCheckBox>

#include "obs-propertieslist.hpp"
#include "obs-toggleswitch.hpp"

/**
* Base class mostly so adding stuff to a list is easier
*/
class OBSActionBaseClass : public QFrame {
	Q_OBJECT

public:
	OBSActionBaseClass(QWidget *parent = nullptr) : QFrame(parent){};
};

/**
* Proxy for QScrollArea for QSS styling
*/
class OBSScrollArea : public QScrollArea {
	Q_OBJECT
public:
	OBSScrollArea(QWidget *parent = nullptr) : QScrollArea(parent) {}
};

/**
* Generic OBS row widget containing one or more controls
*/
class OBSActionRow : public OBSActionBaseClass {
	Q_OBJECT

public:
	OBSActionRow(const QString &name, QWidget *parent = nullptr);
	OBSActionRow(const QString &name, const QString &desc,
		     QWidget *parent = nullptr);

	void setPrefix(QWidget *w, bool auto_connect = true);
	void setSuffix(QWidget *w, bool auto_connect = true);

	QWidget *prefix() const { return _prefix; }
	QWidget *suffix() const { return _suffix; }

	void setPrefixEnabled(bool enabled);
	void setSuffixEnabled(bool enabled);

signals:
	void clicked();

protected:
	void mouseReleaseEvent(QMouseEvent *) override;
	bool hasSubtitle() const { return descLbl != nullptr; }

private:
	QGridLayout *layout;

	QLabel *nameLbl = nullptr;
	QLabel *descLbl = nullptr;

	QWidget *_prefix = nullptr;
	QWidget *_suffix = nullptr;
};

/**
* Collapsible Generic OBS property container
*/
class OBSCollapsibleActionRow : public OBSActionBaseClass {
	Q_OBJECT

public:
	// ToDo: Figure out a better way to handle those options
	OBSCollapsibleActionRow(const QString &name,
				const QString &desc = nullptr,
				bool toggleable = false,
				QWidget *parent = nullptr);

	OBSToggleSwitch *getSwitch() const { return sw; }

	void addProperty(OBSActionBaseClass *ar);

private:
	void collapse(bool collapsed);
	void toggleVisibility();

	QPixmap extendDown;
	QPixmap extendUp;

	QVBoxLayout *layout;
	QPropertyAnimation *anim;
	QLabel *extendIcon;

	OBSActionRow *ar;
	OBSScrollArea *sa;
	OBSToggleSwitch *sw = nullptr;
	OBSPropertiesList *plist;
};
