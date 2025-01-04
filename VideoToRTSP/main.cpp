#include <QtWidgets/QApplication>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "video_to_rtsp.h"

int main(int argc, char* argv[])
{
	// 日志设置
	auto logger = spdlog::basic_logger_mt("file", "VideoToRTSP.log");
	spdlog::set_default_logger(logger);
	spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

	QApplication a(argc, argv);
	VideoToRTSP w;
	w.show();

	return a.exec();
}