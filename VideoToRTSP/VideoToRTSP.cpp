#include <QMessageBox>
#include <QDir>
#include <filesystem>
#include <spdlog/spdlog.h>
#include "VideoToRTSP.h"

VideoToRTSP::VideoToRTSP(QWidget* parent)
	: QMainWindow(parent),
	ui(new Ui::VideoToRTSPClass()),
	mediamtx(new QProcess)
{
	ui->setupUi(this);

	if (std::filesystem::exists("mediamtx.exe"))
	{
		// 启动mediamtx推流服务
		mediamtx->start("mediamtx.exe");
		if (!mediamtx->waitForStarted())
		{
			spdlog::error("无法启动推流服务");
			QMessageBox::about(nullptr, "错误", "无法启动推流服务");
		}
		else
		{
			spdlog::info("Start mediamtx.exe success");
		}
	}
	else
	{
		spdlog::error("Can not find mediamtx.exe");
		QMessageBox::about(nullptr, "错误", "Can not find mediamtx.exe");
	}
}

VideoToRTSP::~VideoToRTSP()
{
	ui->tableWidget->stopAll();
	mediamtx->kill();
	spdlog::info("Stop mediamtx.exe");

	delete mediamtx;
	delete ui;
}