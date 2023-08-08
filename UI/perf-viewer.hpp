#pragma once

#include <QDialog>
#include <QPlainTextEdit>
#include "obs-app.hpp"

#include "ui_OBSPerfViewer.h"

class OBSPerfViewer : public QDialog {
	Q_OBJECT

	std::unique_ptr<Ui::OBSPerfViewer> ui;
	std::unique_ptr<QThread> updater;

private slots:
	void Update(const QString &text);

public:
	OBSPerfViewer(QWidget *parent = 0);
	~OBSPerfViewer() override;
};
