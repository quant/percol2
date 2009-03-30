#ifndef MAINWINDOW_H_INCLUDED
#define MAINWINDOW_H_INCLUDED
//#include <qpushbutton.h> 
#include <q3mainwindow.h>
#include <q3canvas.h>
#include <qcombobox.h> 
#include <qstatusbar.h>
#include <qmessagebox.h>
#include <qsettings.h>
#include <qlabel.h>
//#include <qlineedit.h>
#include <qevent.h>
//#include <qtabwidget.h>
//#include <qgroupbox.h> 
#include <q3buttongroup.h> 
//#include <qradiobutton.h> 
//#include <qgrid.h>
//#include <qvbox.h>
//#include <qdockwindow.h> 
//#include <qworkspace.h> 
//#include <qpopupmenu.h>
#include <qdialog.h> 
//Added by qt3to4:
#include <QResizeEvent>
#include <QMouseEvent>

#include "percol2d.h"
#include "plotter.h"
#include "myparam.h"

class PercolView : public Q3CanvasView
{
    Q_OBJECT
public:
    PercolView(Q3Canvas& c, QWidget* parent=0, const char* name=0, Qt::WFlags f=0);
    void clear();
    void setModel(Percol2D *m) { model = m; }

private:
    QLabel *info;
    Percol2D *model;
};

//---------------------------------------------------------------------
// Define main window 
class MainWindow : public Q3MainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget* parent=0, const char* name=0, Qt::WFlags f=0);
    ~MainWindow(){ clear(); }
    void setModel();

public slots:
    void help();

private slots:
    void chooseFont();
    void drawModelR();
    void drawModelI();
    void drawModelA();
    void computeModel();
    void computeOneR();
//    void setConductivity();
    void setCapacity();
    void clear();
    void init();
    void myMouseMoveEvent(QMouseEvent *);
    bool save();
    bool saveAs();
    void createStatusBar();
    void selectSigma(int);
    void computeRU();
    void computeCapacityU();
    void computeCapacityT();
    void computeRU1();
    void computeRrc();
    void computeRT();
    void computeRT1();
    void stopCalc();

protected:
    void resizeEvent( QResizeEvent * );

    
private:
    void slotWindows();
    void slotWindowsActivated(int);
    void initPlotterT(); 
    void initPlotterU(); 
    void initPlotterCU(); 
    void initPlotterCT(); 

    void randomizeSigma_1();
    void randomizeSigma_2();
    void initMenuBar();
    void initStatusBar();
    void initControlDockWindow();
    void initCanvasView();
    inline double singleSigma(double V, double r);
    inline double singleSigmaT0(double E, double V, double r, double EFc);

    QFont myfont;
    Q3CanvasView *cv;
//    QGroupBox *oneResistorBox;
//    QPopupMenu *options;
    Percol2D *model;
//    QComboBox *comboSigmaType;
      Q3ButtonGroup *typeResistor;
//    QLineEdit *editRecordCond, *editSigmaMin;
    QString curFile;
//    QLabel *locationLabel;
//    QLabel *modLabel;
//    QLabel *result, *lbresult, *lbdouble;

    MyParamD T, U, Tmin, Tmax, dT, Umin, Umax, dU, Ex, sigma0;
    MyParamI cols, rows, seed; 
    MyParamD sigmaU, sigmaMin, r_c, capacity;
    int numOfCurve;
    bool flgStop; 
    QDialog *winPlotU;
    QDialog *winPlotCU;
    QDialog *winPlotT;
    QDialog *winPlotCT;
    Plotter *plotterU;
    Plotter *plotterT;
    Plotter *plotterCT;
    Plotter *plotterCU;
    std::vector<double> plotdata;
    QLabel *dispCond; // Display of condition number of last computation
    QLabel *dispConduct; // Display of condition number of last computation
    QLabel *dispFerr; // Display of ferr
    QLabel *dispBerr; // Display of berr
    QLabel *dispDeltaI; // Display of deltaI
};

#endif /*MAINWINDOW_H_INCLUDED*/