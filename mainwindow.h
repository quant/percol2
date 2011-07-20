/* c++ */
#ifndef MAINWINDOW_H_INCLUDED
#define MAINWINDOW_H_INCLUDED
#include <QtGui>
#include <vector>

#include "percol2d.h"
#include "plotter.h"
#include "myparam.h"

//---------------------------------------------------------------------
// Define main window
extern double elementCr, sigma_m;
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
    void drawModeldV();
    void drawModelV();
    void drawModelJ();
    void clearScene();
    void computeModel();
    void computeOneR();
    void potential();
    void compute_nU();
//    void setConductivity();
//    void setCapacity();
    void clear();
    void init();
    void myMouseMoveEvent(QMouseEvent *);
    bool save();
    bool open();
    bool read();
    bool saveAs();
    void createStatusBar();
    void selectSigma(int);
    void computeRU();
    void computeReffU();
    void computeReffT();
    void computeAreaE();
    double AreaE(double E);
    void computeCapacityU();
    void computeCapacityT();
    void compute_deltaVx();
    void compute_Vx();
    void compute_pV();
    void compute_pdV();
    void compute_p_small_V();
    void compute_p_small_dV();
    void compute_pSigma();
    void compute_pCurrent();
    void compute_SumpSigma();
    void computeRU1();
    void computeRrc();
    void computeRT();
    void computeRT1();
    void stopCalc();
    void computeEF_TU();
    void computeEFT();
    void computeEFU();
    void computeVimax();
    void computeRX();
     void compute_deviation();

protected:
    void resizeEvent( QResizeEvent * );


private:
    void slotWindows();
    void slotWindowsActivated(int);
    void initPlotterT();
    void initPlotterE();
    void initPlotterU();
    void initPlotCurrent();
    void initPlotVoltage();
    void initPlotJouleHeat();
    void initPlotConductance();
    void initPlotterCU();
    void initPlotterCT();
    void mysort(std::vector<double> &v);
    void randomizeSigma_0();
    void randomizeSigma_1();
    void randomizeSigma_2();
    void initMenuBar();
    void initToolBar();
    void initStatusBar();
    void initControlDockWindow();
    void initGraphicsView();
    inline double singleSigma(double r, double rEx);
    inline double singleSigmaT0(double E, double V, double r, double EFc);
    inline double sedlo(double E, double Ey, double Ex, double V);
    inline double cohU(double E, double Ey, double A, double V, double Uc);
    inline double Vbarrier(double r);
    inline double computeSum(int NE, double dE, double Vd, double EFTU);
    inline double Vdot(void);
    inline double effective_medium(double y);
    inline double average(double y);

    void randRcr();
    //    double AreaE(double E);
//    inline double AreaE(double E);
    QFont myfont;
    QGraphicsView *gv;
    Percol2D *model;
    QButtonGroup *typeResistor;
    QString curFile;

    MyParamD T, U, Tmin, Tmax, dT, Umin, Umax, Vijmax, dU, Ex, deltaEx, Ey, rand, EF,EFT,fraction,portion;
    MyParamI cols, rows, seed, i_Rcr;
    MyParamD sigmaU, sigmaMin, r_c, capacity,deviation,a_barrier,Cg0, Delta_r,G_ser,EF0,kappa,y_cr, CUTOFF_SIGMA;
    std::vector<double> EFTarray;
    std::vector<double> EFUarray;
    std::vector<double> AreaEf;
    std::vector<double> CondDist;
    std::vector<double> NumJDist;
    std::vector<int> index_for_sorted_CondDist() const;
    std::vector<int> index_for_sorted_NumJDist() const;
    int numOfCurve,numOfCurveI,numOfCurveJ,numOfCurveSig,numOfCurvedV;
    double gTun, gOv,randc,density;//, Exc, Eyc, randc;//,Gold;
    bool flgStop;
    QDialog *winPlotI;
    QDialog *winPlotJ;
    QDialog *winPlotG;
    QDialog *winPlotV;
    QDialog *winPlotU;
    QDialog *winPlotCU;
    QDialog *winPlotT;
    QDialog *winPlotE;
    QDialog *winPlotCT;
    Plotter *plotterV;
    Plotter *plotterJ;
    Plotter *plotterG;
    Plotter *plotterI;
    Plotter *plotterU;
    Plotter *plotterT;
    Plotter *plotterE;
    Plotter *plotterCT;
    Plotter *plotterCU;
    std::vector<double> plotdata;
    QLabel *dispCond; // Display of condition number of last computation
    QLabel *dispCapac; // Display of capacity value of last computation
    QLabel *dispConduct; // Display of conductivity value of last computation
    QLabel *dispFerr; // Display of ferr
    QLabel *dispBerr; // Display of berr
    QLabel *dispDeltaI; // Display of deltaI
    QAction *exitAction, *saveAction, *openAction, *chooseFontAction;
    QMenu *file;
    QComboBox *typeCond;
};

#endif /*MAINWINDOW_H_INCLUDED*/
