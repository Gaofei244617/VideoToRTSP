#include <filesystem>
#include <QHeaderView>
#include <QPushButton>
#include <QComboBox>
#include <QHostInfo>
#include <QHostAddress>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QMessageBox>
#include <QToolTip>
#include <QDateTime>
#include <QDesktopServices>
#include <spdlog/spdlog.h>
#include "video_table_widget.h"

static int count = 1;

// 获取本机IPv4
static QStringList getLocalIPs()
{
	QStringList ips;

	QString hostName = QHostInfo::localHostName(); // 主机名
	QList<QHostAddress> addrs = QHostInfo::fromName(hostName).addresses(); // 通过主机名获取所有IP地址

	// 所有IPv4地址
	for (const QHostAddress& address : addrs)
	{
		if (address.protocol() == QAbstractSocket::IPv4Protocol)
		{
			ips << address.toString();
		}
	}

	return ips;
}

static std::string toString(EncodeType encode)
{
	std::string type;
	switch (encode)
	{
	case EncodeType::H264:
		type = "H.264";
		break;
	case EncodeType::HEVC:
		type = "H.265";
		break;
	case EncodeType::Other:
		type = "Other";
		break;
	}

	return type;
}

static QString toString(const VideoInfo& videoInfo)
{
	QString info;

	info += "视频: " + QString::fromLocal8Bit(videoInfo.url) + "\n";
	info += "文件: " + QString::number(videoInfo.size / 1024.0 / 1024) + " MB\n";
	info += "时长: " + QString::number(videoInfo.duration) + " 秒\n";
	info += "帧率: " + QString::number(videoInfo.fps) + " fps\n";
	info += "宽高: " + QString::number(videoInfo.width) + "x" + QString::number(videoInfo.height) + "\n";
	info += "编码格式: " + QString::fromStdString(toString(videoInfo.encode));

	return info;
}

VideoTableWidget::VideoTableWidget(QWidget* parent) : QTableWidget(parent)
{
	this->setColumnCount(7);

	// 添加标题栏
	QStringList headers;
	headers << "序号" << "视频" << "URL" << "本机IP" << "进度" << "推流" << "";
	this->setHorizontalHeaderLabels(headers);

	// 设置表头字体
	QFont font;
	font.setFamily("Microsoft YaHei");
	font.setBold(true);
	this->horizontalHeader()->setFont(font);

	// 设置表头样式
	this->horizontalHeader()->setStyleSheet("QHeaderView::section { height: 30px; background-color: #f0f0f0; color: black;}");

	// 设置列宽
	this->horizontalHeader()->resizeSection(0, 50);
	this->horizontalHeader()->resizeSection(1, 200);
	this->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeMode::Stretch);
	this->horizontalHeader()->resizeSection(3, 130);
	this->horizontalHeader()->resizeSection(4, 130);
	this->horizontalHeader()->resizeSection(5, 80);
	this->horizontalHeader()->resizeSection(6, 80);

	this->setFont(QFont("Microsoft YaHei"));

	// 隐藏行号
	this->verticalHeader()->setVisible(false);

	// 行高
	this->verticalHeader()->setDefaultSectionSize(30);

	// 颜色间隔显示
	this->setAlternatingRowColors(true);

	// 去除选中虚线框
	this->setFocusPolicy(Qt::NoFocus);

	// 启用鼠标追踪
	this->setMouseTracking(true);

	// 鼠标悬停样式
	this->setStyleSheet("QTableWidget::item:hover {color: white; background-color: green;} QLineEdit { background-color: white; }");

	// 支持鼠标拖放文件
	this->setAcceptDrops(true);

	// ToolTip
	QFont font2("Microsoft YaHei");
	font2.setPointSize(9);
	QToolTip::setFont(font2);
}

VideoTableWidget::~VideoTableWidget()
{
	stopAll();
	spdlog::info("Program exitp");
}

