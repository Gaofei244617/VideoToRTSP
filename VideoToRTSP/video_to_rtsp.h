#pragma once

#include <QtWidgets/QMainWindow>
#include <QProcess>
#include "ui_video_to_rtsp.h"

QT_BEGIN_NAMESPACE
namespace Ui { class VideoToRTSPClass; };
QT_END_NAMESPACE

class VideoToRTSP : public QMainWindow
{
	Q_OBJECT

public:
	VideoToRTSP(QWidget* parent = nullptr);
	~VideoToRTSP();

private:
	Ui::VideoToRTSPClass* ui;
	QProcess* mediamtx;
};
