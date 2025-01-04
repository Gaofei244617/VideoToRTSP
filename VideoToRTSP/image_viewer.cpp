#include "image_viewer.h"

ImageViewer::ImageViewer(QWidget* parent)
	: QWidget(parent),
	ui(new Ui::ImageViewer())
{
	ui->setupUi(this);

	// 允许显示图像后拖拽缩小窗口
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

void ImageViewer::setTitle(const QString& title)
{
	this->setWindowTitle(title);
}

void ImageViewer::resizeEvent(QResizeEvent* event)
{
	QSize size = ui->label->size();
	ui->label->setPixmap(QPixmap::fromImage(m_image.scaled(size)));
}