// 添加推流视频
void VideoTableWidget::addTableItem(const QString& video)
{
	std::filesystem::path videoPath(video.toLocal8Bit().toStdString());

	QFileInfo fileInfo(video);
	QString suffix = fileInfo.suffix(); // 文件后缀
	if (suffix != "ts" && suffix != "mp4" && suffix != "h264" && suffix != "h265" && suffix != "flv" && suffix != "avi")
	{
		spdlog::error("非视频文件: {}", videoPath.filename().string());
		QMessageBox::about(nullptr, "错误", "非视频文件: " + fileInfo.fileName());
		return;
	}

	// 视频信息
	VideoInfo videoInfo = GetVideoInfo(videoPath.string());
	if (videoInfo.encode != EncodeType::H264 && videoInfo.encode != EncodeType::HEVC)
	{
		spdlog::error("不支持的视频编码格式: {}", videoPath.filename().string());
		QMessageBox::about(nullptr, "错误", "不支持的视频编码格式: " + fileInfo.fileName());
		return;
	}

	spdlog::info("Add video: [{}], Duration: {} s, {}x{}, fps: {}, Encode: {}", videoPath.filename().string(), videoInfo.duration, videoInfo.width, videoInfo.height, videoInfo.fps, toString(videoInfo.encode));

	/**** 添加数据 ****/
	int row = this->rowCount();
	this->insertRow(row);

	// [0] 序号
	this->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));

	// [1] 视频文件
	this->setItem(row, 1, new QTableWidgetItem(fileInfo.fileName()));

	// [3] 本机IP
	QComboBox* comboBox = new QComboBox();
	for (const auto& ip : getLocalIPs())
	{
		comboBox->addItem(ip);
	}
	comboBox->addItem("127.0.0.1");
	comboBox->setStyleSheet("QComboBox{border-radius: 0px;} QComboBox::drop-down {border: none;} QComboBox::hover {background-color: green;}");
	this->setCellWidget(row, 3, comboBox);
	QObject::connect(comboBox, &QComboBox::activated, this, &VideoTableWidget::onActivated);

	// [2] RTSP地址
	QString url = getRtspUrl(video, comboBox->currentText());
	this->setItem(row, 2, new QTableWidgetItem(url));

	// [4] 状态
	this->setItem(row, 4, new QTableWidgetItem("Stop"));

	// [5] 推流
	QPushButton* pushBtn = new QPushButton("推流");
	// 设置border-radius后background-color才能生效
	pushBtn->setStyleSheet("QPushButton{border-radius: 0px;} QPushButton:hover {color: white; background-color: green; border-radius: 0px;}");
	this->setCellWidget(row, 5, pushBtn);
	connect(pushBtn, &QPushButton::clicked, this, &VideoTableWidget::onPushButtonClicked);

	// [6] 删除
	QPushButton* delBtn = new QPushButton("删除");
	delBtn->setStyleSheet("QPushButton{border-radius: 0px;} QPushButton:hover {color: white; background-color: #a52a2a; border-radius: 0px;}");
	this->setCellWidget(row, 6, delBtn);
	connect(delBtn, &QPushButton::clicked, this, &VideoTableWidget::onDelButtonClicked);

	m_videos.append(videoInfo);
	m_senders.emplaceBack(std::make_shared<RtspSender>());

	/**** 单元格样式 ****/
	// 文本对齐
	this->item(row, 0)->setTextAlignment(Qt::AlignCenter);
	this->item(row, 4)->setTextAlignment(Qt::AlignCenter);

	// 字体
	this->item(row, 0)->setFont(QFont("Microsoft YaHei"));
	this->item(row, 1)->setFont(QFont("Microsoft YaHei"));
	this->item(row, 2)->setFont(QFont("Microsoft YaHei"));
	this->cellWidget(row, 3)->setFont(QFont("Microsoft YaHei"));
	this->item(row, 4)->setFont(QFont("Microsoft YaHei"));
	this->cellWidget(row, 5)->setFont(QFont("Microsoft YaHei"));
	this->cellWidget(row, 6)->setFont(QFont("Microsoft YaHei"));

	// 编辑权限
	this->item(row, 0)->setFlags(this->item(row, 0)->flags() & (~Qt::ItemIsEditable));
	this->item(row, 1)->setFlags(this->item(row, 1)->flags() & (~Qt::ItemIsEditable));
	this->item(row, 4)->setFlags(this->item(row, 4)->flags() & (~Qt::ItemIsEditable));

	spdlog::info("Add video: [{}] success", videoPath.filename().string());
}

