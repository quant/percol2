#include <QTextCodec>
#include "percol2d.h"
#include "mainwindow.h"
#include <QApplication>

int
main(int argc, char **argv)
{
    QApplication app( argc, argv );
//    QPalette pal(Qt::green.light(200), Qt::blue.light(200));
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("CP1251"));


    MainWindow mainWindow;

    mainWindow.resize(mainWindow.sizeHint());
//    mainWindow.setFont( QFontDialog::getFont( 0, mainWindow.font() ) );
    mainWindow.setWindowTitle("Percolation model");
//    QApplication::setPalette(pal, true);
    mainWindow.show();

    QObject::connect( qApp, SIGNAL(lastWindowClosed()), qApp, SLOT(quit()) );

    return app.exec();
}
