#pragma once

#include <QTableWidget>
#include <QFileInfo>
#include <QEvent>
#include <QMouseEvent>
#include <memory>
#include "video_info.h"
#include "send_rtsp.h"

class VideoTableWidget : public QTableWidget
{
public:
	explicit VideoTableWidget(QWidget* parent = nullptr);
	~VideoTableWidget();

	void addTableItem(const QString& video);
	void stopAll();

protected:
	void onPushButtonClicked();    // 推流button
	void onDelButtonClicked();     // 删除button
	void onActivated(int index);   // IP comboBox

	void dragEnterEvent(QDragEnterEvent* event) override;    // 文件拖拽: 进入
	void dragMoveEvent(QDragMoveEvent* event) override;      // 文件拖拽: 移动
	void dropEvent(QDropEvent* event) override;              // 文件拖拽: 释放

	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void mouseDoubleClickEvent(QMouseEvent* event) override;

	void showToolTip(QMouseEvent* event);

protected:
	QList<VideoInfo> m_videos;
	QList<std::shared_ptr<RtspSender>> m_senders;  // 推流器
};
