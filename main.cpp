#include <qtextcodec.h>
#include "percol2d.h"
#include "mainwindow.h"
#include <qapplication.h>

int
main(int argc, char **argv)
{
    QApplication app( argc, argv );
//    QPalette pal(Qt::green.light(200), Qt::blue.light(200));
    QTextCodec::setCodecForTr(QTextCodec::codecForName("CP1251"));
 
    MainWindow mainWindow;

    mainWindow.resize(mainWindow.sizeHint());
//    mainWindow.setFont( QFontDialog::getFont( 0, mainWindow.font() ) );
    mainWindow.setCaption("Percolation model");
//    QApplication::setPalette(pal, true);
    mainWindow.show();

    QObject::connect( qApp, SIGNAL(lastWindowClosed()), qApp, SLOT(quit()) );
    
    return app.exec();
}