void VideoTableWidget::stopAll()
{
	for (const auto& sender : m_senders)
	{
		sender->stop();
	}
}

// 删除一行数据
void VideoTableWidget::onDelButtonClicked()
{
	QPushButton* button = dynamic_cast<QPushButton*>(sender());
	if (nullptr == button)
	{
		return;
	}

	int x = button->frameGeometry().x();
	int y = button->frameGeometry().y();
	QModelIndex index = this->indexAt(QPoint(x, y));
	int row = index.row();

	std::string video = m_videos[row].url;
	std::string url = this->item(row, 2)->text().toStdString();

	// 停止推流
	m_senders[row]->stop();
	spdlog::info("Stop {}", url);

	m_senders.removeAt(row);
	m_videos.removeAt(row);
	this->removeRow(row);

	// 刷新序号
	int cnt = this->rowCount();
	for (int i = 0; i < cnt; i++)
	{
		this->item(row, 0)->setText(QString::number(i + 1));
	}

	spdlog::info("Delete {} success", video);
}

// 推流
void VideoTableWidget::onPushButtonClicked()
{
	QPushButton* button = dynamic_cast<QPushButton*>(sender());
	if (nullptr == button)
	{
		return;
	}

	int x = button->frameGeometry().x();
	int y = button->frameGeometry().y();
	QModelIndex index = this->indexAt(QPoint(x, y));
	int row = index.row();

	if (button->text() == "推流")
	{
		RTSPConfig config;
		config.video = m_videos.at(row).url;
		config.url = this->item(row, 2)->text().toStdString(); // 流地址
		config.loop = 1000000;

		double duration = m_videos.at(row).duration;  // 视频时长
		QDateTime startTime = QDateTime::currentDateTime();
		QTableWidgetItem* item = this->item(row, 4);

		config.callback = [item, startTime, duration](double p)
			{
				QDateTime curTime = QDateTime::currentDateTime();
				int seconds = startTime.secsTo(curTime);
				int h = seconds / 3600;
				int m = seconds % 3600 / 60;
				int s = seconds % 60;

				QString txt;
				if (duration > 5000)
				{
					txt = "[" + QString("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0')) + "]  " + QString::number(p * 100, 'f', 2) + " %";
				}
				else if (duration > 500)
				{
					txt = "[" + QString("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0')) + "]  " + QString::number(p * 100, 'f', 1) + " %";
				}
				else
				{
					txt = "[" + QString("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0')) + "]  " + QString::number(int(p * 100 + 0.5)) + " %";
				}

				item->setText(txt);
				item->setForeground(QColor(0, 127, 0));
			};

		spdlog::info("Start to push {}", config.url);
		m_senders[row]->async_send_rtsp(config); // 推流

		button->setText("停止");
		QComboBox* comboBox = dynamic_cast<QComboBox*>(this->cellWidget(row, 3));
		comboBox->setEnabled(false);
		comboBox->setStyleSheet("QComboBox{border-radius: 0px;color:#aaaaaa;} QComboBox::drop-down {border: none;} QComboBox::hover {background-color: green;}");

		spdlog::info("Push {} success", config.url);
	}
	else
	{
		std::string url = this->item(row, 2)->text().toStdString();
		spdlog::info("Start to stop {}", url);

		m_senders[row]->stop(); // 停止推流

		button->setText("推流");
		this->item(row, 4)->setText("Stop");
		this->item(row, 4)->setForeground(QColor(0, 0, 0));

		QComboBox* comboBox = dynamic_cast<QComboBox*>(this->cellWidget(row, 3));
		comboBox->setEnabled(true);
		comboBox->setStyleSheet("QComboBox{border-radius: 0px;color:black;} QComboBox::drop-down {border: none;} QComboBox::hover {background-color: green;}");
		spdlog::info("Stop {} success", url);
	}
}

