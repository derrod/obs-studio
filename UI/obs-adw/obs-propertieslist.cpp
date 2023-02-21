#include "obs-propertieslist.hpp"
#include "obs-actionrow.hpp"

OBSPropertiesList::OBSPropertiesList(QWidget *parent) : QFrame(parent)
{
	layout = new QVBoxLayout();
	layout->setSpacing(0);
	layout->setContentsMargins(0, 0, 0, 0);

	plist = new QList<OBSActionBaseClass *>;

	setLayout(layout);
}

void OBSPropertiesList::addProperty(OBSActionBaseClass *ar)
{
	// All non-first objects get a special name so they can have a top border.
	// Alternatively we could insert a widget here that acts as a separator and could
	// be styled separately.
	if (layout->count() > 0) {
		ar->setObjectName("secondary");
		/*
		QWidget *spacer = new QWidget(this);
		spacer->setObjectName("obsSpacer");
		spacer->setSizePolicy(QSizePolicy::Expanding,
				      QSizePolicy::Fixed);
		layout->addWidget(spacer);
		*/
	}

	ar->setParent(this);
	plist->append(ar);
	layout->addWidget(ar);
	adjustSize();
}
