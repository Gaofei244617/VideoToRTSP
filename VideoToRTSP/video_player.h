#pragma once

#include <thread>
#include <QtWidgets/QWidget>
#include <QImage>
#include "video_decoder.h"
#include "ui_video_player.h"

/*** 视频播放组件 ***/
class VideoPlayer : public QWidget
{
	Q_OBJECT

public:
	VideoPlayer(QWidget* parent = nullptr);
	~VideoPlayer();

	void setTitle(const QString& title);
	void play(const std::string& video);

protected:
	void updateImage();
	void resizeEvent(QResizeEvent* event) override;
	void closeEvent(QCloseEvent* event) override;

protected:
	Ui::VideoPlayer* ui;

	std::shared_ptr<ImageQueue> m_images;
	std::shared_ptr<VideoDecoder> m_decoder;
	std::thread t;
	std::thread t2;
};
