#include "obs-propertieslist.hpp"
#include "obs-actionrow.hpp"

OBSPropertiesList::OBSPropertiesList(QWidget *parent) : QFrame(parent)
{
	layout = new QVBoxLayout();
	layout->setSpacing(0);
	layout->setContentsMargins(0, 0, 0, 0);

	rowsList = new QList<OBSActionBaseClass *>;

	setLayout(layout);
}

/* Note: This function takes ownership of the added widget
 * and it may be deleted when the properties list is destoryed
 * or the clear() method is called! */
void OBSPropertiesList::addProperty(OBSActionBaseClass *ar)
{
	// Add custom spacer once more than one element exists
	if (layout->count() > 0)
		layout->addWidget(new OBSPropertiesListSpacer(this));

	ar->setParent(this);
	rowsList->append(ar);
	layout->addWidget(ar);
	adjustSize();
}

void OBSPropertiesList::clear()
{
	rowsList->clear();
	QLayoutItem *item = layout->takeAt(0);

	while (item) {
		if (item->widget())
			item->widget()->deleteLater();
		delete item;

		item = layout->takeAt(0);
	}

	adjustSize();
}
