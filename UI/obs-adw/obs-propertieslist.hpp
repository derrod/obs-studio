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
	QList<OBSActionBaseClass *> *rows() const { return rowsList; }

private:
	QVBoxLayout *layout;
	QList<OBSActionBaseClass *> *rowsList;
};

/**
* Spacer with only cosmetic functionality
*/
class OBSPropertiesListSpacer : public QFrame {
	Q_OBJECT
public:
	OBSPropertiesListSpacer(QWidget *parent = nullptr) : QFrame(parent)
	{
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	}
};
