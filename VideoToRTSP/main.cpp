#include "VideoToRTSP.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    VideoToRTSP w;
    w.show();
    return a.exec();
}
