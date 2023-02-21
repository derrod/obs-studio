#pragma once

#include <QFrame>
#include <QWidget>
#include <QLayout>

class OBSActionBaseClass;

class OBSPropertiesList : public QFrame {
	Q_OBJECT

public:
	OBSPropertiesList(QWidget *parent = nullptr);

	void addProperty(OBSActionBaseClass *ar);

	// This has too many fucking *'s
	QList<OBSActionBaseClass *> *properties() const { return plist; }

private:
	QVBoxLayout *layout;
	QList<OBSActionBaseClass *> *plist;
};
