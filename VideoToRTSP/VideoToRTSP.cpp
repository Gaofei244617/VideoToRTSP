#include <QMessageBox>
#include <QDir>
#include <filesystem>
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
			QMessageBox::about(nullptr, "错误", "无法启动推流服务");
		}
	}
	else
	{
		QMessageBox::about(nullptr, "错误", "Can not find mediamtx.exe");
	}
}

VideoToRTSP::~VideoToRTSP()
{
	ui->tableWidget->stopAll();
	mediamtx->kill();

	delete mediamtx;
	delete ui;
}