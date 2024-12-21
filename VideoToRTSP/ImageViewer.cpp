#include "ImageViewer.h"

ImageViewer::ImageViewer(QWidget* parent)
	: QWidget(parent),
	ui(new Ui::ImageViewer())
{
	ui->setupUi(this);
	ui->label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
}

ImageViewer::~ImageViewer()
{
	delete ui;
}

void ImageViewer::setImage(const QImage& image)
{
	m_image = image;
	ui->label->setPixmap(QPixmap::fromImage(image.scaled(960, 540)));
}

void ImageViewer::resizeEvent(QResizeEvent* event)
{
	QSize size = ui->label->size();
	ui->label->setPixmap(QPixmap::fromImage(m_image.scaled(size)));
}