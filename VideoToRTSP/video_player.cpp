#include <QPixmap>
#include "video_player.h"

VideoPlayer::VideoPlayer(QWidget* parent)
	: QWidget(parent),
	ui(new Ui::VideoPlayer())
{
	ui->setupUi(this);

	// 允许显示图像后拖拽缩小窗口
	ui->player->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
}

VideoPlayer::~VideoPlayer()
{
	delete ui;
}

void VideoPlayer::setTitle(const QString& title)
{
	this->setWindowTitle(title);
}

void VideoPlayer::play(const std::string& video)
{
	m_images = std::make_shared<ImageQueue>();
	m_decoder = std::make_shared<VideoDecoder>(video, m_images);
	t = std::thread(&VideoDecoder::decode, m_decoder.get());
	t2 = std::thread(&VideoPlayer::updateImage, this);
}

void VideoPlayer::updateImage()
{
	while (true)
	{
		if (m_images->size() > 0)
		{
			QSize size = ui->player->size();
			QImage image = m_images->pop();
			ui->player->setPixmap(QPixmap::fromImage(image.scaled(size)));

			std::this_thread::sleep_for(std::chrono::milliseconds(30));
		}
	}
}

void VideoPlayer::resizeEvent(QResizeEvent* event)
{
	QSize size = ui->player->size();
	QPixmap pixmap = ui->player->pixmap();
	ui->player->setPixmap(pixmap.scaled(size));
}

void VideoPlayer::closeEvent(QCloseEvent* event)
{
	m_decoder->stop();
	t.join();
}