#ifndef MAINWINDOW_H_INCLUDED
#define MAINWINDOW_H_INCLUDED
#include <QtGui>

#include "percol2d.h"
#include "plotter.h"
#include "myparam.h"

//---------------------------------------------------------------------
// Define main window 
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget* parent=0, Qt::WindowFlags f=0);
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
//    void setCapacity();
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
    void compute_pSigma();
    void compute_SumpSigma();
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
    void initGraphicsView();
    inline double singleSigma(double V, double r);
    inline double singleSigmaT0(double E, double V, double r, double EFc);

    QFont myfont;
    QGraphicsView *gv;
    Percol2D *model;
    QButtonGroup *typeResistor;
    QString curFile;

    MyParamD T, U, Tmin, Tmax, dT, Umin, Umax, dU, Ex, rand;
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
    QLabel *dispCapac; // Display of capacity value of last computation
    QLabel *dispConduct; // Display of conductivity value of last computation
    QLabel *dispFerr; // Display of ferr
    QLabel *dispBerr; // Display of berr
    QLabel *dispDeltaI; // Display of deltaI
    QAction *exitAction, *saveAction, *chooseFontAction;
    QMenu *file;
};

#endif /*MAINWINDOW_H_INCLUDED*/