// 修改推流IP
void VideoTableWidget::onActivated(int index)
{
	QComboBox* comboBox = dynamic_cast<QComboBox*>(sender());
	if (nullptr == comboBox)
	{
		return;
	}

	int x = comboBox->frameGeometry().x();
	int y = comboBox->frameGeometry().y();
	int row = this->indexAt(QPoint(x, y)).row();

	QString ip = comboBox->itemText(index);
	QString url = this->item(row, 2)->text(); // Example rtsp://192.168.3.52:8554/stream/1

	size_t i = url.indexOf(QString(":8554"));
	url.replace(7, i - 7, ip);

	this->item(row, 2)->setText(url);
}

// 拖动文件到窗口，触发
void VideoTableWidget::dragEnterEvent(QDragEnterEvent* event)
{
	if (event->mimeData()->hasUrls())
	{
		event->acceptProposedAction(); // 事件数据中存在路径，方向事件
	}
	else
	{
		event->ignore();
	}
}

//拖动文件到窗口移动文件，触发
void VideoTableWidget::dragMoveEvent(QDragMoveEvent* event) {}

// 拖动文件到窗口释放文件，触发
void VideoTableWidget::dropEvent(QDropEvent* event)
{
	const QMimeData* mimeData = event->mimeData();
	if (mimeData->hasUrls())
	{
		for (const auto& url : mimeData->urls())
		{
			QString fileName = url.toLocalFile();
			addTableItem(fileName);
		}
	}
}

void VideoTableWidget::mouseMoveEvent(QMouseEvent* event)
{
	showToolTip(event);
}

void VideoTableWidget::mouseReleaseEvent(QMouseEvent* event)
{
	showToolTip(event);
}

void VideoTableWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
	QModelIndex index = this->indexAt(event->pos());
	int row = index.row();
	int col = index.column();

	if (row >= 0 && col == 1)
	{
		// 视频播放
		QString video = QString::fromLocal8Bit(m_videos[row].url);
		QDesktopServices::openUrl(QUrl::fromLocalFile(video));
	}
	else
	{
		QTableWidget::mouseDoubleClickEvent(event);
	}
}

void VideoTableWidget::showToolTip(QMouseEvent* event)
{
	QModelIndex index = this->indexAt(event->pos());
	int row = index.row();
	int col = index.column();
	if (row >= 0)
	{
		if (col == 1)
		{
			VideoInfo info = m_videos.at(row);
			QToolTip::showText(event->globalPosition().toPoint(), toString(info));
		}
		else if (col == 2)
		{
			QToolTip::showText(event->globalPosition().toPoint(), "双击拷贝");
		}
		else if (col == 3)
		{
			QToolTip::showText(event->globalPosition().toPoint(), "点击下拉");
		}
		else if (col == 4)
		{
			QToolTip::showText(event->globalPosition().toPoint(), "推流状态");
		}
		else if (col == 5 || col == 6)
		{
			QToolTip::showText(event->globalPosition().toPoint(), "按钮");
		}
	}
}

QString VideoTableWidget::getRtspUrl(const QString& video, const QString& ip)
{
	QFileInfo fileInfo(video);
	//return "rtsp://" + ip + ":8554/" + py.zhToPY(fileInfo.baseName());
	return "rtsp://" + ip + ":8554/stream/" + QString::number(count++);
}