#pragma once

#include <QtWidgets/QWidget>
#include <QResizeEvent>
#include <QImage>
#include "ui_image_viewer.h"

class ImageViewer : public QWidget
{
	Q_OBJECT

public:
	ImageViewer(QWidget* parent = nullptr);
	~ImageViewer();

	void setImage(const QImage& image);
	void setTitle(const QString& title);

protected:
	void resizeEvent(QResizeEvent* event) override;

public:
	Ui::ImageViewer* ui;
	QImage m_image;
};
