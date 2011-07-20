/* c++ */
#ifndef MAINWINDOW_H_INCLUDED
#define MAINWINDOW_H_INCLUDED

#define _CRT_SECURE_NO_WARNINGS 1
#include <vector>
#include <string>

#include "percol2d.h"

//---------------------------------------------------------------------
// Define main window 
extern double elementCr, sigma_m;

struct X_of_T
{
    double T;
    double G;
};

struct MainWindow
{
    MainWindow();
    ~MainWindow(){ clear(); }
    void setModel();

    void computeModel();
    void computeOneR();
    void clear();
    bool save();
    bool saveAs();
    void selectSigma(int);
    void computeAreaE();
    double AreaE(double E);
    void computeCapacityU();
    void computeCapacityT();
    void compute_pSigma();
    void compute_SumpSigma();
    void computeRU1();
    void computeRrc();
    void computeRT(std::vector<X_of_T> & data);
    void computeReffT(std::vector<X_of_T> & data);
    void computeRU(std::vector<X_of_T> & data);
    void computeReffU(std::vector<X_of_T> & data);
    void computeRT1();
    void computeEF_TU();
    void computeEFT();
    void computeEFU();
    void compute_deviation();

    void randomizeSigma_1();
    void randomizeSigma_2();

    inline double singleSigma(double r);
    inline double singleSigmaT0(double E, double V, double r, double EFc);
    inline double sedlo(double E, double Ey, double Ex, double V);
    inline double cohU(double E, double Ey, double A, double V, double Uc);
    inline double Vbarrier(double r);
    inline double computeSum(int NE, double dE, double Vd, double EFTU);
    inline double Vdot(void);
    inline double effective_medium(double y);
    inline double average(double y);

    void randRcr();
    Percol2D *model;
    int typeResistor;

    double T, U, Tmin, Tmax, dT, Umin, Umax, dU, Ex, Ey, rand, EF,EFT,fraction;
    int cols, rows, seed; 
    double sigmaU, sigmaMin, r_c, capacity,deviation;
    std::vector<double> EFTarray;
    std::vector<double> EFUarray;
    std::vector<double> AreaEf;
    double gTun, gOv,randc;
    double dispCond; // Display of condition number of last computation
    double dispCapac; // Display of capacity value of last computation
    double dispConduct; // Display of conductivity value of last computation
    double dispFerr; // Display of ferr
    double dispBerr; // Display of berr
    double dispDeltaI; // Display of deltaI
    int typeCond;
};

#endif /*MAINWINDOW_H_INCLUDED*/
