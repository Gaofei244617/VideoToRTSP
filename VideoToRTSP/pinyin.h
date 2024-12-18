#pragma once

#include <QStringList>

class ZhToPY
{
public:
	explicit ZhToPY();

public:
	QString zhToPY(const QString& chinese); // 汉字转拼音
	QString zhToJP(const QString& chinese); // 汉字转字母简拼

private:
	QStringList listPY;
	QStringList listJP;
};
