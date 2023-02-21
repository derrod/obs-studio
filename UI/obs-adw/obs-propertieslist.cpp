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
