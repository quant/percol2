#define _CRT_SECURE_NO_WARNINGS 1
#include "mainwindow.h"
#include "mkl.h"
#include <math.h>
#include "myparam.h"
#include <QCursor>
#include <QVBoxLayout>
#include <QMessageBox>

const double Cond_min = 1e-12;
const int NCUT = 1e4;
const int nJ=5000;

//const double Delta_r=35;//15; //nm
const double E0=560.;//meV
const double Vg0=240;//350;//240//350;//400;//50;
//const double Vg0=50;//240//350;//400;//50;
//const double Cg0=-0.15;//-0.12;//-0.1;//-0.06;//-0.05;//-0.04;
// const double EF0=20;
double elementCr = 1e+22;
double sigma_m=0.1;
//double a_barrier=200;//150;//100;

class ColorScale
{
protected:
    double v0,v1;
    QColor c0,c1;
    friend class ColorScale2;
public:
    ColorScale() : v0(0), v1(1), c0(Qt::red), c1(Qt::blue) {}
    void setRange(double vmin,double vmax)
    {
        v0 = vmin; v1 = vmax;
    }
    void setColors(QColor cmin,QColor cmax)
    {
        c0 = cmin; c1 = cmax;
    }
    QColor color(double v) const
    {
        if (v <= v0) return c0;
        if (v >= v1) return c1;
        int r = c0.red() + (v - v0)/(v1 - v0) * (c1.red() - c0.red());
        int g = c0.green() + (v - v0)/(v1 - v0) * (c1.green() - c0.green());
        int b = c0.blue() + (v - v0)/(v1 - v0) * (c1.blue() - c0.blue());
        return QColor(r,g,b);
    }
};

class ColorScale2
{
    ColorScale a,b;
public:
    ColorScale2() {}
    void setRange(double vmin,double vmax)
    {
        double vmid = 0.5 *( vmax + vmin );
        a.setRange(vmin, vmid);
        b.setRange(vmid, vmax);
    }
    void setColors(QColor cmin,QColor cmax)
    {
        int r0,g0,b0,r1,g1,b1;
        cmin.getRgb(&r0,&g0,&b0);
        cmax.getRgb(&r1,&g1,&b1);
        a.setColors(cmin,QColor(0.5*(r0+r1),0.5*(g0+g1),0.5*(b0+b1)));
        b.setColors(QColor(0.5*(r0+r1),0.5*(g0+g1),0.5*(b0+b1)),cmax);
    }
    QColor color(double v) const
    {
        if (v < a.v0) return a.c0;
        if (v < b.v0) return a.color(v);
        if (v < b.v1) return b.color(v);
        return b.c1;
    }
    void setExtraColor(double v, QColor c)
    {
        a.v1 = b.v0 = v;
        a.c1 = b.c0 = c;
    }
};

ColorScale csV; // Voltage scale and node coloring
ColorScale2 csJ; // Joule heat scale and node coloring
ColorScale2 csI; // Current scale and edge coloring
ColorScale2 csS; // Sigma scale and edge coloring

class DoubleValidator : public QRegExpValidator
{
public:
    DoubleValidator(QWidget *parent) : QRegExpValidator(parent)
    {
        const QRegExp x(QString("[+-]?\\d+\\.?\\d*([dDe][+-]?\\d+)?"));
        setRegExp(x);
    }
    ~DoubleValidator(){}
} theDoubleValidator(0);

class PosIntValidator : public QRegExpValidator
{
public:
    PosIntValidator(QWidget *parent) : QRegExpValidator(parent)
    {
        const QRegExp x(QString("\\d+"));
        setRegExp(x);
    }
    ~PosIntValidator(){}
} thePosIntValidator(0);



void MainWindow::initMenuBar()
{
    exitAction = new QAction("Exit", this);
    saveAction = new QAction("Save As", this);
    openAction = new QAction("Open file", this);
    chooseFontAction = new QAction("Choose Font", this);
    saveAction->setStatusTip(tr("Сохранить в файле напряжения, токи и параметры расчета"));
    openAction->setStatusTip(tr("Загрузить параметры из файла"));
    saveAction->setShortcut(tr("Ctrl+S"));
    openAction->setShortcut(tr("Ctrl+O"));
    exitAction->setShortcut(tr("Ctrl+Q"));
    chooseFontAction->setShortcut(tr("Ctrl+F"));
    connect(exitAction, SIGNAL(triggered()), this, SLOT(close()));
    connect(chooseFontAction, SIGNAL(triggered()), this, SLOT(chooseFont()));
    connect(saveAction, SIGNAL(triggered()), this, SLOT(saveAs()));
    connect(openAction, SIGNAL(triggered()), this, SLOT(open()));


    QMenu* file = menuBar()->addMenu("Menu");
    file->addAction(saveAction);
    file->addAction(openAction);
    file->addAction(chooseFontAction);
    file->addSeparator();
    file->addAction(exitAction);

//    menu->insertSeparator();


/*    Q3PopupMenu* help = new Q3PopupMenu( menu );
    help->insertItem("&About", this, SLOT(help()), Qt::Key_F1);
    menu->insertItem(tr("Помощь"),help);
    */
}
void MainWindow::initToolBar()
{
    QPixmap pix(15,15);
    pix.fill(Qt::red);

    QAction *criticalElement = new QAction("Critical Element", this);
    QToolBar *toolBar = new QToolBar(this);
 //   QHBoxLayout *H = new QHBoxLayout;
//    this->Ex.setDisplay(("E_x[meV]"), H);
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    QToolButton *button1=new QToolButton();
    button1->setIcon(pix);
    toolBar->addAction(criticalElement);
    addToolBar(toolBar);
}

void MainWindow::chooseFont()
{
    bool ok;
    QFont oldfont = myfont;
    myfont = QFontDialog::getFont(&ok, oldfont, this);

    if (ok)
        setFont(myfont);
    else
        myfont = oldfont;


}

void MainWindow::initStatusBar()
{
    createStatusBar();
    this->dispCond = new QLabel("RCond: --------------",
            this->statusBar());
    this->statusBar()->addWidget(this->dispCond);
    this->dispDeltaI = new QLabel("dI: -----------",
            this->statusBar());
    this->statusBar()->addWidget(this->dispDeltaI);
    this->dispFerr = new QLabel("Ferr: -----------",
            this->statusBar());
    this->statusBar()->addWidget(this->dispFerr);
    this->dispBerr = new QLabel("Berr: -----------",
            this->statusBar());
    this->statusBar()->addWidget(this->dispBerr);
    this->dispConduct = new QLabel("G: ------------",
            this->statusBar());
    this->statusBar()->addWidget(this->dispConduct);
    this->dispCapac = new QLabel("C: ------------",
            this->statusBar());
    this->statusBar()->addWidget(this->dispCapac);
}


void MainWindow::initControlDockWindow()
{

     QDockWidget *myDockWidget = new QDockWidget(tr("Control Panel"),this);
     myDockWidget->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
     addDockWidget(Qt::RightDockWidgetArea, myDockWidget);
     QWidget *control = new QWidget();
     QVBoxLayout *vl0 = new QVBoxLayout(control);
          {
        QGroupBox *gb1 = new QGroupBox(tr("One Resistor"),control);
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(gb1->sizePolicy().hasHeightForWidth());
        gb1->setSizePolicy(sizePolicy);

        QGroupBox *gb2 = new QGroupBox(tr("Resistor Network"), control);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(gb2->sizePolicy().hasHeightForWidth());
        gb2->setSizePolicy(sizePolicy);


        /* now fill gb1/OneResistor */
         QVBoxLayout *vl2 = new QVBoxLayout(gb1);
         vl0->addWidget(gb1);
        {
            QHBoxLayout *hlLR = new QHBoxLayout(gb1);
           QGroupBox *gb = new QGroupBox(tr("Compute Conductance G"));
            vl2->addLayout(hlLR);
            vl2->addWidget(gb);

            /* now fill LR */
            QVBoxLayout *L = new QVBoxLayout(gb1);
            hlLR->addLayout(L);
            {
                this->T.setDisplay(("T[meV]"), this, L);
//                this->Ex.setDisplay(("a[nm]"), L);
                this->deltaEx.setDisplay(("deltaE_x[meV]"), this, L);
                this->Ex.setDisplay(("E_x[meV]"), this, L);
                this->Ey.setDisplay(("E_y[meV]"), this, L);
                this->Tmin.setDisplay(("T_min"), this, L);
                this->Tmax.setDisplay(("T_max"), this, L);
                this->dT.setDisplay(("dT"), this, L);
                this->a_barrier.setDisplay(("a_bar"), this, L);
                this->Delta_r.setDisplay(("Delta_r"), this, L);
//                this->EFT.setDisplay(("EF(T,U)"), L);

                QPushButton *bL  = new QPushButton(tr("E_F for given T,U"));
                connect(bL,SIGNAL(clicked()), this, SLOT(computeEF_TU()));
                L->addWidget(bL);
                QPushButton *computeDensityButton = new QPushButton(tr("density(U)"));
                connect(computeDensityButton,SIGNAL(clicked()), this, SLOT(compute_nU()));
                L->addWidget(computeDensityButton);
                L->addStretch(1);
           }
            QVBoxLayout *R = new QVBoxLayout(gb1);
//            QVBoxLayout *R = new QVBoxLayout(this);
            hlLR->addLayout(R);
            {
                this->U.setDisplay(("U[meV]"), this, R);
                this->rand.setDisplay(("r[0--1]"), this, R);
                this->EF0.setDisplay(("EF0"), this, R);
                this->EFT.setDisplay(("EFT"), this, R);
                this->Umin.setDisplay(("U_min"), this, R);
                this->Umax.setDisplay(("U_max"), this, R);
                this->dU.setDisplay(("dU"), this, R);
                this->Cg0.setDisplay(("Cg"), this, R);
                this->G_ser.setDisplay(("G_ser"), this, R);
                this->y_cr.setDisplay(("y_cr"), this, R);
//                QPushButton *computeDensityButton = new QPushButton(tr("density(U)"));
//                connect(computeDensityButton,SIGNAL(clicked()), this, SLOT(compute_nU()));
//                R->addWidget(computeDensityButton);

                typeCond = new QComboBox;
                typeCond->addItem(tr("Total conductance"));
                typeCond->addItem(tr("Tunnel conductance"));
                typeCond->addItem(tr("Over-barrier G"));
                typeCond->addItem(tr("Tunnel+Over-barrier G"));
                typeCond->addItem(tr("G(E_F0)"));
                typeCond->setCurrentIndex(3);
                R->addWidget(typeCond);
                R->addStretch(1);
           }
            /* now fill gb/Compute Conductance G */
            QHBoxLayout *hl = new QHBoxLayout(gb);
            {
                QPushButton *b1  = new QPushButton(tr("G(T)"));
                connect(b1,SIGNAL(clicked()), this, SLOT(computeRT1()));

                QPushButton *b2 = new QPushButton(tr("G(U)"));
                connect(b2,SIGNAL(clicked()), this, SLOT(computeRU1()));

                QPushButton *b3  = new QPushButton(tr("U(x) for given T,U"));
                connect(b3,SIGNAL(clicked()), this, SLOT(potential()));

                QPushButton *b4  = new QPushButton(tr("E_F(U)"));
////                QPushButton *b4  = new QPushButton(tr("Area(E)"));
//                connect(b4,SIGNAL(clicked()), this, SLOT(computeEFU()));
                connect(b4,SIGNAL(clicked()), this, SLOT(computeAreaE()));

/*                typeCond = new QComboBox;
                typeCond->addItem(tr("Total conductance"));
                typeCond->addItem(tr("Tunnel conductance"));
                typeCond->addItem(tr("Over-barrier G"));
                typeCond->addItem(tr("Tunnel+Over-barrier G"));
                typeCond->addItem(tr("G(E_F0)"));
                typeCond->setCurrentIndex(0);
*/
                hl->addWidget(b1);
                hl->addWidget(b2);
                hl->addWidget(b3);
                hl->addWidget(b4);
            }
        }

        /* now fill gb2/ResistorNetwork */
        QVBoxLayout *vl3 = new QVBoxLayout(gb2);
        vl0->addWidget(gb2);
        {
            QHBoxLayout *hlLR = new QHBoxLayout;
            QHBoxLayout *hl3 = new QHBoxLayout;
            QGroupBox *gb3 = new QGroupBox(tr("Canvas Images"), gb2);
            QGroupBox *gb4 = new QGroupBox(tr("G-curves"), gb2);
            QGroupBox *gb5 = new QGroupBox(tr("C-curves"), gb2);
            vl3->addLayout(hlLR);
            hl3->addWidget(gb3);
            hl3->addWidget(gb4);
            hl3->addWidget(gb5);
            vl3->addLayout(hl3);

            /* now fill LR */
            QVBoxLayout *L = new QVBoxLayout(this);
            hlLR->addLayout(L);
            {
                QGroupBox *gbRType = new QGroupBox(tr("Resistor Network Type"));
                L->addWidget(gbRType);

                QRadioButton *type1 = new QRadioButton(tr("RNW Exp(-kappa*x)"));
//                QRadioButton *type1 = new QRadioButton(tr("Одинаковые сопротивления"));
                QRadioButton *type2 = new QRadioButton(tr("RNW 0 or 1"));
                QRadioButton *type3 = new QRadioButton(tr("model RNW"));

                QVBoxLayout *vbox = new QVBoxLayout(gbRType);
                vbox->addWidget(type1);
                vbox->addWidget(type2);
                vbox->addWidget(type3);
                //    vbox->addStretch(1);

                this->typeResistor = new QButtonGroup;
                this->typeResistor->setExclusive(true);
                this->typeResistor->addButton(type1,0);
                this->typeResistor->addButton(type2,1);
                this->typeResistor->addButton(type3,2);
                this->typeResistor->button(2)->setChecked(true);
                //connect(typeResistor,SIGNAL(clicked(int)),this,SLOT(selectSigma(int)));

    this->CUTOFF_SIGMA.setDisplay(("Sigma_cut_off"), this, L);
    this->kappa.setDisplay(("kappa"), this, L);
//    this->Vijmax.setDisplay(("deltaVijmax"), L);
//    this->portion.setDisplay(("portion"), L);

            }
            QVBoxLayout *R = new QVBoxLayout(this);
            hlLR->addLayout(R);
            {
    this->rows.setDisplay(("rows"), this, R);
    this->cols.setDisplay(("cols"), this, R);
    this->seed.setDisplay(("seed"), this, R);
    this->deviation.setDisplay(("deviation"), this, R);
    this->sigmaU.setDisplay(("sigmaU"), this, R);
//    this->fraction.setDisplay(("fraction x"), R);
    this->r_c.setDisplay(("r_c"), this, R);
    this->sigmaMin.setDisplay(("sigmaMin"), this, R);
    this->i_Rcr.setDisplay(("i_cr"), this, R);
//    this->Vijmax.setDisplay(("deltaVijmax"), R);

//     QGroupBox *critElem = new QGroupBox(tr("Red Bond"));
//     QVBoxLayout *Hcr = new QVBoxLayout(R);
//     this->r_c.setDisplay(("r_c"), Hcr);
//     this->sigmaMin.setDisplay(("sigmaMin"), Hcr);
//     Hcr->addWidget(critElem);
            }


            /* now fill gb0/Canvas Image */
//           QVBoxLayout *vl = new QVBoxLayout(hl3);
           QVBoxLayout *vl = new QVBoxLayout(gb3);
            {
                QPushButton *computeButton = new QPushButton(tr("Compute"));
                computeButton->setDefault(true);
                connect(computeButton, SIGNAL(clicked()), this, SLOT(computeModel()));

                QPushButton *drawResButton = new QPushButton(tr("Conductivity network"));
                connect(drawResButton,SIGNAL(clicked()), this, SLOT(drawModelR()));

                QPushButton *drawCurButton = new QPushButton(tr("Draw Current I"));
                connect(drawCurButton,SIGNAL(clicked()), this, SLOT(drawModelI()));

                QPushButton *drawVButton = new QPushButton(tr("Draw Voltage V"));
                connect(drawVButton,SIGNAL(clicked()), this, SLOT(drawModelV()));


                QPushButton *drawJButton = new QPushButton(tr("Draw Heat I*V"));
                connect(drawJButton,SIGNAL(clicked()), this, SLOT(drawModelJ()));

                QPushButton *drawdVButton = new QPushButton(tr("Draw dV"));
                connect(drawdVButton,SIGNAL(clicked()), this, SLOT(drawModeldV()));

                QPushButton *clearButton = new QPushButton(tr("Clear"));
                connect(clearButton,SIGNAL(clicked()), this, SLOT(clearScene()));



                vl->addWidget(computeButton);
                vl->addWidget(drawResButton);
                vl->addWidget(drawCurButton);
                vl->addWidget(drawdVButton);
                vl->addWidget(drawVButton);
                vl->addWidget(drawJButton);
                vl->addWidget(clearButton);
                vl->addStretch(1);

            }
           QVBoxLayout *vl1 = new QVBoxLayout(gb4);
            {
                QPushButton *stopB = new QPushButton(tr("STOP"));
                connect(stopB, SIGNAL(clicked()), this, SLOT(stopCalc()));

                QPushButton *xB = new QPushButton(tr("Conductivity(x)"));
                connect(xB,SIGNAL(clicked()), this, SLOT(computeRX()));

                QPushButton *uB = new QPushButton(tr("Conductivity(U)"));
                connect(uB,SIGNAL(clicked()), this, SLOT(computeRU()));

                QPushButton *ueffB = new QPushButton(tr("Effective Gm(U)"));
                connect(ueffB,SIGNAL(clicked()), this, SLOT(computeReffU()));

                QPushButton *VmaxB = new QPushButton(tr("Vmax(U)"));
                connect(VmaxB,SIGNAL(clicked()), this, SLOT(computeVimax()));

                QPushButton *rcB = new QPushButton(tr("Conductivity(rc)"));
                connect(rcB,SIGNAL(clicked()), this, SLOT(computeRrc()));

                QPushButton *dB = new QPushButton(tr("Deviation"));
                connect(dB,SIGNAL(clicked()), this, SLOT(compute_deviation()));

                vl1->addWidget(stopB);
                vl1->addWidget(xB);
                vl1->addWidget(uB);
                vl1->addWidget(ueffB);
                vl1->addWidget(VmaxB);
                vl1->addWidget(rcB);
                vl1->addWidget(dB);
                vl1->addStretch(1);

            }
//           QVBoxLayout *vl2 = new QVBoxLayout(hl3);
          QVBoxLayout *vl2 = new QVBoxLayout(gb5);
            {
                QPushButton *cuB = new QPushButton(tr("Capacity(U)"));
                connect(cuB,SIGNAL(clicked()), this, SLOT(computeCapacityU()));

                QPushButton *ctB = new QPushButton(tr("Capacity(T)"));
                connect(ctB,SIGNAL(clicked()), this, SLOT(computeCapacityT()));

                QPushButton *gtB = new QPushButton(tr("Conductivity(T)"));
                connect(gtB,SIGNAL(clicked()), this, SLOT(computeRT()));

                QPushButton *gteffB = new QPushButton(tr("Effective Gm(T)"));
                connect(gteffB,SIGNAL(clicked()), this, SLOT(computeReffT()));

//                QPushButton *pVb = new QPushButton(tr("pVoltage"));
//                connect(pVb,SIGNAL(clicked()), this, SLOT(compute_pV()));
                QPushButton *pVb = new QPushButton(tr("Voltage(x) at y=y_cr"));
                connect(pVb,SIGNAL(clicked()), this, SLOT(compute_Vx()));
//                connect(pVb,SIGNAL(clicked()), this, SLOT(compute_deltaVx()));

//                QPushButton *pSVb = new QPushButton(tr("pSmallVoltage"));
//                connect(pSVb,SIGNAL(clicked()), this, SLOT(compute_p_small_V()));

                QPushButton *pdVb = new QPushButton(tr("pdeltaV"));
                connect(pdVb,SIGNAL(clicked()), this, SLOT( compute_pdV()));

                QPushButton *pSdVb = new QPushButton(tr("pSmalldeltaV"));
                connect(pSdVb,SIGNAL(clicked()), this, SLOT(compute_p_small_dV()));

//                QPushButton *pC = new QPushButton(tr("pCurrent"));
//                connect(pC,SIGNAL(clicked()), this, SLOT(compute_pCurrent()));

                QPushButton *cpSB = new QPushButton(tr("pSigma"));
                connect(cpSB,SIGNAL(clicked()), this, SLOT(compute_pSigma()));

//               QPushButton *cSpSB = new QPushButton(tr("Sum_pSigma"));
//                connect(cSpSB,SIGNAL(clicked()), this, SLOT(compute_SumpSigma()));

                vl2->addWidget(cuB);
                vl2->addWidget(ctB);
                vl2->addWidget(gtB);
                vl2->addWidget(gteffB);
                vl2->addWidget(pVb);
//                vl2->addWidget(pSVb);
                vl2->addWidget(pdVb);
//                vl2->addWidget(pSdVb);
//                vl2->addWidget(pC);
                vl2->addWidget(cpSB);
 //               vl2->addWidget(cSpSB);
                vl2->addStretch(1);

            }

 }

    }
    vl0->addStretch(1);

    myDockWidget->setWidget(control);


}


void MainWindow::initGraphicsView()
{
    QGraphicsScene *scene = new QGraphicsScene(0,0,600,600);
    this->gv = new QGraphicsView(scene,this);
    setCentralWidget(this->gv);
    this->gv->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
}
void MainWindow::initPlotCurrent()
{
    winPlotI = new QDialog(this);
    winPlotI->setWindowTitle(tr("Distribution of current:"));
    QVBoxLayout *layout=new QVBoxLayout(winPlotI);
    this->plotterI= new Plotter(winPlotI);
    this->plotterI->setCurveData(1, this->plotdata);
    layout->addWidget(this->plotterI);
}
void MainWindow::initPlotVoltage()
{
    winPlotV = new QDialog(this);
    winPlotV->setWindowTitle(tr("Distribution of voltage:"));
    QVBoxLayout *layout=new QVBoxLayout(winPlotV);
    this->plotterV= new Plotter(winPlotV);
    this->plotterV->setCurveData(1, this->plotdata);
    layout->addWidget(this->plotterV);
}
void MainWindow::initPlotJouleHeat()
{
    winPlotJ = new QDialog(this);
    winPlotJ->setWindowTitle(tr("Distribution of Joule Heat:"));
    QVBoxLayout *layout=new QVBoxLayout(winPlotJ);
    this->plotterJ= new Plotter(winPlotJ);
    this->plotterJ->setCurveData(1, this->plotdata);
    layout->addWidget(this->plotterJ);
}
void MainWindow::initPlotConductance()
{
    winPlotG = new QDialog(this);
    winPlotG->setWindowTitle(tr("Distribution of conductance:"));
    QVBoxLayout *layout=new QVBoxLayout(winPlotG);
    this->plotterG= new Plotter(winPlotG);
    this->plotterG->setCurveData(1, this->plotdata);
    layout->addWidget(this->plotterG);
}
void MainWindow::initPlotterU()
{
    winPlotU = new QDialog(this);
    winPlotU->setWindowTitle(tr("Dependences G(U):"));
    QVBoxLayout *layout=new QVBoxLayout(winPlotU);
    this->plotterU= new Plotter(winPlotU);
//    this->plotterU = new Plotter(winPlotU);
    this->plotterU->setCurveData(1, this->plotdata);
    layout->addWidget(this->plotterU);
}


void MainWindow::initPlotterT()
{
    winPlotT = new QDialog(this);
    winPlotT->setFocus();
    winPlotT->setWindowTitle(tr("Dependences G(T):"));
    QVBoxLayout *layout=new QVBoxLayout(winPlotT);
    this->plotterT= new Plotter(winPlotT);
    this->plotterT->setCurveData(1, this->plotdata);
    layout->addWidget(this->plotterT);
}
void MainWindow::initPlotterE()
{
    winPlotE = new QDialog(this);
    winPlotE->setFocus();
    winPlotE->setWindowTitle(tr("Area(E):"));
    QVBoxLayout *layout=new QVBoxLayout(winPlotE);
    this->plotterE= new Plotter(winPlotE);
    this->plotterE->setCurveData(1, this->plotdata);
    layout->addWidget(this->plotterE);
}
void MainWindow::initPlotterCT()
{
    winPlotCT = new QDialog(this);
    winPlotCT->setWindowTitle(tr("Capacity(T):"));
    QVBoxLayout *layout=new QVBoxLayout(winPlotCT);
    this->plotterCT= new Plotter(winPlotCT);
    this->plotterCT->setCurveData(1, this->plotdata);
    layout->addWidget(this->plotterCT);
}
void MainWindow::initPlotterCU()

{
    winPlotCU = new QDialog(this);
    winPlotCU->setWindowTitle(tr("Capacity(U):"));
    QVBoxLayout *layout=new QVBoxLayout(winPlotCU);
    this->plotterCU= new Plotter(winPlotCU);
    this->plotterCU->setCurveData(1, this->plotdata);
    layout->addWidget(this->plotterCU);
}

void MainWindow::setModel()
{
    if (model) delete model;
    model = new PercolRect(this->rows,this->cols);
}

MainWindow::MainWindow(QWidget *parent, Qt::WindowFlags f)
: randc(0.5),
//Exc(5.), Eyc(5.),
density(1000.),sigmaU(1.e2), flgStop(false),portion(0.01),
//  T(0.13), Tmin(0.1),Tmax(5.331), dT(0.1), //Gold(0.0),
  T(0.13), Tmin(0.),Tmax(0.3), dT(0.01), //Gold(0.0),
U(465.), Umin(460.), Umax(600.), dU(10.),
//U(155.), Umin(155.), Umax(240.), dU(5.),
i_Rcr(0),CUTOFF_SIGMA(1.e-15),
//U(950), Umin(200.), Umax(1500), dU(5.),
r_c(0.0), Ex(22.), deltaEx(0.), Vijmax(2.), Ey(6.), rand(0.5), EF(20.),EFT(22.),kappa(10.),y_cr(15),
a_barrier(180.), Cg0(-0.09),Delta_r(22.),G_ser(1.0),EF0(42.),
//a_barrier(150.), Cg0(-0.12),Delta_r(30.),G_ser(3.0),EF0(20.),
QMainWindow(parent,f),
rows(30), cols(53), numOfCurve(1), seed(1), model(0)

{
    this->initMenuBar();
    this->initToolBar();
    this->initStatusBar();
    this->initPlotCurrent();
    this->initPlotVoltage();
    this->initPlotJouleHeat();
    this->initPlotConductance();
    this->initPlotterT();
    this->initPlotterE();
    this->initPlotterU();
    this->initPlotterCT();
    this->initPlotterCU();
    this->initControlDockWindow();
    this->setModel();
    this->initGraphicsView();
    init();
}


bool MainWindow::open()
{

     QString fn = QFileDialog::getOpenFileName( this,
                         tr("Choose a file name"), ".",
                         tr("*.dat*"));
    if ( !fn.isEmpty() ) {
         // read from file
        curFile = fn;
        return this->read();
    }
    return false;
}

bool MainWindow::read()
{
    if (this->curFile.isEmpty())
    {
        return this->open();
    }
    QFile f(this->curFile);
    if (!f.open(QIODevice::ReadOnly)) {
                 QMessageBox::information(this, tr("Unable to open file"),
                 f.errorString());
             return false;
         }
    return true;
}
bool MainWindow::saveAs()
{
     QString fn = QFileDialog::getSaveFileName( this,
                         tr("Choose a file name"), ".",
                         tr("*.dat*"));
    if ( !fn.isEmpty() ) {
        curFile = fn;
        return this->save();
    }
    return false;
}

bool MainWindow::save()
{
    if (this->curFile.isEmpty())
    {
        return this->saveAs();
    }
    QString nV=this->curFile+"V";
    QFile fV(nV);
    if(fV.exists()){
        int n=QMessageBox::warning(0,
            tr("Warning"),
            "File with this name has already existed,"
            "\n Do you want to rewrite it?",
            "Yes",
            "No",
            QString::null,
            0,
            1
            );
        if(n){
           return this->saveAs(); //выбираем новое имя
        }
    }
    //Saving the file!
    fV.open(QIODevice::WriteOnly|QIODevice::Truncate);

    QTextStream o(&fV);
    o << "Percolation model:\n";
    o << "Rows Cols Seed Conduct Temp Vg sigmaU Ex\n";
        QString s1;
        s1.sprintf("%i %i %i %lg %lg %lg %lg% lg\n",int(this->rows),int(this->cols),int(this->seed),
            double(model->conductivity), double(this->T), double(this->U),
            double(this->sigmaU),double(this->Ex));
        o << s1;
    o << "x_cr   y_cr  sigma_cr\n";
//    this->i_Rcr = model->index_of_Rcr();
    double sigma_cr=model->Sigma[this->i_Rcr ];
    QPair<int,int> endsRcr = model->ends(i_Rcr);
    QPair<double,double> xy0Rcr = model->xy(endsRcr.first);
    QPair<double,double> xy1Rcr = model->xy(endsRcr.second);
    double x=0.5*(xy0Rcr.first+xy1Rcr.first);
    double y=0.5*(xy0Rcr.second+xy1Rcr.second);
    QString s2;
    s2.sprintf("%lg %lg %lg\n",x,y,sigma_cr);
    o << s2;

        o << "x y Voltage:\n";
        QString s;
        QPair<double,double> xy = model->xy(0);
        s.sprintf("%lg %lg %lg\n",xy.first,xy.second,model->V[0]);
        o << s;
//    o << "Calculated v-nodes\n";
    for (int w = 0; w < model->nW(); ++w)
    {
        QString s;
        QPair<double,double> xy = model->xy(w + model->nV());
        s.sprintf("%lg %lg %lg\n",xy.first,xy.second,model->W[w]);
//        s.sprintf("%i %lg %lg %lg\n",w + model->nV(),xy.first,xy.second,model->W[w]);
        o << s;
    }
        QString ss;
        QPair<double,double> xy1 = model->xy(1);
        ss.sprintf("%lg %lg %lg\n",xy1.first,xy1.second,model->V[1]);
        o << ss;
        fV.close();
//    statusBar()->showMessage(tr("Saved '%1'").arg(fV), 2009);

        QString nC=this->curFile+"C";
    QFile fC(nC);
//    QFile f(this->curFile);

    if(fC.exists()){
        int n=QMessageBox::warning(0,
            tr("Warning"),
            "File with this name has already existed,"
            "\n Do you want to rewrite it?",
            "Yes",
            "No",
            QString::null,
            0,
            1
            );
        if(n){
           return this->saveAs(); //выбираем новое имя
        }
    }
    //Saving the file!
    fC.open(QIODevice::WriteOnly|QIODevice::Truncate);

    QTextStream oC(&fC);
    oC << "Current at centers of edges\n";
    for (int e = 0; e < model->nI(); ++e)
    {
        QPair<int,int> ends = model->ends(e);
        QPair<double,double> xy0 = model->xy(ends.first);
        QPair<double,double> xy1 = model->xy(ends.second);
        x=0.5*(xy0.first+xy1.first);
        y=0.5*(xy0.second+xy1.second);
        QString s;
 //       QPair<int,int> from_to = model->ends(e);
        s.sprintf("%lg %lg %lg\n",x,y, fabs(model->I[e]));
        oC << s;
    }
    fC.close();

    QString nJ=this->curFile+"J";
    QFile fJ(nJ);
    if(fJ.exists()){
        int n=QMessageBox::warning(0,
            tr("Warning"),
            "File with this name has already existed,"
            "\n Do you want to rewrite it?",
            "Yes",
            "No",
            QString::null,
            0,
            1
            );
        if(n){
           return this->saveAs(); //выбираем новое имя
        }
    }
    //Saving the file!
    fJ.open(QIODevice::WriteOnly|QIODevice::Truncate);

    QTextStream oJ(&fJ);
    oJ << "Joule Heat at centers of edges\n";
    for (int e = 0; e < model->nI(); ++e)
    {
        QPair<int,int> ends = model->ends(e);
        QPair<double,double> xy0 = model->xy(ends.first);
        QPair<double,double> xy1 = model->xy(ends.second);
        x=0.5*(xy0.first+xy1.first);
        y=0.5*(xy0.second+xy1.second);
        QString s;
        s.sprintf("%lg %lg %lg\n",x,y, model->IdifV[e]);
        oJ << s;
    }
    fJ.close();

    QString ndV=this->curFile+"dV";
    QFile fdV(ndV);
    if(fdV.exists()){
        int n=QMessageBox::warning(0,
            tr("Warning"),
            "File with this name has already existed,"
            "\n Do you want to rewrite it?",
            "Yes",
            "No",
            QString::null,
            0,
            1
            );
        if(n){
           return this->saveAs(); //выбираем новое имя
        }
    }
    //Saving the file!
    fdV.open(QIODevice::WriteOnly|QIODevice::Truncate);

    QTextStream odV(&fdV);
    odV << "Voltage difference at centers of edges\n";
    for (int e = 0; e < model->nI(); ++e)
    {
        QPair<int,int> ends = model->ends(e);
        QPair<double,double> xy0 = model->xy(ends.first);
        QPair<double,double> xy1 = model->xy(ends.second);
        x=0.5*(xy0.first+xy1.first);
        y=0.5*(xy0.second+xy1.second);
        QString s;
        s.sprintf("%lg %lg %lg\n",x,y, fabs(model->difV[e]));
        odV << s;
    }
    fdV.close();

    //    statusBar()->showMessage(tr("Saved '%1'").arg(fC), 2009);
    return TRUE;
}





void MainWindow::createStatusBar()
{

}



void MainWindow::help()
{
    static QMessageBox *about;
    if (!about)
    {
        char buf[200];
        MKL_Get_Version_String(buf,sizeof(buf)-1);
        buf[sizeof(buf)-1] = 0;
        about = new QMessageBox("Percolation",buf,
           QMessageBox::Information, 1, 0, 0, this);
    }
    about->setButtonText(1, "Dismiss" );
    about->show();
}

void MainWindow::init()
{
    int r_type = this->typeResistor->checkedId();
    this->selectSigma(r_type);
}

void MainWindow::clear()
{
    setMouseTracking( FALSE );
    this->gv->scene()->clear();
}


class NodeItemFactory
{
    QGraphicsScene *canvas;
    ColorScale *cs;
    double scale;
    QPair<double,double> offset;
    int blob_size;
public:
    NodeItemFactory( QGraphicsScene *_canvas, ColorScale *_cs,
        double _scale, QPair<double,double> _offset, int _blob_size = 2 )
        : canvas(_canvas), cs(_cs), scale(_scale), offset(_offset), blob_size(_blob_size) {}
//    Q3CanvasEllipse *newVnode(double v, QPair<double,double> xy)
    QGraphicsEllipseItem *newVnode(double v, QPair<double,double> xy)
    {
        int v_blob_size = 2;
        double canvasx = (xy.first - offset.first) * scale - 0.5 * v_blob_size;
        double canvasy = (xy.second - offset.second) * scale -  0.5 * v_blob_size;
        QGraphicsEllipseItem *n = canvas->addEllipse(canvasx,canvasy,
                 v_blob_size,v_blob_size,QPen(Qt::black,0),QBrush(cs->color(v)));
        return n;
    }
    QGraphicsEllipseItem *newWnode(double v, QPair<double,double> xy)
//new:    QGraphicsEllipseItem *newWnode(double v, QPair<double,double> xy)???(10,10)->(-5, -5, 10, 10);
    {
        double canvasx = (xy.first - offset.first) * scale - 0.5*blob_size;
        double canvasy = (xy.second - offset.second) * scale - 0.5*blob_size;
        QColor c = cs->color(v);
        QGraphicsEllipseItem *n = canvas->addEllipse(canvasx,canvasy,blob_size,blob_size,QPen(c),QBrush(c));
//        n->setBrush(QBrush(cs->color(v)));
//        n->setZ( 128 );
//        n->move( canvasx, canvasy );
        return n;
    }
    void setBlobSize(int s) { blob_size = s; }
};

class EdgeItemFactory
{
    QGraphicsScene *canvas;
    ColorScale2 *cs;
    double scale;
    QPair<double,double> offset;
public:
    EdgeItemFactory( QGraphicsScene *_canvas, ColorScale2 *_cs,
        double _scale, QPair<double,double> _offset )
        : canvas(_canvas), cs(_cs), scale(_scale), offset(_offset) {}
    QGraphicsLineItem *newEdge(double c, QPair<double,double> xy0, QPair<double,double> xy1)
    {   int canvasx0 = (xy0.first - offset.first) * scale;//- 0.5*blob_size;
        int canvasy0 = (xy0.second - offset.second) * scale;//- 0.5*blob_size;
        int canvasx1 = (xy1.first - offset.first) * scale;//- 0.5*blob_size;
        int canvasy1 = (xy1.second - offset.second) * scale;//- 0.5*blob_size;
        QPen pen;
        if (c == 0)
            pen = QPen(Qt::white);
        else if (c == 1.e-15)
            pen = QPen(Qt::white);
//            pen = QPen(Qt::blue);
        else
            pen = QPen(cs->color(c),2);
        if(c==1000) pen=QPen(Qt::black);
        if(c==elementCr) pen=QPen(Qt::red);
        QGraphicsLineItem *n = canvas->addLine(canvasx0,canvasy0,canvasx1,canvasy1,pen);
        return n;
    }
};


void MainWindow::clearScene()
{
    QGraphicsScene *scene = gv->scene();
    gv->fitInView(scene->sceneRect(),Qt::KeepAspectRatioByExpanding);
    scene->clear();
}

void MainWindow::drawModelV()
{
    QGraphicsScene *scene = gv->scene();
    gv->fitInView(scene->sceneRect(),Qt::KeepAspectRatioByExpanding);
    scene->clear();

    // Set view port
    const double factor = 0.95;
    double xscale = factor * scene->width()  / (model->xmax() - model->xmin());
    double yscale = factor * scene->height() / (model->ymax() - model->ymin());
    double scale = xscale < yscale ? xscale : yscale;

    QPair<double,double> offset;
    offset.first = -0.5 * (model->xmax() - model->xmin()) * (1.0 - factor) / factor;
    offset.second = -0.5 * (model->ymax() - model->ymin()) * (1.0 - factor) / factor;
//    double Jmaxold,Jmaxoldold;
    double vmin = 1e300, vmax = -1e300;
    for (int v = 0; v < model->nV(); ++v)
    {
        if (model->V[v] > vmax) vmax = model->V[v];
        if (model->V[v] < vmin) vmin = model->V[v];
    }
    for (int w = 0; w < model->nW(); ++w)
    {
        double woltage = model->W[w];
        if (woltage < vmin || woltage > vmax)
        {
            QString s;
            s.sprintf("Got voltage %lg at w=%i\n",woltage,w);
            QMessageBox::warning(this, tr("W-voltage out of bound"),s,
                QMessageBox::Ok,QMessageBox::Ok);
            break;
        }
        if (woltage > vmax) vmax = woltage;
        if (woltage < vmin) vmin = woltage;
    }
    csV.setRange(vmin,vmax);
    csV.setColors(Qt::red,Qt::blue);

    NodeItemFactory vfactory(scene,&csV,scale,offset);
    vfactory.setBlobSize(2);//!!!

    for (int v = 0; v < model->nV(); ++v)
    {
        double V = model->V[v];
        QPair<double,double> xy = model->xy(v);
        QGraphicsEllipseItem *n = vfactory.newVnode(V, xy);
        n->setZValue(128);
//        n->show();
    }

    int v = model->nV();
    for (int w = 0; w < model->nW(); ++w)
    {
        double W = model->W[w];
        QPair<double,double> xy = model->xy(v+w);
        QGraphicsEllipseItem  *n = vfactory.newWnode(W, xy);
        n->setZValue(128);
//        n->show();
    }

    scene->update();
    this->gv->update();
//    setMouseTracking( TRUE );
}

void MainWindow::drawModelI()
{

    QGraphicsScene *scene = gv->scene();
    gv->fitInView(scene->sceneRect(),Qt::KeepAspectRatioByExpanding);
//    scene->clear();

    // Set view port
    const double factor = 0.95;
    double xscale = factor * scene->width()  / (model->xmax() - model->xmin());
    double yscale = factor * scene->height() / (model->ymax() - model->ymin());
    double scale = xscale < yscale ? xscale : yscale;

    QPair<double,double> offset;
    offset.first = -0.5 * (model->xmax() - model->xmin()) * (1.0 - factor) / factor;
    offset.second = -0.5 * (model->ymax() - model->ymin()) * (1.0 - factor) / factor;

    int emax;
    double imin = 1e300, imax = -1e300;
    double q;
    for (int i = 0; i < model->nI(); ++i)
    {
        q = (model->I[i]);
        if (fabs(q) > imax&&model->Sigma[i]!=this->sigmaU)
        {
            imax = fabs(q);
            emax=i;
        }
        if (fabs(q) < imin)
            imin = fabs(q);
    }

    csI.setRange(0, imax);
//    csI.setRange(imin, 0.1*imax);
//    csI.setColors(Qt::white,Qt::green);
    csI.setColors(Qt::white,Qt::black);
//    csI.setColors(Qt::black,Qt::white);
//    csI.setExtraColor(imin+0.05*(imax-imin),QColor("#007f00"));
    EdgeItemFactory ifactory(scene,&csI,scale,offset);

    //setMouseTracking( FALSE );
    randRcr();
 //   this->i_Rcr = model->index_of_Rcr();
    elementCr = fabs((model->I[this->i_Rcr ]));
    for (int e = 0; e < model->nI(); ++e)
    {
        QPair<int,int> ends = model->ends(e);
        QPair<double,double> xy0 = model->xy(ends.first);
        QPair<double,double> xy1 = model->xy(ends.second);
        q=fabs((model->I[e]));
        if(q>0.01*imax){
        //QGraphicsLineItem *n = ifactory.newEdge( q , xy0, xy1 );
        }
    }
    //setMouseTracking( TRUE );


#if 0
    ColorScale csDI;
    csDI.setRange(0,deltai_max);
    csDI.setColors(Qt::white,Qt::blue);
    NodeItemFactory difactory(pCanvas,&csDI,scale,offset);
    difactory.setBlobSize(10);
    for (int w = 0; w < model->nW(); ++w)
    {
        QMemArray<int> from = model->from(v+w);
        QMemArray<int> to = model->to(v+w);
        double total_i = 0;

        for (int i = 0; i < from.size(); ++i)
        {
            total_i += model->I[ from[i] ];
        }
        for (int i = 0; i < to.size(); ++i)
        {
            total_i -= model->I[ to[i] ];
        }

        QPair<double,double> xy = model->xy(v+w);
        QCanvasEllipse *n = difactory.newWnode(fabs(total_i), xy);
        n->show();
    }
#endif
    scene->update();
    this->gv->update();
}
void MainWindow::drawModeldV()
{

    QGraphicsScene *scene = gv->scene();
    gv->fitInView(scene->sceneRect(),Qt::KeepAspectRatioByExpanding);
    scene->clear();
    QVector<int> idx;

    // Set view port
    const double factor = 0.95;
    double xscale = factor * scene->width()  / (model->xmax() - model->xmin());
    double yscale = factor * scene->height() / (model->ymax() - model->ymin());
    double scale = xscale < yscale ? xscale : yscale;

    QPair<double,double> offset;
    offset.first = -0.5 * (model->xmax() - model->xmin()) * (1.0 - factor) / factor;
    offset.second = -0.5 * (model->ymax() - model->ymin()) * (1.0 - factor) / factor;
//-------
    double Vmin = 1e300, Vmax = -1e300;
    double q,V_1,V_2,V_i;
    int nv=model->nV();
    V_2=model->W[0];
    int nI=model->nI();
    QVector<double> Vij(nI);
    for (int i = 0; i < model->nI(); ++i)
    {
    QPair<int,int> ends_i = model->ends(i);
    if(ends_i.first < nv) V_1=model->V[ends_i.first];
    else V_1=model->W[ends_i.first-nv];
    if(ends_i.second < nv) V_2=model->V[ends_i.second];
    else V_2=model->W[ends_i.second-nv];
        V_i=fabs((V_1-V_2));
//        V_i=log10(fabs(V_i));
        Vij[i]=V_i;
//	double IV_i = fabs(I_i * V_i);
        double q = V_i;
        if (q > Vmax) Vmax = q;
        if (q < Vmin) Vmin = q;
    }
//--------
    csI.setRange(0, Vmax);
//    csI.setRange(imin, imax);
    csI.setColors(Qt::white,Qt::green);
//    csI.setColors(Qt::white,Qt::black);
//    csI.setExtraColor(Vmin+0.5*(Vmax-Vmin),QColor("#007f00"));
    EdgeItemFactory ifactory(scene,&csI,scale,offset);
    randRcr();
    idx = model->index_for_sorted_difV();
    for (int e = 0; e < model->nI(); ++e)
    {
        QPair<int,int> ends = model->ends(idx[e]);
        QPair<double,double> xy0 = model->xy(ends.first);
        QPair<double,double> xy1 = model->xy(ends.second);
        q=fabs(model->difV[idx[e]]);
        //        q=Vij[e];
//        if(q>0.1*Vmax){
//        if(q>0.01) QGraphicsLineItem *n = ifactory.newEdge( q , xy0, xy1 );
//        }
    }


    scene->update();
    this->gv->update();
}

void MainWindow::drawModelJ()
{

    QGraphicsScene *scene = gv->scene();
    gv->fitInView(scene->sceneRect(),Qt::KeepAspectRatioByExpanding);
//    scene->clear();

    // Set view port
    const double factor = 0.95;
    double xscale = factor * scene->width()  / (model->xmax() - model->xmin());
    double yscale = factor * scene->height() / (model->ymax() - model->ymin());
    double scale = xscale < yscale ? xscale : yscale;

    QPair<double,double> offset;
    offset.first = -0.5 * (model->xmax() - model->xmin()) * (1.0 - factor) / factor;
    offset.second = -0.5 * (model->ymax() - model->ymin()) * (1.0 - factor) / factor;
//-------
    double Vmin = 1e300, Vmax = -1e300;
    double q,V_1,V_2,V_i;
    int nv=model->nV();
    V_2=model->W[0];
    int nI=model->nI();
    QVector<double> Jij(nI);
    for (int i = 0; i < model->nI(); ++i)
    {
    QPair<int,int> ends_i = model->ends(i);
    if(ends_i.first < nv) V_1=model->V[ends_i.first];
    else V_1=model->W[ends_i.first-nv];
    if(ends_i.second < nv) V_2=model->V[ends_i.second];
    else V_2=model->W[ends_i.second-nv];
        V_i=fabs(model->I[i]*(V_1-V_2));
//        V_i=log10(fabs(V_i));
        Jij[i]=V_i;
//	double IV_i = fabs(I_i * V_i);
        double q = V_i;
        if (q > Vmax) Vmax = q;
        if (q < Vmin) Vmin = q;
    }
//--------
    csI.setRange(0, Vmax);
//    csI.setRange(imin, imax);
    csI.setColors(Qt::white,Qt::green);
//    csI.setColors(Qt::white,Qt::black);
//    csI.setExtraColor(Vmin+0.5*(Vmax-Vmin),QColor("#007f00"));
    EdgeItemFactory ifactory(scene,&csI,scale,offset);
    randRcr();
    for (int e = 0; e < model->nI(); ++e)
    {
        QPair<int,int> ends = model->ends(e);
        QPair<double,double> xy0 = model->xy(ends.first);
        QPair<double,double> xy1 = model->xy(ends.second);
        q=Jij[e];
        if(q>0.5*Vmax)
        {
//            QGraphicsLineItem *n = ifactory.newEdge( q , xy0, xy1 );
        }
    }

    scene->update();
    this->gv->update();
}

void MainWindow::drawModelR()
{
    int r_type = this->typeResistor->checkedId();
    this->selectSigma(r_type);
    QGraphicsScene *scene = gv->scene();
    gv->fitInView(scene->sceneRect(),Qt::KeepAspectRatioByExpanding);
    scene->clear();

    // Set view port
    const double factor = 0.95;
    double xscale = factor * scene->width()  / (model->xmax() - model->xmin());
    double yscale = factor * scene->height() / (model->ymax() - model->ymin());
    double scale = xscale < yscale ? xscale : yscale;

    QPair<double,double> offset;
    offset.first = -0.5 * (model->xmax() - model->xmin()) * (1.0 - factor) / factor;
    offset.second = -0.5 * (model->ymax() - model->ymin()) * (1.0 - factor) / factor;
    double Smin = 1e300, Smax = -1e300;
    double q;
    for (int i = 0; i < model->nI(); ++i)
    {
        q = (model->Sigma[i]);
        if (fabs(q) > Smax&&q!=this->sigmaU) Smax = fabs(q);
        if (fabs(q) < Smin&&(q>10*this->CUTOFF_SIGMA&&q!=this->sigmaU))
        {
            Smin = fabs(q);
        }
    }
    csS.setRange(Smin, Smax);
    csS.setColors(Qt::white,Qt::black);
//    csS.setExtraColor(0.01*Smax,QColor("#007f00"));
    EdgeItemFactory ifactory(scene,&csS,scale,offset);
    for (int e = 0; e < model->nI(); ++e)
    {
        QPair<int,int> ends = model->ends(e);
        QPair<double,double> xy0 = model->xy(ends.first);
        QPair<double,double> xy1 = model->xy(ends.second);
        q=fabs((model->Sigma[e]));
//        QGraphicsLineItem *n = ifactory.newEdge( q , xy0, xy1 );
//        n->show();
    }
    scene->update();
    this->gv->update();

}


void MainWindow::computeModel()
{
    int r_type = this->typeResistor->checkedId();
    this->selectSigma(r_type);
    model->compute();
    this->i_Rcr=model->index_of_Rcr();
    QString s;
    s.sprintf("RCond: %.3lg",model->rcond);
    double z = s.toDouble();
    this->dispCond->setText(s);
    this->dispCond->update();
//    this->statusBar()->update();
    s.sprintf("dI: %.3lg",model->deltaI);
    z = s.toDouble();
    this->dispDeltaI->setText(s);
    this->dispDeltaI->update();
    s.sprintf("Ferr: %.3lg",model->ferr);
    z = s.toDouble();
    this->dispFerr->setText(s);
    this->dispFerr->update();
    s.sprintf("Berr: %.3lg",model->berr);
    z = s.toDouble();
    this->dispBerr->setText(s);
    this->dispBerr->update();
    s.sprintf("G: %.3lg",model->conductivity);
    z = s.toDouble();
    this->dispConduct->setText(s);
    this->dispConduct->update();
    s.sprintf("C: %.3lg",model->capacity);
    z = s.toDouble();
    this->dispCapac->setText(s);
    this->dispCapac->update();
    this->statusBar()->update();
//    this->setConductivity();
    this->r_c.updateDisplay();
    this->U.updateDisplay();
    this->T.updateDisplay();
    this->dT.updateDisplay();
    this->EFT.updateDisplay();
    this->EF0.updateDisplay();
    this->Ex.updateDisplay();
    this->portion.updateDisplay();
    this->fraction.updateDisplay();
    qApp->processEvents();
}
void MainWindow::computeOneR()
{
//        model->conductivity=singleSigma(this->rand);
//        model->conductivity=singleSigma(this->rand);
        model->conductivity=singleSigma(this->rand,this->Ex);
    QString s;
    s.sprintf("G: %.3lg",model->conductivity);
//    double z = s.toDouble();
    this->dispConduct->setText(s);
    this->dispConduct->update();
//        model->conductivity=singleSigma(this->rand, this->Ex);
        this->U.updateDisplay();
        this->T.updateDisplay();
        this->Ex.updateDisplay();
        this->Ey.updateDisplay();
        this->EFT.updateDisplay();
        QApplication::processEvents();
}
void MainWindow::computeCapacityU()
{
    winPlotCU->show();
    winPlotCU->raise();
    winPlotCU->activateWindow();
    QVector<double> data;
//    double EF00=this->EF0;
//    int G_type = this->typeCond->currentIndex();
 //   if(G_type!=4) computeEFU();
//    int j=0;
    int Jm;
    int iseed=this->seed;
//    double yy;
    int l=5;
//    for (double x = this->Umin; x <= this->Umax; x += this->dU)
    while (l< 110)
    {
        this->cols=l+3;
        this->rows=l;
        double x=l;
/*       this->U =x;
        if(G_type==4) this->EFT=EF00;
        else
        {
            if(j<this->EFUarray.size())
            {
                double EFTU=this->EFUarray[j];
                this->EFT=EFTU;
                j++;
            }
            else
            {
            QString s;
            s.sprintf("element of array of j=%i\n",j);
            QMessageBox::warning(this, tr("is out of bound"),s,
                QMessageBox::Ok,QMessageBox::Ok);
            break;
        }
        }
        if(this->flgStop) {this->flgStop=false; return;}
*/
        double yy=0;
        int jj=0;
        yy=0;
        Jm=1000;
        if(l>40) Jm=500;
//       if(l>30) Jm=600;
        if(l>60) Jm=300;
        if(l>100) Jm=10;
        while(jj<Jm){
        this->seed=iseed;
        iseed++;
        this->computeModel();
        double y=model->capacity;//(x*x);
        yy=yy+y;
        jj++;
        }
        if(l>50) l=l+10;
        else l=l+5;
//        l=l+10;
        data.push_back(x);
        data.push_back(yy/Jm);
        this->plotterCU->setCurveData(this->numOfCurve,data);
        if(this->flgStop) {this->numOfCurve++; this->flgStop=false; return;}
    }
    this->numOfCurve++;

}
void MainWindow::computeCapacityT()
{
    winPlotCT->show();
    winPlotCT->raise();
    winPlotCT->activateWindow();
    QVector<double> data;
    int G_type = this->typeCond->currentIndex();//checkedId();
    if(G_type!=4) computeEFT();
    int j=0;
    double EF00=this->EF0;
    for (double x = this->Tmax; x >= this->Tmin; x -= this->dT)
//    for (double x = this->Tmin; x <= this->Tmax; x += this->dT)
    {
        this->T =x;
        if(G_type==4) this->EFT=EF00;
        else
        {
            if(j<this->EFTarray.size())
            {
                double EFTU=this->EFTarray[j];
                this->EFT=EFTU;
                j++;
            }
            else
            {
            QString s;
            s.sprintf("EFTarray at j=%i\n",j);
            QMessageBox::warning(this, tr("is out of bound"),s,
                QMessageBox::Ok,QMessageBox::Ok);
            break;
        }
        }
        if(this->flgStop) {this->flgStop=false; return;}
        this->computeModel();
 //       this->setCapacity();
        data.push_back(x);
        data.push_back(model->capacity);
            if(model->conductivity<NCUT*this->CUTOFF_SIGMA)
            {
                this->numOfCurve++;
                break;
            }
        this->plotterCT->setCurveData(this->numOfCurve,data);
    }
    this->numOfCurve++;
    }

void MainWindow::computeRrc()
{
    winPlotT->show();
    winPlotT->raise();
    winPlotT->activateWindow();
    int r_type = this->typeResistor->checkedId();
    double xmin, xmax, dx;
    xmin=0.35;//this->r0;
    xmax=0.4999;//this->r0+5./this->T;
    if(xmax>1.) xmax=1.;
    dx=(xmax-xmin)/101;
    QVector<double> data;
    QVector<double> data1;
//    double EF00=this->EF0;
//    this->EFT=EF00;
    double y_old=0;
        for (double x = xmin; x <= xmax; x += dx)
    {
        data1.push_back(fabs(x-0.5));
        double y=pow(fabs(x-0.5),4./3.);
        data1.push_back(y);
        this->plotterT->setCurveData(this->numOfCurve,data1);
    }
    this->numOfCurve++;
//return;
    for (double x = xmin; x <= xmax; x += dx)
    {
        this->r_c = x;
        if(this->flgStop) {this->flgStop=false; return;}
        this->selectSigma(r_type);
//        this->computeModel();
//        double y=model->conductivity;
        double y=effective_medium(y_old);
        y_old=y;
        data.push_back(fabs(x-0.5));
//        data.push_back(fabs(x-0.5));
        data.push_back(y);
//        data.push_back(model->conductivity);
        this->plotterT->setCurveData(this->numOfCurve,data);
    }
    this->numOfCurve++;
}
void MainWindow::computeRU1()
{
    QVector<double> data;
    winPlotU->show();
    winPlotU->raise();
    winPlotU->activateWindow();
    int G_type = this->typeCond->currentIndex();
    double EF00=this->EF0;  //!!!!!!!!!!

        if(G_type!=4)
    {
//        computeEFU();
    }

    int j=0;

    for (double x = this->Umin; x <= this->Umax; x += this->dU)
    {
        this->U = x;
/*        if(G_type==4) this->EFT=EF00;
        else
        {
            if(j<this->EFUarray.size())
            {
                double EFTU=this->EFUarray[j];
                this->EFT=EFTU;
                j++;
            }
            else
            {
            QString s;
            s.sprintf("element of array of j=%i\n",j);
            QMessageBox::warning(this, tr("is out of bound"),s,
                QMessageBox::Ok,QMessageBox::Ok);
            break;
        }
        }
*/
        computeOneR();
        data.push_back(x);
        double y=model->conductivity;
//        y=6.28*y;
        data.push_back(y);
        this->plotterU->setCurveData(this->numOfCurve,data);
    }

    this->numOfCurve++;
}
/*
void MainWindow::computeRU()
{
    winPlotU->show();
    winPlotU->raise();
    winPlotU->activateWindow();
    int ni=model->nI();
    double Totsum=0;
        for (int i = 0; i < ni; ++i)
    {
        if (fabs(model->Sigma[i])!=this->sigmaU) Totsum=Totsum+1;
    }
    int G_type = this->typeCond->currentIndex();
        if(G_type!=4)
    {
        computeEFU();
        double EFTU=this->EFUarray[0];
        if(EFTU==0) return;
    }
    int j=0;
    QVector<double> data;
    for (double x = this->Umin; x <= this->Umax; x += this->dU)
    {   this->U =x;

//        if(G_type!=0) this->EFT=EF0;
        if(G_type==4) this->EFT=EF0;
        else
        {
        double EFTU=this->EFUarray[j];
        this->EFT=EFTU;
        j++;
        if(EFTU==0) break;
        }
        if(this->flgStop) {this->flgStop=false; return;}
        this->computeModel();
        double y=model->conductivity;
        double sum=0.;
        double sigma_m=singleSigma(this->rand)*this->portion;
    for (int i = 0; i < ni; ++i)
    {
        if (fabs(model->Sigma[i]) <=sigma_m) sum=sum+1;
    }
    sum=sum/Totsum;
    if(sum==0) break;
    this->fraction=sum;
        data.push_back(sum);
//        data.push_back(-sum+this->rand);
        y=6.28*y;
        data.push_back(y);

//            data.push_back(x);
//            if(model->conductivity<NCUT*CUTOFF_SIGMA)
//            {
//                this->numOfCurve++;
//                break;
//            }

            this->plotterU->setCurveData(this->numOfCurve,data);
    }
    this->numOfCurve++;
}
*/

void MainWindow::computeRX()
{
    QVector<double> data;
    QVector<double> data1;
    winPlotU->show();
    winPlotU->raise();
    winPlotU->activateWindow();
    int ni=model->nI();
    double Totsum=0;
        for (int i = 0; i < ni; ++i)
    {
        if (fabs(model->Sigma[i])!=this->sigmaU) Totsum=Totsum+1;
    }
    int G_type = this->typeCond->currentIndex();
        if(G_type!=4)
    {
        computeEFU();
//        double EFTU=this->EFUarray[0];
//        if(EFTU==0) return;
    }
    int j=0;
    double EF00=this->EF0;
//    randRcr();
//    double sigma00=singleSigma(0.5);
    for (double x = 0.1; x < 1; x += 0.05)
    {
        data1.push_back(x);
        double y=pow(fabs(x-0.5),4./3.);
        data1.push_back(y);
        this->plotterU->setCurveData(this->numOfCurve,data1);
    }
    this->numOfCurve++;
//    for (double x = 0.4; x < 0.5; x += 0.005)
    for (double x = this->Umin; x <= this->Umax; x += this->dU)
    {
//        this->fraction =x;
        this->U =x;
        if(G_type==4) this->EFT=EF00;
        else
        {
            if(j<this->EFUarray.size())
            {
                double EFTU=this->EFUarray[j];
                this->EFT=EFTU;
                j++;
            }
            else
            {
            QString s;
            s.sprintf("element of array of j=%i\n",j);
            QMessageBox::warning(this, tr("is out of bound"),s,
                QMessageBox::Ok,QMessageBox::Ok);
            break;
        }
        }
        if(this->flgStop) {this->flgStop=false; return;}
        double sum=0.;
//! закомментировала строку ниже, поскольку в singleSigma уже два аргумента
        //        sigma_m=this->portion*singleSigma(this->randc);//0.01*sigma00*jj;
        this->computeModel();
    for (int i = 0; i < ni; ++i)
    {
        if (fabs(model->Sigma[i]) <sigma_m) sum=sum+1;
    }
    sum=sum/Totsum;
    this->fraction=sum;
        data.push_back(sum);
//        data.push_back(x);
//        double y1=pow(fabs(sum-this->randc),4./3.);
        double y=model->conductivity;
        data.push_back(y);
        this->plotterU->setCurveData(this->numOfCurve,data);
        if(y<2e-11) {
                this->numOfCurve++;
                break;
        }

 }
    this->numOfCurve++;
}


void MainWindow::computeRU()
{
    QVector<double> data;
//    QVector<double> data1;
    winPlotU->show();
    winPlotU->raise();
    winPlotU->activateWindow();
    int G_type = this->typeCond->currentIndex();
        if(G_type!=4)
    {
        computeEFU();
//        double EFTU=this->EFUarray[0];
//        if(EFTU==0) return;
    }
    int j=0;
    double EF00=this->EF0;
    for (double x = this->Umin; x <= this->Umax; x += this->dU)
    {
    this->U =x;
    if(this->flgStop) {this->flgStop=false; return;}
        if(G_type==4) this->EFT=EF00;
        else
        {
            if(j<this->EFUarray.size())
            {
                double EFTU=this->EFUarray[j];
                this->EFT=EFTU;
                j++;
            }
            else
            {
            QString s;
            s.sprintf("element of array of j=%i\n",j);
            QMessageBox::warning(this, tr("is out of bound"),s,
                QMessageBox::Ok,QMessageBox::Ok);
            break;
        }
        }
    this->computeModel();
    double y=model->conductivity;
        y=(this->cols-3)*y/(this->rows);
        y=6.28*y;
//        double y=0.5*1000*(this->EFT-Vdot())/3.14159/E0;
        data.push_back(x);
        data.push_back(y);
        this->plotterU->setCurveData(this->numOfCurve,data);

               if(y<2e-10) {
                this->numOfCurve++;
                break;
            }
    }
    this->numOfCurve++;
}
void MainWindow::compute_nU()
{
    QVector<double> data;
    winPlotU->show();
    winPlotU->raise();
    winPlotU->activateWindow();
    int G_type = this->typeCond->currentIndex();
        if(G_type!=4)
    {
        computeEFU();
//        double EFTU=this->EFUarray[0];
//        if(EFTU==0) return;
    }
    int j=0;
    double EF00=this->EF0;
    for (double x = this->Umin; x <= this->Umax; x += this->dU)
    {
    this->U =x;
    if(this->flgStop) {this->flgStop=false; return;}
        if(G_type==4) this->EFT=EF00;
        else
        {
            if(j<this->EFUarray.size())
            {
                double EFTU=this->EFUarray[j];
                this->EFT=EFTU;
                j++;
            }
            else
            {
            QString s;
            s.sprintf("element of array of j=%i\n",j);
            QMessageBox::warning(this, tr("is out of bound"),s,
                QMessageBox::Ok,QMessageBox::Ok);
            break;
        }
        }
        double y=0.5*1000*(this->EFT-Vdot())/3.14159/E0;
        data.push_back(x);
        data.push_back(y);
        this->plotterU->setCurveData(this->numOfCurve,data);

    }
    this->numOfCurve++;
}
void MainWindow::computeVimax()
{
    QVector<double> data;
    winPlotU->show();
    winPlotU->raise();
    winPlotU->activateWindow();
    //int r_type = this->typeResistor->checkedId();
    int G_type = this->typeCond->currentIndex();
    double EF00=this->EF0;
    int j=0;
    for (double x = this->Umin; x <= this->Umax; x += this->dU)
    {
    this->U =x;
          this->U.updateDisplay();
    if(this->flgStop) {this->flgStop=false; return;}
        if(G_type==4) this->EFT=EF00;
        else
        {
            if(j<this->EFUarray.size())
            {
                double EFTU=this->EFUarray[j];
                this->EFT=EFTU;
                j++;
            }
            else
            {
            QString s;
            s.sprintf("element of array of j=%i\n",j);
            QMessageBox::warning(this, tr("is out of bound"),s,
                QMessageBox::Ok,QMessageBox::Ok);
            break;
        }
        }
    this->computeModel();
//    this->i_Rcr = model->index_of_Rcr();
    double V_1,V_2,V_i;
    V_2=model->W[0];
    int nv=model->nV();
    int imax;
    QVector<double> Vij(model->nI());
    double Vmin = 1e300, Vmax = -1e300;
    for (int i = 0; i < model->nI(); ++i)
    {
        if(fabs(model->Sigma[i])!=this->sigmaU)
        {
    QPair<int,int> ends_i = model->ends(i);
    if(ends_i.first < nv) V_1=model->V[ends_i.first];
    else V_1=model->W[ends_i.first-nv];
    if(ends_i.second < nv) V_2=model->V[ends_i.second];
    else V_2=model->W[ends_i.second-nv];
//        V_i=fabs(model->I[i]);
//        V_i=fabs(model->I[i])*fabs((V_1-V_2));
        V_i=fabs((V_1-V_2));
//        if(V_i<1e-20) V_i=1e-20;
        Vij[i]=V_i;
        double q = V_i;
        if (q > Vmax)
        {
        Vmax = q;
        imax=i;
        }
        if (q < Vmin) Vmin = q;
        }
    }
        data.push_back(x);
//        data.push_back(Vmax);
        data.push_back(fabs(model->I[this->i_Rcr])*fabs(Vij[this->i_Rcr]));
//        data.push_back(fabs(model->Sigma[this->i_Rcr]));
        this->Vijmax=Vmax;
        this->Vijmax.updateDisplay();
        this->i_Rcr.updateDisplay();
        this->plotterU->setCurveData(this->numOfCurve,data);
    }
    this->numOfCurve++;
}
void MainWindow::computeReffU()
{
    QVector<double> data;
    QVector<double> data1;
    winPlotU->show();
    winPlotU->raise();
    winPlotU->activateWindow();
    int r_type = this->typeResistor->checkedId();
        int G_type = this->typeCond->currentIndex();
        if(G_type!=4)
    {
        computeEFU();
//        double EFTU=this->EFUarray[0];
//        if(EFTU==0) return;
    }
    int j=0;
    double y_old=0;
    double EF00=this->EF0;
    for (double x = this->Umin; x <= this->Umax; x += this->dU)
    {
    this->U =x;
          this->U.updateDisplay();
    if(this->flgStop) {this->flgStop=false; return;}
        if(G_type==4) this->EFT=EF00;
        else
        {
            if(j<this->EFUarray.size())
            {
                double EFTU=this->EFUarray[j];
                this->EFT=EFTU;
                j++;
            }
            else
            {
            QString s;
            s.sprintf("element of array of j=%i\n",j);
            QMessageBox::warning(this, tr("is out of bound"),s,
                QMessageBox::Ok,QMessageBox::Ok);
            break;
        }
        }
        this->selectSigma(r_type);
        double y=effective_medium(y_old);
        y_old=y;
        double y1=6.28*y;
        data.push_back(x);
        data.push_back(y1);
        this->plotterU->setCurveData(this->numOfCurve,data);

        /*       if(y<2e-10) {
                this->numOfCurve++;
                break;
            }
        */
    }
    this->numOfCurve++;
}
double MainWindow::average(double y)
{
    double sum=0;
        for (int i = 0; i < model->nI(); ++i)
      {
        double sigma_ik=fabs(model->Sigma[i]);
        if (sigma_ik!=this->sigmaU)
        {
            sum = sum+(sigma_ik-y)/(sigma_ik+fabs(y));
        }
        }
return sum;
}
double MainWindow::effective_medium(double y_old)
{
//    this->computeModel();
//    double yd=model->conductivity;
    double y0=y_old;
    double sum10=0;
    double sum11=1;
    double Totsum=0;
    double y1=0;
        for (int i = 0; i < model->nI(); ++i)
      {
        double sigma_ik=fabs(model->Sigma[i]);
        if (sigma_ik!=this->sigmaU)
        {
            Totsum=Totsum+1;
            sum10 = sum10+(sigma_ik-y0)/(sigma_ik+fabs(y0));
        }
        }
    if(y0==0)
  {
    sum10=1;
    double dy=0.1;
    double sumold;
    while(sum11>0)
    {
    y1=y1+dy;
    sumold=sum11;
    sum11=average(y1)/Totsum;
    }
    if(sumold!=1)
    {
        sum10=sumold;
        y0=y1-dy;
    }
  }
    else
    {
        sum10=average(y0)/Totsum;
        if(sum10 < 0) y1=0.5*y0;
        else y1=1.5*y0;
        sum11=average(y1)/Totsum;
    }
    double y2=0;
    double y11=y0;
    int j=0;
        while(fabs(y1-y11)>0.001*y1)
//        while(fabs(sum11-sum10)>0.01*fabs(sum10))
        {
            y2=y1-sum11*(y1-y0)/(sum11-sum10);
            double sum12=average(y2)/Totsum;
            j++;
            if(sum12>0&&sum11<0||sum12<0&& sum11>0)
            {
                sum10=sum11;
                y0=y1;
            }
/*
            else if(sum12<0.5&&sum10<0.5||sum12>0.5&& sum10>0.5)
            {
              if(sum10>sum) y0=y0-dy;//0.5;
              else y0=y0+dy;//0.5;//(EFT0+this->EF)/2;
              sum10=computeSum(NE, dE, Vd, EFT0);
            }
            */
              sum11=sum12;
              y11=y1;
              y1=y2;
        }

return y1;
//return (y1-yd)/yd;
}
/*
double MainWindow::effective_medium(void)
{
    this->computeModel();
    double yd=model->conductivity;
    double y0=0;
    double sum10=0;
    double Totsum=0;
        for (int i = 0; i < model->nI(); ++i)
      {
        double sigma_ik=fabs(model->Sigma[i]);
        if (sigma_ik!=this->sigmaU)
        {
            Totsum=Totsum+1;
//            sum10 = sum10+sigma_ik/(sigma_ik+fabs(y0));
        }
        }
        sum10=1;//sum10/Totsum;
    double dy=0.1;
    double y1=0;
    double sum11=1;
    double sumold;
    while(sum11>0.5)
    {
    y1=y1+dy;
    sumold=sum11;
    sum11=average(y1)/Totsum;
    }
    if(sumold!=1)
    {
        sum10=sumold;
        y0=y1-dy;
    }
    double y2;
    int j=0;
        while(fabs(sum11-0.5)>0.001)
        {
            y2=y1-(sum11-0.5)*(y1-y0)/(sum11-sum10);
            double sum12=average(y2)/Totsum;
            j++;
            if(sum12>0.5&&sum11<0.5||sum12<0.5&& sum11>0.5)
            {
                sum10=sum11;
                y0=y1;
            }
              sum11=sum12;
              y1=y2;

        }

return y1;
}
*/
void MainWindow::compute_deviation()
{
    double q;
    int isum=0;
    double sum1=0.;
    double Smax = -1e300;
    int G_type = this->typeCond->currentIndex();
        if(G_type!=4)
    {
        computeEF_TU();
    }
    int r_type = this->typeResistor->checkedId();
    this->selectSigma(r_type);
     for (int i = 0; i < model->nI(); ++i)
    {
        q = (model->Sigma[i]);
        if (q!=this->sigmaU)
        {
            isum=isum+1;
            sum1=sum1+q;
        if (fabs(q) > Smax) Smax = fabs(q);
        }
     }
    double sigma_av=sum1/isum;
    double d = -1e300;
     for (int i = 0; i < model->nI(); ++i)
    {
        q = (model->Sigma[i]);
        if (q!=this->sigmaU)
        {
        q= (q-sigma_av)/sigma_av;
        if (fabs(q) > d) d = fabs(q);
        }
     }
     this->deviation=d;
     this->deviation.updateDisplay();

}
/*void MainWindow::compute_all_distributions()
{
    winPlotV->show();
    winPlotV->raise();
    winPlotV->activateWindow();
    this->computeModel();
    int ni= model->nI();
    QVector<int> idx(ni);
    idx = model->index_for_sorted_difV();
//          idx = model->index_for_sorted_I();
//        idx = model->index_for_sorted_IdifV();
//    double v=fabs(model->difV[idx[ni]]);
//    double dVmin=-20;//log10(fabs(model->difV[idx[ni]]));
//    double dVmax=log10(2.);
    double dVmin=0.;///1e-12;//log10(fabs(model->difV[idx[ni]]));
    double dVmax=2.;
    int ndV=201;
    QVector<double> pdV( ndV );
    QVector<double> pI( nS );
    QVector<double> pCond( nS );
    pS.fill(0.0);
    double dpdV=(dVmax-dVmin)/(ndV-1);
    QVector<double> Vij(ni);
    QVector<double> data;
    int ev,ei;
    int iseed=this->seed;
    int jj=0;
    while (jj < nJ)
        {
           this->seed=iseed;
           iseed++;
           double cond=model->conductivity;
           if(cond>1e-12)
           {
               this->CondDist[j]=cond;
               jj=jj+1;

           for (int i = 0; i < ni; ++i)
        {
            double dV=fabs(model->difV[i]);
            double Ii=log10(fabs(model->I[i]));
          double sig=model->Sigma[idx[i]];
          if(sig<this->sigmaU){
             ei=int((Ii-Imin)/dpI);
              if(dV>dVmin)
        {
            ev=int((dV-dVmin)/dpdV);
        if (e < 0 || e > ndV-1)
        {
            QString s;
            s.sprintf("Got %i at i=%i\
                      n",e,i);
            QMessageBox::warning(this, tr("e out of bound"),s,
                QMessageBox::Ok,QMessageBox::Ok);
            break;
        }
                pdV[e]=pdV[e]+1;
        }
          }
        }
         this->computeModel();
//         idx = model->index_for_sorted_difV();
        }
         }

        double sumP=0.;
        double ln10=log(10.);
        for (int i = 0; i < nS; ++i)
        {
          double xi=dVmin+i*dS+0.5*dS;
          sumP=sumP+pS[i];
//          sumP=sumP+pS[i]*exp(xi*ln10);
        }
        sumP=sumP*dS;
//        double sum=0;
        for (int i = 0; i < nS; ++i)
        {
            double x=dVmin+i*dS+0.5*dS;
            double p=pS[i];
//            double p=pS[i]*exp(x*ln10);
//            if(p>0){
                data.push_back(x);
                data.push_back(p/sumP);
//            }
        }
         this ->plotterV->setCurveData(this->numOfCurve,data);
         this->numOfCurve++;
}*/
//1.
//Voltage dependence on x
void MainWindow::compute_Vx()
{
    winPlotU->show();
    winPlotU->raise();
    winPlotU->activateWindow();
    QVector<double> data;
        for (int v = 0; v < model->nV(); ++v)
    {
        double V = model->V[v];
        QPair<double,double> xy = model->xy(v);
        double y0=xy.second;
        if(y0==this->y_cr)
        {
        double x0=xy.first;
data.push_back(x0);
data.push_back(V);
this->plotterU->setCurveData(this->numOfCurve,data);
        }
    }

    int v = model->nV();
    for (int w = 0; w < model->nW(); ++w)
    {
        double W = model->W[w];
        QPair<double,double> xy = model->xy(v+w);
        double y0=xy.second;
        if(y0==this->y_cr)
        {
        double x0=xy.first;
data.push_back(x0);
data.push_back(W);
this->plotterU->setCurveData(this->numOfCurve,data);
        }
    }
    this->numOfCurve++;
}
//2.
//Voltage bias dependence on x
void MainWindow::compute_deltaVx()
{
    winPlotU->show();
    winPlotU->raise();
    winPlotU->activateWindow();
    QVector<double> data;

    double V_1,V_2,V_i;
    int nv=model->nV();
    V_2=model->W[0];
    int nI=model->nI();
    QVector<double> Vij(nI);
    for (int i = 0; i < model->nI(); ++i)
    {
        if(fabs(model->Sigma[i])!=this->sigmaU)
        {
    QPair<int,int> ends_i = model->ends(i);
    if(ends_i.first < nv) V_1=model->V[ends_i.first];
    else V_1=model->W[ends_i.first-nv];
    if(ends_i.second < nv) V_2=model->V[ends_i.second];
    else V_2=model->W[ends_i.second-nv];
        V_i=fabs((V_1-V_2));
//        V_i=fabs(model->I[i]*(V_1-V_2));
//        if(V_i<1e-20) V_i=1e-20;
//        if(V_i<0.1) V_i=0.1;
        V_i=fabs(V_i);
//        V_i=log10(fabs(V_i));
        Vij[i]=V_i;
        QPair<double,double> xy0 = model->xy(ends_i.first);
        QPair<double,double> xy1 = model->xy(ends_i.second);
        double x0=xy0.first;
        double x1=xy1.first;
        double y0=xy0.second;
        double y1=xy1.second;
        if(y0==y1&&y0==this->y_cr)
        {
data.push_back((x0+x1)*0.5);
data.push_back(Vij[i]);
this->plotterU->setCurveData(this->numOfCurve,data);
        }

        //	double IV_i = fabs(I_i * V_i);
        }
    }
    this->numOfCurve++;
}//3.a
//Voltage Distribution
void MainWindow::compute_p_small_V()
{
    winPlotV->show();
    winPlotV->raise();
    winPlotV->activateWindow();
        QVector<double> data;
        QVector<int> idx = model->index_for_sorted_W();
    int ix1=(this->cols-1)/2-15;
    int ix2=(this->cols-1)/2+15;
    int iy1=(this->rows-1)/2-15;
    int iy2=(this->rows-1)/2+15;
    int j=0;
        for (int i = 0; i < model->nW(); ++i)
        {
            QPair<double,double> xy = model->xy(idx[i]+model->nV());
        if (xy.first!=0 && xy.first!=1
            &&xy.first!=model->xmax()
            &&xy.first!=model->xmax()-1)
        {
            if((xy.first>=ix1&&xy.first<=ix2)&&
                (xy.second>=iy1&&xy.second<=iy2))
            {
                j=j+1;
            double x1=j;
            double x=x1;//model->nW();
            data.push_back(x);
            data.push_back(model->W[idx[i]]);//deltaV);
            }}
        }
        this ->plotterV->setCurveData(this->numOfCurve,data);
    this->numOfCurve++;

}
//3.b
//Voltage Distribution
void MainWindow::compute_pV()
{
    winPlotV->show();
    winPlotV->raise();
    winPlotV->activateWindow();
        QVector<double> data;
        QVector<int> idx = model->index_for_sorted_W();
        //double deltaV=2./(this->cols-3);
        for (int i = 0; i < model->nW(); ++i)
        { QPair<double,double> xy = model->xy(idx[i]+model->nV());
        if (xy.first!=0 && xy.first!=1
            &&xy.first!=model->xmax()
            &&xy.first!=model->xmax()-1)
        {
            double x1=i+1;
            double x=x1/model->nW();
//            double x=x1/model->nW();
            data.push_back(x);
            data.push_back(model->W[idx[i]]);//deltaV);
        }
        }
        this ->plotterV->setCurveData(this->numOfCurve,data);

/*        int nS=501;//1001;
    QVector<double> pS( nS );
    pS.fill(0.0);
    double dS=fabs(2./(nS-1));
    int e;
    int isum=0;
    for (int i = 0; i < model->nV(); ++i)
    {

        double q = model->V[i];
        QPair<double,double> xy = model->xy(i);
        if (xy.first!=0 && xy.first!=1
            &&xy.first!=model->xmax()
            &&xy.first!=model->xmax()-1)
        {

            e=int((q+1)/dS);
            if (e < 0 || e > nS-1)
            {
                QString s;
                s.sprintf("Got %i at i=%i\n",e,i);
                QMessageBox::warning(this, tr("e out of bound"),s,
                    QMessageBox::Ok,QMessageBox::Ok);
                break;
            }
            pS[e]=pS[e]+1;
            isum=isum+1;
        }
    }
    int ii=isum;
    int v = model->nV();


    for (int i = 0; i < model->nW(); ++i)
    {
        double q = model->W[i];
        QPair<double,double> xy = model->xy(i+v);
        if (xy.first!=0 && xy.first!=1
            &&xy.first!=model->xmax()
            &&xy.first!=model->xmax()-1)
        {


            e=int((q+1)/dS);
            if (e < 0 || e > nS-1)
            {
                QString s;
                s.sprintf("Got %i at i=%i\n",e,i);
                QMessageBox::warning(this, tr("e out of bound"),s,
                    QMessageBox::Ok,QMessageBox::Ok);
                break;
            }
            pS[e]=pS[e]+1;
            isum=isum+1;
        }
    }
    ii=isum;
    double sum=0.;
    data.push_back(-1.);
    data.push_back(0.);
    for (int i = 0; i < nS; ++i)
    {
        //            double x=-1+i*dS;
        double x=-1+i*dS+0.5*dS;
        data.push_back(x);
        double p=pS[i];///isum;
        sum=sum+p;
        data.push_back(p);
        //            data.push_back(sum);
        this ->plotterU->setCurveData(this->numOfCurve,data);
    }
    */
    this->numOfCurve++;
}
//4.b
//Difference Voltage Distribution
void MainWindow::compute_p_small_dV()
{
    winPlotV->show();
    winPlotV->raise();
    winPlotV->activateWindow();
    this->computeModel();
    QVector<int> idx;
    idx = model->index_for_sorted_difV();
    QVector<double> data;
    QVector<double> Vij(model->nI());
    int ix1=(this->cols-1)/2-15;
    int ix2=(this->cols-1)/2+15;
    int iy1=(this->rows-1)/2-15;
    int iy2=(this->rows-1)/2+15;
    int j;
int lmax=30;
        for (int l = 0; l < lmax; ++l)
        {
            j=0;
        for (int i = 0; i < model->nI(); ++i)
        {
        double y=fabs(model->difV[idx[i]]);
        QPair<int,int> ends = model->ends(idx[i]);
        int from = ends.first;
        int to   = ends.second;
        QPair<double,double> xy0 = model->xy(from);
        QPair<double,double> xy1 = model->xy(to);
        if(xy0.first<=ix2&&xy1.first<=ix2&&xy0.first>=ix1&&xy1.first>=ix1&&
            xy0.second<=iy2&&xy1.second<=iy2&&xy0.second>=iy1&&xy1.second>=iy1)
        {
//         if(y>0)
            {
            if(l==0) Vij[j]=0.;
             Vij[j]=Vij[j]+y;
             j=j+1;
//            data.push_back(j);
//            data.push_back(y);
            }

         if(l==(lmax-1))
            {
            data.push_back(j+1);
            if(Vij[j]>0) data.push_back(Vij[j]/lmax);
            }
         }
        }
        if(l!=(lmax-1)) {
            this->computeModel();
            idx = model->index_for_sorted_difV();
        }

        }
         this ->plotterV->setCurveData(this->numOfCurve,data);
         this->numOfCurve++;
}
//4.
//Difference Voltage Distribution
void MainWindow::compute_pdV()
{
//    this->computeModel();
//    int ni= model->nI();
//1. range dVmin, dVmax for distribution of voltage difference
//    QVector<int> idV = model->index_for_sorted_difV();
//    double v=fabs(model->difV[idV[ni-1]]);
    double dVmin;
    double dVmax=log(2.);
//    double dVmax=log10(2.);
/*    if(v>0&&v>this->CUTOFF_SIGMA)
    {
        dVmin=log(v);
//        dVmin=log10(v);
        double delta1=(dVmax-dVmin)/5;
        dVmin=dVmin-2.3*delta1;
    }
    else dVmin=log(this->CUTOFF_SIGMA);
    */
    dVmin=log(1.e-20);
    int npV=201;
    QVector<double>pdV;
    pdV.resize(npV+2);
//    QVector<double> pdV( npV+2 );
//    pdV.fill(0.0);
    double dV=(dVmax-dVmin)/(npV+1);
//2. range imin, imax for current distribution
/*    QVector<int> idI = model->index_for_sorted_I();
    double imin=fabs(model->I[idI[ni-1]]);
    double imax=fabs(model->I[idI[0]]);
    imax=log(imax);
//    imax=log10(imax);
    if(imin>0&&imin>this->CUTOFF_SIGMA)
    {
        imin=log(imin);
//        imin=log10(imin);
        double delta2=(imax-imin)/5;
        imax=imax+6*delta2;
        imin=imin-4*delta2;
    }
    else imin=log(this->CUTOFF_SIGMA);
    */
    double imin=log(1.e-25);
    double imax=-1.;
    //    if(imin<this->CUTOFF_SIGMA) imin=log10(this->CUTOFF_SIGMA);
    int npI=201;
    QVector<double> pI( npI+3 );
    pI.fill(0.0);
    double dI=(imax-imin)/(npI+1);
//3. range Jmin, Jmax for distribution of Joule heat
/*    QVector<int> idJ = model->index_for_sorted_IdifV();
    double Jmin=fabs(model->IdifV[idJ[ni-1]]);
    double Jmax=fabs(model->IdifV[idJ[0]]);
    Jmax=log(Jmax);
//    Jmax=log10(Jmax);
    if(Jmin>0&&Jmin>this->CUTOFF_SIGMA)
    {
        Jmin=log(Jmin);
//        Jmin=log10(Jmin);
        double delta2=(Jmax-Jmin)/5;
        Jmin=Jmin-4*delta2;
//        Jmax=Jmax+2.3*delta2;
    }
    else Jmin=log(this->CUTOFF_SIGMA);
    Jmin=imin;
//    if(Jmin<this->CUTOFF_SIGMA) Jmin=log10(this->CUTOFF_SIGMA);
*/
    int npJ=201;
    QVector<double> pJ( npJ+3 );
    pJ.fill(0.0);
    double Jmin=log(1.e-25);
    double Jmax=-1.;
    double dJ=(Jmax-Jmin)/(npJ+1);
    this->CondDist.resize(nJ);
    this->NumJDist.resize(nJ);
    int e;
       int iseed=this->seed;
//       int j=0;
       double sumI=0;
//       QVector<double> numJmax;
       double y_old=0;
       double sigma_mid=0;
    for (int j = 0; j < nJ; ++j)
    {
      int ni;
      double yy, yn;
      for (;;) // in fact we have only 10 seeds to try in the worst case
    {
           this->seed=iseed;
           this->computeModel();
           iseed++;
           ni= model->nI();
           yy=model->conductivity;
           yn=model->capacity;
           if (yy > Cond_min) break;
    }
        this->CondDist[j]=yy;
        double yeff=effective_medium(y_old);
        y_old=yeff;
        this->NumJDist[j]=yeff;//yn;
        sigma_mid=sigma_mid+yeff;
        for (int i = 0; i < ni; ++i)
        {
            double Vi=log(fabs(model->difV[i]));
            double Ji=log(fabs(model->IdifV[i]));
            double Ii=log(fabs(model->I[i]));
          if(model->Sigma[i]<this->sigmaU)
          {
           sumI=sumI+1;
//current distribution
          if(Ii<imin)
          {
              pI[0]=pI[0]+1;
          }
          else
          {
          e=1+int((Ii-imin)/dI);
          if(e>(npI+2)) pI[npI+2]=pI[npI+2]+1;
          if(e==(npI+2)) pI[e-1]=pI[e-1]+1;
          if(e<(npI+2)) pI[e]=pI[e]+1;
          }
//Joule heat distribution
          if(Ji<Jmin)
          {
              pJ[0]=pJ[0]+1;
          }
          else
          {
          e=1+int((Ji-Jmin)/dJ);
          if(e>(npJ+2)) pJ[npJ+2]=pJ[npJ+2]+1;
          if(e==(npJ+2)) pJ[e-1]=pJ[e-1]+1;
          if(e<(npJ+2)) pJ[e]=pJ[e]+1;
          }
//voltage dufference distribution
          if(Vi<dVmin)
          {
              pdV[0]=pdV[0]+1;
          }
          else
          {
          e=1+int((Vi-dVmin)/dV);
          if(e>=(npV+2)) pdV[e-1]=pdV[e-1]+1;
          if(e<(npV+2)) pdV[e]=pdV[e]+1;
          }
          }
          }
        }
//voltage difference
    winPlotV->show();
    winPlotV->raise();
    winPlotV->activateWindow();
    QVector<double> dataV;
//    double x0=double(this->cols)/pow(double(this->kappa),4./3.);
    double x0=double(this->cols-3);
    x0=log(x0);
//    x0=log10(x0);
    double sumV=0.;
        double ln10=log(10.);
        for (int i = 0; i < npV+2; ++i)
        {
//          double xi=dVmin+i*dV-0.5*dV;
          sumV=sumV+pdV[i]*dV;
        }
        sumV=nJ*dV;
                dataV.push_back(dVmin/x0);
        if(pdV[0]>0) dataV.push_back(log(pdV[0]/sumV)/x0);
//        if(pdV[0]>0) dataV.push_back(log10(pdV[0]/sumV/ln10)/x0);
        else     dataV.push_back(log(this->CUTOFF_SIGMA));
        for (int i = 1; i < npV+2; ++i)
        {
            double x=dVmin+i*dV-0.5*dV;
            double p=pdV[i];
                dataV.push_back(x/x0);
            if(p>0) dataV.push_back(log(p/sumV)/x0);
//            if(p>0) dataV.push_back(log10(p/sumV/ln10)/x0);
            else dataV.push_back(log(this->CUTOFF_SIGMA));
        }
         this ->plotterV->setCurveData(this->numOfCurvedV,dataV);
         this->numOfCurvedV++;
//---------current
    winPlotI->show();
    winPlotI->raise();
    winPlotI->activateWindow();
    QVector<double> dataI;
        double sumCur=0.;
        for (int i = 0; i < npV+2; ++i)
        {
          sumCur=sumCur+pI[i]*dI;
        }
         sumCur=nJ*dI;
                dataI.push_back(imin/x0);
         if(pI[0]>0) dataI.push_back(log(pI[0]/sumCur)/x0);
//         if(pI[0]>0) dataI.push_back(log10(pI[0]/sumCur/ln10)/x0);
         else dataI.push_back(log(this->CUTOFF_SIGMA));
        for (int i = 1; i < npI+2; ++i)
        {
            double x=imin+i*dI-0.5*dI;
            double p=pI[i];
            dataI.push_back(x/x0);
         if(p>0) dataI.push_back(log(p/sumCur)/x0);
//         if(p>0) dataI.push_back(log10(p/sumCur/ln10)/x0);
         else dataI.push_back(log(this->CUTOFF_SIGMA));
        }
         dataI.push_back(imax/x0);
//         if(pI[npI+2]>0) dataI.push_back(log10(pI[npI+2]/sumCur/ln10)/x0);
         if(pI[npI+2]>0) dataI.push_back(log(pI[npI+2]/sumCur)/x0);
         else dataI.push_back(log(this->CUTOFF_SIGMA));
         this->plotterI->setCurveData(this->numOfCurveI,dataI);
         this->numOfCurveI++;
//---------Joule Heat
    winPlotJ->show();
    winPlotJ->raise();
    winPlotJ->activateWindow();
    QVector<double> dataJ;
//    double x1=this->kappa;
        double sumJ=0.;
        for (int i = 0; i < npV+3; ++i)
        {
          sumJ=sumJ+pJ[i]*dJ;
        }
        sumJ=nJ*dJ;
//        dataJ.push_back(Jmin);
        dataJ.push_back(Jmin/x0);
    if(pJ[0]>0){
        double yn=log(pJ[0]/sumJ)/x0;
        dataJ.push_back(yn);
    }
         else dataJ.push_back(log(this->CUTOFF_SIGMA));
        for (int i = 1; i < npJ+2; ++i)
        {
            double x=Jmin+i*dJ-0.5*dJ;
            double p=pJ[i];
            double yn=log(p/sumJ)/x0;
//            dataJ.push_back(x);
            dataJ.push_back(x/x0);
            if(p>0) dataJ.push_back(yn);
         else dataJ.push_back(log(this->CUTOFF_SIGMA));
        }
//         dataJ.push_back(Jmax);
         dataJ.push_back(Jmax/x0);
         if(pJ[npJ+2]>0) dataJ.push_back(log(pJ[npJ+2]/sumJ)/x0);
         else dataJ.push_back(log(this->CUTOFF_SIGMA));
         this->plotterJ->setCurveData(this->numOfCurveJ,dataJ);
         this->numOfCurveJ++;
//---------conductance
//    this->CondDist.resize(nJ,0.0);
    QVector<int> idxS = this->index_for_sorted_CondDist();
{
  int k;
  FILE *f = fopen("cd1.txt","wt");
  for (k=0; k<CondDist.size(); ++k)
    fprintf(f,"CondDist[%i]=%lg\n",k,CondDist[k]);
  for (k=0; k<idxS.size(); ++k)
    fprintf(f,"sorted.CondDist[%i]=%lg\n",idxS[k],CondDist[idxS[k]]);
  fclose(f);
}

    QVector<int> idxNumJ = this->index_for_sorted_NumJDist();
    int nS1=51;
    int numJ=51;
    sigma_mid=sigma_mid/nJ;
    QVector<double> pS1( nS1+1 );// from 0-box to Ns-box
    QVector<double> pNumJ( numJ+1 );// from 0-box to Ns-box
    pS1.fill(0.0);
    pNumJ.fill(0.0);
    int jmin=idxS[0];
    int jmax=idxS[nJ-1];
    int jminJ=idxNumJ[0];
    int jmaxJ=idxNumJ[nJ-1];
    double Smin1=CondDist[jmin];
    double Smax1=CondDist[jmax];
    if( Smin1>0) Smin1=log(Smin1);
//    if( Smin1>0) Smin1=log10(Smin1);
    else Smin1=-15*2.3;
     Smax1=log(Smax1);
//     Smax1=log10(Smax1);
    double dS1=(Smax1-Smin1)/(nS1+1);
    double NumJmin=NumJDist[jminJ];
    double NumJmax=NumJDist[jmaxJ];
    if(NumJmin>0) NumJmin=log(NumJmin);
    else NumJmin=-15*2.3;
    NumJmax=log(NumJmax);
    double dnumJ=(NumJmax-NumJmin)/(numJ+1);
    for (int i = 0; i < nJ; ++i)
        {
          double y=log(CondDist[idxS[i]]);
          double yn=log(NumJDist[idxNumJ[i]]);
//          double yn=NumJDist[idxNumJ[i]];
          e=int((y-Smin1)/dS1);
          int e1=int((yn-NumJmin)/dnumJ);
          if(e==(nS1+1)) pS1[e-1]=pS1[e-1]+1;
          else pS1[e]=pS1[e]+1;
          if(e1==(numJ+1)) pNumJ[e1-1]=pNumJ[e1-1]+1;
          else pNumJ[e1]=pNumJ[e1]+1;
        }
        ln10=log(10.);
    double sumP=nJ*dS1;
    winPlotG->show();
    winPlotG->raise();
    winPlotG->activateWindow();
    QVector<double> data1;
        for (int i = 0; i < (nS1+1); ++i)
        {
            double x=Smin1+i*dS1+0.5*dS1;
            double p=pS1[i]*exp(x);
//            double p=pS1[i]*exp(x*ln10);
                data1.push_back(x/x0);
            if(p>0) data1.push_back(log(p/sumP)/x0);
//            if(p>0) data1.push_back(log10(p/sumP)/x0);
            else data1.push_back(log(this->CUTOFF_SIGMA));
        }
    this->plotterG->setCurveData(this->numOfCurveSig,data1);
    this->numOfCurveSig++;
    QVector<double> data0;
    data0.push_back(log(sigma_mid)/x0);
    data0.push_back(-1);
    data0.push_back(log(sigma_mid)/x0);
    data0.push_back(-10);
    this->plotterG->setCurveData(this->numOfCurveSig,data0);
    this->numOfCurveSig++;
/*    QVector<double> datat;
    datat.push_back(-0.5*this->kappa/x0);
    datat.push_back(-1);
    datat.push_back(-0.5*this->kappa/x0);
    datat.push_back(-10);
    this->plotterG->setCurveData(this->numOfCurveSig,datat);
    this->numOfCurveSig++;*/
    double x2=this->cols-3;
    x2=x2*x2;
    double sumpJ=nJ*dnumJ;
    winPlotCU->show();
    winPlotCU->raise();
    winPlotCU->activateWindow();
    QVector<double> data;
        for (int i = 0; i < (numJ+1); ++i)
        {
            double x=NumJmin+(i+0.5)*dnumJ;
            double p=pNumJ[i]*exp(x);
            data.push_back(x/x0);
            if(p>0) data.push_back(log(p/sumpJ)/x0);
//            if(p>0) data.push_back(p/sumpJ);
//            else data.push_back(this->CUTOFF_SIGMA);
            else data.push_back(log(this->CUTOFF_SIGMA));
        }
    this->plotterG->setCurveData(this->numOfCurveSig,data);
    this->numOfCurveSig++;
//    this->plotterCU->setCurveData(this->numOfCurve,data1);
//    this->numOfCurve++;

}
    //--------------------------
/*        int ix=1;
        for (int i = 0; i < ni; ++i)
        {
//            double sig=model->Sigma[idx[i]];
//          if(sig<this->sigmaU){
            if(j==0) Vij[i]=0.;
            double x1=ix;//(i+1);
            double x=x1;
//            double x=x1/ni;
//            double y=(model->difV[i]);
            double y=fabs(model->difV[idx[i]]);
//            double y=fabs(model->I[idx[i]]);
//          double y=fabs(model->IdifV[idx[i]]);
            Vij[i]=y+Vij[i];
         if(j==(nJ-1))//&&Vij[i]>0.1*Vij[0])
            {
            data.push_back(x);
            ix=ix+1;
            data.push_back(Vij[i]);
//          data.push_back((Vij[i]*0.5*(this->cols-4))/jmax);
            }
//          }
          }
        this ->plotterV->setCurveData(this->numOfCurve,data);
*/
    /*
        double Vmin = 1e300, Vmax = -1e300;
    double q,V_1,V_2,V_i;
    int nv=model->nV();
    V_2=model->W[0];
    int nI=model->nI();
    QVector<double> Vij(nI);
    for (int i = 0; i < model->nI(); ++i)
    {
        if(fabs(model->Sigma[i])!=this->sigmaU)
        {
    QPair<int,int> ends_i = model->ends(i);
    if(ends_i.first < nv) V_1=model->V[ends_i.first];
    else V_1=model->W[ends_i.first-nv];
    if(ends_i.second < nv) V_2=model->V[ends_i.second];
    else V_2=model->W[ends_i.second-nv];
        V_i=fabs((V_1-V_2));
//        V_i=fabs(model->I[i]*(V_1-V_2));
//        if(V_i<1e-20) V_i=1e-20;
//        if(V_i<0.1) V_i=0.1;
        V_i=fabs(V_i);
//        V_i=log10(fabs(V_i));
        Vij[i]=V_i;

//	double IV_i = fabs(I_i * V_i);
        double q = V_i;
        if (q > Vmax) Vmax = q;
        if (q < Vmin) Vmin = q;
        }
    }
            mysort(Vij);
        for (int i = 0; i < nI; ++i)
        {
//            if(Vij[i]>0) {
            data.push_back(i+1);
            data.push_back(Vij[i]);
            this ->plotterV->setCurveData(this->numOfCurve,data);
//            }
        }
*/
/*        int nS=501;//1001;
    QVector<double> pS( nS );
    pS.fill(0.0);
//    Vmin=-20.;
    double dS=(Vmax-Vmin)/(nS-1);
    int e;
    int isum=0;
    for (int i = 0; i < nI; ++i)
        {
        if(fabs(model->Sigma[i])!=this->sigmaU)
        {

            q = Vij[i];

                e=int((q-Vmin)/dS);
        if (e < 0 || e > nS-1)
        {
            QString s;
            s.sprintf("Got %i at i=%i\n",e,i);
            QMessageBox::warning(this, tr("e out of bound"),s,
                QMessageBox::Ok,QMessageBox::Ok);
            break;
        }
                pS[e]=pS[e]+1;

//                line[e]+=1;
                isum=isum+1;
        }
        }
        double sum=0.;
//        int isum=model->nI();
        for (int i = 0; i < nS; ++i)
        {
            double x=Vmin+i*dS+0.5*dS;
            data.push_back(x);
            double p=pS[i];//isum;
            sum=sum+p;
            data.push_back(p);
//            data.push_back(sum);
            this ->plotterV->setCurveData(this->numOfCurve,data);
        }
*/
//        this->numOfCurve++;

//}
//Current Distribution
void MainWindow::compute_pCurrent()
{
    winPlotI->show();
    winPlotI->raise();
    winPlotI->activateWindow();
       QVector<double> data;
        QVector<int> idx = model->index_for_sorted_I();
        for (int i = 0; i < model->nI(); ++i)
        {
            double sig=model->Sigma[idx[i]];
          if(sig<this->sigmaU){
            double x1=i+1;
            double x=x1/model->nI();
            data.push_back(x);
            data.push_back(fabs(model->I[idx[i]]));
      }
        }
        this->plotterI->setCurveData(this->numOfCurve,data);
/*    for (int i = 0; i < model->nI(); ++i)
    {
        if(fabs(model->Sigma[i])!=this->sigmaU)
        {
        V_i=fabs(model->I[i]);
        Vij[i]=V_i;

        double q = V_i;
        if (q > Vmax) Vmax = q;
        if (q < Vmin) Vmin = q;
        }
        }

        mysort(Vij);
    int nS=501;//1001;
    QVector<double> pS( nS );
    pS.fill(0.0);
    double dS=(Vmax-Vmin)/(nS-1);
    int e;
    int isum=0;
    for (int i = 0; i < nI; ++i)
        {
        if(fabs(model->Sigma[i])!=this->sigmaU)
        {

            q = Vij[i];

                e=int((q-Vmin)/dS);
        if (e < 0 || e > nS-1)
        {
            QString s;
            s.sprintf("Got %i at i=%i\n",e,i);
            QMessageBox::warning(this, tr("e out of bound"),s,
                QMessageBox::Ok,QMessageBox::Ok);
            break;
        }
                pS[e]=pS[e]+1;

//                line[e]+=1;
                isum=isum+1;
        }
        }
        double sum=0.;
//        int isum=model->nI();
        for (int i = 0; i < nS; ++i)
        {
            double x=(Vmin+i*dS+0.5*dS);///Vmax;
//            if(x>0.1*Vmax) {
            data.push_back(x);
            double p=pS[i]/isum;
            sum=sum+p;
            data.push_back(p);
//            data.push_back(sum);
            this ->plotterU->setCurveData(this->numOfCurve,data);
//            }
        }
        */
    this->numOfCurve++;
}

QVector<int> MainWindow::index_for_sorted_CondDist() const
{
    QVector<int> res(nJ);
    for (int i=0; i<res.size(); ++i)
    {
        res[i] = i;
    }

    struct MyLessThan
    {
        const     QVector<double>& w;
        MyLessThan(const  QVector<double>& w_) : w(w_) {}
        bool operator()(int a, int b) const { return w[a] < w[b]; }
    };

    qSort(res.begin(),res.end(),MyLessThan(CondDist));

    return res;
}
QVector<int> MainWindow::index_for_sorted_NumJDist() const
{
    QVector<int> res(nJ);
    for (int i=0; i<res.size(); ++i)
    {
        res[i] = i;
    }

    struct MyLessThan
    {
        const     QVector<double>& w;
        MyLessThan(const  QVector<double>& w_) : w(w_) {}
        bool operator()(int a, int b) const { return w[a] < w[b]; }
    };

    qSort(res.begin(),res.end(),MyLessThan(NumJDist));

    return res;
}
//Resistance Distribution
void MainWindow::compute_pSigma()
{
    winPlotG->show();
    winPlotG->raise();
    winPlotG->activateWindow();
       QVector<double> data;
       QVector<double> data1;
         int r_type = this->typeResistor->checkedId();//?
         this->selectSigma(r_type);//?
         QVector<int> idx = model->index_for_sorted_Sigma();
         this->computeModel();
         double cond=model->conductivity;
         double x,xcr;
         int nx=model->nI()-6*this->rows+4;

       for (int i = 0; i < model->nI(); ++i)
        {
            if(model->Sigma[idx[i]]<this->sigmaU){
//            double x1=i+1;
//            x=x1/model->nI();
            if(model->Sigma[idx[i]]>cond) xcr=i-6*this->rows+4;
            double x1=i-6*this->rows+4;
            x=x1/nx;
            data.push_back(x);
            data.push_back(model->Sigma[idx[i]]);
            }

        }
      this ->plotterG->setCurveData(this->numOfCurve,data);
      this->numOfCurve++;
         data1.push_back(xcr/nx);
           data1.push_back(cond);
           data1.push_back(xcr/nx);
           data1.push_back(5*cond);

      this ->plotterG->setCurveData(this->numOfCurve,data1);
      this->numOfCurve++;

}
    void MainWindow::compute_SumpSigma()
{
    winPlotU->show();
    winPlotU->raise();
    winPlotU->activateWindow();
    QVector<double> data;
    int G_type = this->typeCond->currentIndex();
    if(G_type!=4)
    {
        computeEF_TU();
    }

    int r_type = this->typeResistor->checkedId();
    this->selectSigma(r_type);

    double Smin = 1e300, Smax = -1e300;
    double q;
    int isum=0;
     for (int i = 0; i < model->nI(); ++i)
    {
        q = (model->Sigma[i]);
        if (q!=this->sigmaU) isum=isum+1;
        if (fabs(q) > Smax&&q!=this->sigmaU) Smax = fabs(q);
        if (fabs(q) < Smin) Smin = fabs(q);
    }
    int nS=1001;
    QVector<double> pS( nS );
    pS.fill(0.0);
    Smin=0;
    double dS=(Smax-Smin)/(nS-1);
    int e;
//    int ni=model->nI();
        for (int i = 0; i < model->nI(); ++i)
        {

            q = (model->Sigma[i]);
            if(q!=this->sigmaU)
//            if(q!=this->sigmaU&&q>CUTOFF_SIGMA)
            {
                e=int((q-Smin)/dS);
        if (e < 0 || e > nS-1)
        {
            QString s;
            s.sprintf("Got %i at i=%i\n",e,i);
            QMessageBox::warning(this, tr("e out of bound"),s,
                QMessageBox::Ok,QMessageBox::Ok);
            break;
        }
                pS[e]=pS[e]+1;
            }
        }
        double sum=0.;
        for (int i = 0; i < nS; ++i)
        {
            double x=Smin+i*dS+0.5*dS;
            data.push_back(x);
            double p=pS[i]/isum;
            sum=sum+p;
            data.push_back(sum);
            this->plotterU->setCurveData(this->numOfCurve,data);
        }
    this->numOfCurve++;
}



void MainWindow::stopCalc()
{
    this->flgStop=true;
}

double MainWindow::AreaE(double E)
{
    double Uxy,x1,x2, y1, y2, r1, r2, r3, r4;
    double Area=0;
    for (double x =0.5; x <= 499.5; x += 1)
    {
        for (double y =0.5; y <= 499.5; y += 1)
        {
            if((x-500)*(x-500)+(y-500)*(y-500)>=122500)//122500=350^2
            {
            x1=500+x;
            x2=500-x;
            y1=500+y;
            y2=500-y;
            r1=sqrt(x1*x1+y2*y2)-350;
            r2=sqrt(x2*x2+y2*y2)-350;
            r3=sqrt(x1*x1+y1*y1)-350;
            r4=sqrt(x2*x2+y1*y1)-350;
            if(r1>0&&r2>0&&r3>0&&r4>0)
            {
            r1=r1/this->Delta_r;
            r2=r2/this->Delta_r;
            r3=r3/this->Delta_r;
            r4=r4/this->Delta_r;
            Uxy=this->U*(1/(1+r1*r1)+1/(1+r2*r2)+1/(1+r3*r3)+1/(1+r4*r4));
            if(Uxy<=E) Area=Area+1;
            }
            }
        }
    }
    return Area;
}
void MainWindow::computeEF_TU()
{
    double E,EFT0,EFT1,EFT2 ;
    double dE=0.1;
    double sum, sum1, Area, sum10, sum11, sum12;
    double Ucur=this->U;//!!!!!!!!!!!
    this->U=Vg0;
    double Vd0=Vdot();
    this->U=Ucur;
    double EF00=this->EF0;

//    double aa=(sqrt(1250.)-350)/this->Delta_r;
    double aa=(1000*sqrt(1.25)-350)/this->Delta_r;
    aa=aa*aa;
    aa=4*this->U/(1+aa);
//    aa=0;
    if(this->T==0)
    {
        EFT1=aa+EF00+Vdot()-Vd0+this->Cg0*(Ucur-Vg0);//!!!!!!!!!!
    }
    else
    {
        //T!=0
    double Vd=Vdot();
    this->EF=aa+EF00+Vd-Vd0+this->Cg0*(Ucur-Vg0);
    int NE=(this->EF+40*this->T-Vd)/dE;
    this->AreaEf.resize(NE);
    sum=0;
    EFT1=this->EF-1;//!!!!!!!!!!!!!!!!!!!!!!!!
    for (int i=0; i< NE; ++i)
    {
        E=dE*(i+1)+Vd;
        Area=AreaE(E)/10000;
        this->AreaEf[i]=Area;
        if(E<=this->EF) sum=sum+Area;
    }
        this->density=sum;
    if(sum<0)
    {
            QString s;
            s.sprintf("dot is empty at  U=%lg\n",this->U);
            QMessageBox::warning(this, tr("Warning!!!"),s,
                QMessageBox::Ok,QMessageBox::Ok);
    }
    else
    {
        EFT0=EFT1;//this->EF-1.;//!!!!!!!!!!!!!!!!!
        sum1=computeSum(NE, dE, Vd, EFT0);
    while(sum1>sum)
    {
    EFT0=EFT0-1;
    sum1=computeSum(NE, dE, Vd, EFT0);
    }
        sum10=sum1;
        EFT1=EFT0+1;
        sum11=computeSum(NE, dE, Vd, EFT1);
        int j=0;
        while(fabs(sum11-sum)>0.001*sum)
//        while(fabs(sum11-sum)>0.001)
        {
            EFT2=EFT1-(sum11-sum)*(EFT1-EFT0)/(sum11-sum10);
            sum12=computeSum(NE, dE, Vd, EFT2);
            j++;
            if(sum12>sum&&sum11<sum||sum12<sum&& sum11>sum)
            {
                sum10=sum11;
                EFT0=EFT1;
            }
/*            else if(sum12<sum&&sum10<sum||sum12>sum&& sum10>sum)
            {
              if(sum10>sum) EFT0=EFT0-1;//0.5;
              else EFT0=EFT0+1;//0.5;//(EFT0+this->EF)/2;
              sum10=computeSum(NE, dE, Vd, EFT0);
            }
*/
                sum11=sum12;
                EFT1=EFT2;
        }
    }
    }
        this->EFT=EFT1;
        this->EFT.updateDisplay();
    }


void MainWindow::computeEFU()
{
    double E,EFT0,EFT1,EFT2 ;
    double dE=0.1;
    double sum, sum1, Area, sum10, sum11, sum12;
    int NU=1+(this->Umax-this->Umin)/this->dU;
    this->EFUarray.resize(NU);
    double Ucur=this->U;//!!!!!!!!!!!
    this->U=Vg0;
    double Vd0=Vdot();
//    double aa1=(sqrt(1250.)-350)/this->Delta_r;
    double aa1=(1000*sqrt(1.25)-350)/this->Delta_r;
    aa1=aa1*aa1;
    aa1=4/(1+aa1);
//    aa1 = 0;
    double EF00=this->EF0;
    if(this->T==0) {
//    int j=0;
    for(int l=0; l<NU; l++)
//    for(double x=this->Umin; x<=this->Umax; x+=this->dU)
    {
        this->U=this->Umin+this->dU*l;
//        this->U=x;
        double aa=aa1*this->U;
        EFT1=aa+EF00+Vdot()-Vd0+this->Cg0*(this->U-Vg0);
//            if(j<this->EFUarray.size())
            {
            this->EFUarray[l]=EFT1;
//            this->EFUarray[j]=EFT1;
//                j++;
                this->EFT=EFT1;
                this->EFT.updateDisplay();
            }
/*            else
            {
            QString s;
            s.sprintf("element of array of j=%i\n",j);
            QMessageBox::warning(this, tr("is out of bound"),s,
                QMessageBox::Ok,QMessageBox::Ok);

            }
*/    }
    return;
    }
//    int j=0;
//    for(double x=this->Umin; x<=this->Umax; x+=this->dU)
    for(int l=0; l<NU; l++)
    {
        this->U=this->Umin+this->dU*l;
//        this->U=x;
        double Vd=Vdot();
        double aa=aa1*this->U;
//        aa=0;
        this->EF=aa+EF00+Vd-Vd0+this->Cg0*(this->U-Vg0);
    int NE=(this->EF+40*this->T-Vd)/dE;
    this->AreaEf.resize(NE);
    sum=0;
    EFT1=this->EF-1;//!!!!!!!!!!!!!!!!!!!!!!!!
    for (int i=0; i< NE; ++i)
    {
        E=dE*(i+1)+Vd;
        Area=AreaE(E)/10000;
        this->AreaEf[i]=Area;
        if(E<=this->EF) sum=sum+Area;
    }
        this->density=sum;
    if(sum==0)
    {
            QString s;
            s.sprintf("dot is empty at  U=%lg\n",this->U);
            QMessageBox::warning(this, tr("Warning!!!"),s,
                QMessageBox::Ok,QMessageBox::Ok);
 //            j++;
    }
    else
    {
        EFT0=EFT1;//this->EF-1.;//!!!!!!!!!!!!!!!!!
        sum1=computeSum(NE, dE, Vd, EFT0);
    while(sum1>sum)
    {
    EFT0=EFT0-1;
    sum1=computeSum(NE, dE, Vd, EFT0);
    }
        sum10=sum1;
        EFT1=EFT0+1;
        sum11=computeSum(NE, dE, Vd, EFT1);
        while(fabs(sum11-sum)>0.001*sum)
        {
            EFT2=EFT1-(sum11-sum)*(EFT1-EFT0)/(sum11-sum10);
            sum12=computeSum(NE, dE, Vd, EFT2);
            if(sum12>sum&&sum11<sum||sum12<sum&& sum11>sum)
            {
                sum10=sum11;
                EFT0=EFT1;
            }
/*            else if(sum12<sum&&sum10<sum||sum12>sum&& sum10>sum)
            {
              if(sum10>sum) EFT0=EFT0-1;//0.5;
              else EFT0=EFT0+1;//0.5;//(EFT0+this->EF)/2;
              sum10=computeSum(NE, dE, Vd, EFT0);
            }
            */
                sum11=sum12;
                EFT1=EFT2;
        }
//            if(j<this->EFUarray.size())
            {
            this->EFUarray[l]=EFT1;
//            this->EFUarray[j]=EFT1;
//                j++;
                this->EFT=EFT1;
                this->EFT.updateDisplay();
            }
/*            else
            {
            QString s;
            s.sprintf("element of array of j=%i\n",j);
            QMessageBox::warning(this, tr("is out of bound"),s,
                QMessageBox::Ok,QMessageBox::Ok);
            break;
            }*/

    }
    }
}
double MainWindow::computeSum(int NE, double dE, double Vd, double EFT)
{
        double sum=0;
        for (int i=0; i< NE; ++i)
        {
            double E=dE*(i+1)+Vd;
            sum=sum+this->AreaEf[i]/(1+exp((E-EFT)/this->T));
        }
return sum;
}
void MainWindow::computeAreaE()
{
    winPlotU->show();
    winPlotU->raise();
    winPlotU->activateWindow();
    QVector<double> data;
    double E,EFT0,EFT1,EFT2 ;
    double dE=0.1;
    double sum, sum1, Area, sum10, sum11, sum12;
    int NU=1+(this->Umax-this->Umin)/this->dU;
    this->EFUarray.resize(NU);
    this->U=Vg0;
    double Vd0=Vdot();
//    double aa1=(sqrt(1250)-350)/this->Delta_r;
    double aa1=(1000*sqrt(1.25)-350)/this->Delta_r;
    aa1=aa1*aa1;
    aa1=4/(1+aa1);
    double EF00=this->EF0;
    if(this->T==0) {
    int j=0;
    for(double x=this->Umin; x<=this->Umax; x+=this->dU)
    {   this->U=x;
        double aa=aa1*this->U;
//        aa=0;
        EFT1=aa+EF00+Vdot()-Vd0+this->Cg0*(this->U-Vg0);
        if(j<this->EFUarray.size())
            {
                this->EFUarray[j]=EFT1;
                j++;
                this->EFT=EFT1;
                this->EFT.updateDisplay();
            }
            else
            {
            QString s;
            s.sprintf("element of array of j=%i\n",j);
            QMessageBox::warning(this, tr("is out of bound"),s,
                QMessageBox::Ok,QMessageBox::Ok);
//            break;
            }
        data.push_back(x);
        data.push_back(EFT1);
        this->plotterU->setCurveData(this->numOfCurve,data);
    }
    this->numOfCurve++;
    return;
    }

    int j=0;
    for(double x=this->Umin; x<=this->Umax; x+=this->dU)
    {
    this->U=x;
    sum=0;
    double Vd=Vdot();
    double aa=this->U*aa1;
//    aa=0;
    this->EF=aa+EF00+Vd-Vd0+this->Cg0*(this->U-Vg0);
    int NE=(this->EF+20*this->T-Vd)/dE;
    this->AreaEf.resize(NE);
    for (int i=0; i< NE; ++i)
    {
        E=dE*(i+1)+Vd;
        Area=AreaE(E)/10000;
        this->AreaEf[i]=Area;
        if(E<=this->EF) sum=sum+Area;
        //       data.push_back(E);
        //       data.push_back(Area);
        //       this->plotterT->setCurveData(this->numOfCurve,data);
    }
    ////////////////
        if(sum==0)
    {
            QString s;
            s.sprintf("dot is empty at  U=%lg\n",x);
            QMessageBox::warning(this, tr("Warning!!!"),s,
                QMessageBox::Ok,QMessageBox::Ok);
             j++;
    }
    else
    {
        if(j!=0) EFT0=EFT1;
        else EFT0=this->EF-1.;
        sum1=computeSum(NE, dE, Vd, EFT0);
    while(sum1>sum)
    {
    EFT0=EFT0-1;
    sum1=computeSum(NE, dE, Vd, EFT0);
    }
        sum10=sum1;
        EFT1=EFT0+1;
        sum11=computeSum(NE, dE, Vd, EFT1);
        while(fabs(sum11-sum)>0.0001*sum)
        {
            EFT2=EFT1-(sum11-sum)*(EFT1-EFT0)/(sum11-sum10);
            sum12=computeSum(NE, dE, Vd, EFT2);
            if(sum12>sum&&sum11<sum||sum12<sum&& sum11>sum)

            {
                sum10=sum11;
                EFT0=EFT1;
            }
                sum11=sum12;
                EFT1=EFT2;
        }
          if(j<this->EFUarray.size())
            {
                this->EFUarray[j]=EFT1;
                j++;
                this->EFT=EFT1;
                this->EFT.updateDisplay();
            }
            else
            {
            QString s;
            s.sprintf("element of array of j=%i\n",j);
            QMessageBox::warning(this, tr("is out of bound"),s,

                                 QMessageBox::Ok,QMessageBox::Ok);
//            break;
            }
        data.push_back(x);
        data.push_back(EFT1);
        this->plotterU->setCurveData(this->numOfCurve,data);
    }
    }
    this->numOfCurve++;
}

/*void MainWindow::computeAreaE()
{
    winPlotT->show();
    winPlotT->raise();
    winPlotT->activateWindow();
    QVector<double> data;
//    QVector<double> AreaEf;

    double E,EFT0,EFT1,EFT2 ;
    double dE=0.1;
    double Ucur=this->U;
    this->U=Vg0;
    double Vd0=Vdot();
    this->U=Ucur;
    double Vd=Vdot();
    this->EF=EF0+Vd-Vd0+this->Cg0*(this->U-Vg0);
    int NE=(this->EF+10*this->Tmax-Vdot())/dE;
    this->AreaEf.resize(NE,0.0);
//    int NE=(this->EF+10*this->Tmax-Vdot())/dE;
    double sum, sum1, Area, sum10, sum11, sum12,x;
//    QVector<double> AreaEf(NE);
//    AreaEf.fill(0.0);
    int NT=1+(this->Tmax-this->Tmin)/this->dT;
    this->EFTarray.resize(NT,0.0);

    sum=0;
    for (int i=0; i< NE; ++i)
    {
        E=dE*(i+1)+Vd;
        Area=AreaE(E)/10000;
        this->AreaEf[i]=Area;
        if(E<=this->EF) sum=sum+Area;
        //       data.push_back(E);
        //       data.push_back(Area);
        //       this->plotterT->setCurveData(this->numOfCurve,data);
    }
    int j=0;
    for(double kT=this->Tmax; kT>=this->Tmin; kT-=this->dT)
    {
        this->T=kT;
        EFT0=this->EF-0.4*kT;
        sum1=computeSum(NE, dE, Vd, EFT0);
        sum10=sum1;
        if(sum10>sum) EFT1=EFT0-0.5;
        else EFT1=(EFT0+this->EF)/2;
            sum11=computeSum(NE, dE, Vd, EFT1);
        while(abs(sum11-sum)>0.01)
        {
            EFT2=EFT1-(sum11-sum)*(EFT1-EFT0)/(sum11-sum10);
            sum12=computeSum(NE, dE, Vd, EFT2);
            if(sum12>sum&&sum11<sum||sum12<sum&& sum11>sum)
            {
                sum10=sum11;
                EFT0=EFT1;
            }
            else if(sum12<sum&&sum10<sum||sum12>sum&& sum10>sum)
            {
              if(sum10>sum) EFT0=EFT0-1;//0.5;
              else EFT0=EFT0+1;//0.5;//(EFT0+this->EF)/2;
              sum10=computeSum(NE, dE, Vd, EFT0);
            }
                sum11=sum12;
                EFT1=EFT2;
        }
        this->EFTarray[j]=EFT1;
        j++;
        this->EFT=EFT1;
        this->EFT.updateDisplay();
        data.push_back(kT);
        data.push_back(EFT1);
        this->plotterT->setCurveData(this->numOfCurve,data);
    }
    this->numOfCurve++;

}
*/
void MainWindow::computeEFT()
{
    double E,EFT0,EFT1,EFT2 ;
    double dE=0.1;
    double Ucur=this->U;
    this->U=Vg0;
    double Vd0=Vdot();
    this->U=Ucur;
    double Vd=Vdot();
//    double aa1=(sqrt(1250.)-350)/this->Delta_r;
    double aa1=(1000*sqrt(1.25)-350)/this->Delta_r;
    aa1=aa1*aa1;
    aa1=4/(1+aa1);
    double aa=aa1*this->U;
//    aa=0; //???
    double EF00=this->EF0;
    this->EF=aa+EF00+Vd-Vd0+this->Cg0*(this->U-Vg0);
    int NE=(int)( (this->EF+40*this->Tmax-Vdot())/dE );
    this->AreaEf.resize(NE);
    double sum, sum1, Area, sum10, sum11, sum12;
    int NT=1 +int((this->Tmax-this->Tmin)/this->dT);
    this->EFTarray.resize(NT);
    sum=0;
    for (int i=0; i< NE; ++i)
    {
        E=dE*(i+1)+Vd;
        Area=AreaE(E)/10000;
        this->AreaEf[i]=Area;
        if(E<=this->EF) sum=sum+Area;
    }
        this->density=sum;
    if(sum<0)
    {
            QString s;
            s.sprintf("dot is empty at  U=%lg\n",Ucur);
            QMessageBox::warning(this, tr("Warning!!!"),s,
                QMessageBox::Ok,QMessageBox::Ok);
            return;
    }
    else
    {
    EFT1=this->EF-1.;
    for(int j=0; j<NT; j++)
    {
        this->T=this->Tmax-this->dT*j;
        EFT0=EFT1;//this->EF-1.;//!!!!!!!!!!!!!!!!!
        sum1=computeSum(NE, dE, Vd, EFT0);
    while(sum1>sum)
    {
    EFT0=EFT0-1;
    sum1=computeSum(NE, dE, Vd, EFT0);
    }
        sum10=sum1;
        EFT1=EFT0+1;
        sum11=computeSum(NE, dE, Vd, EFT1);
        while(fabs(sum11-sum)>0.001*sum)
        {
            EFT2=EFT1-(sum11-sum)*(EFT1-EFT0)/(sum11-sum10);
            sum12=computeSum(NE, dE, Vd, EFT2);
            if(sum12>sum&&sum11<sum||sum12<sum&& sum11>sum)
            {
                sum10=sum11;
                EFT0=EFT1;
            }
                sum11=sum12;
                EFT1=EFT2;
        }
        this->EFTarray[j]=EFT1;
    }
    }
}

/*void MainWindow::computeRT1()
{
    double Gtot, E,dE, Emin,csh,sum,alpha,Ec,Uc,V;
    winPlotT->show();
    winPlotT->raise();
    winPlotT->activateWindow();
    QVector<double> data;
    V=Vbarrier(this->rand);
    Uc=Vdot();
    double kT=this->T;
    int G_type = this->typeCond->currentIndex();
    if(G_type!=4) {
    this->Tmax=kT;
    this->Tmin=kT;
    computeEFT();
    double EFTU=this->EFTarray[0];
    this->EFT=EFTU;
    }
    dE=0.1;//(20.*kT)/600.;
//    dE=(14.*kT)/500.;
    if(dE>=0.6) dE=0.5;
    Ec=this->EFT;
//    if(Ec<Uc) Ec=Uc+dE; //Это неправильно!!!
//    double g0=cohU(Ec, this->Ey, r1, V, Uc);
    double g0=sedlo(Ec, this->Ey, this->Ex, V);
    if(kT==0)
    {
//      if(Ec>Uc)  Gtot=cohU(Ec,this->Ey,r1,V,Uc);
      if(Ec>Uc)  Gtot=sedlo(Ec, this->Ey,this->Ex, V);
    }
    else
    {   Gtot=0;
        double GTunnel=0.;
        double GOver=0.;
    Emin=Ec-10*kT;
//    Emin=Ec-7*kT;
    double sumt=0.;
    double aa=0.5*dE/kT;
        Emin=Uc+dE;
        for(E=Emin; E<=Ec+40*kT; E+=dE){
//        for(E=Emin; E<=Ec+7*kT; E+=dE){
            alpha=0.5*(E-Ec)/kT;
            csh=1./cosh(alpha);
            sum=aa*csh*csh;
            sumt=sumt+sum;
//            double g=cohU(E,this->Ey,r1,V,Uc);
            double g=sedlo(E, this->Ey, this->Ex, V);
            GTunnel+= this->gTun*sum;
            GOver+= this->gOv*sum;
            Gtot+=g*sum;
        data.push_back(E);
 //       data.push_back(sum);
       data.push_back(g*sum);
        this->plotterT->setCurveData(this->numOfCurve,data);
        }
//        Gtot=Gtot/sumt;
//        Gtot=Gtot*exp(-2*6.2832*kT/r1)/sumt;
//        GTunnel=GTunnel/sumt;
//        GOver = GOver/sumt;
    }
    this->numOfCurve++;
}
*/

void MainWindow::computeRT1()
{
    winPlotT->show();
    winPlotT->raise();
    winPlotT->activateWindow();
    QVector<double> data;
    int G_type = this->typeCond->currentIndex();//checkedId();
//    if(G_type!=4&&G_type!=3) computeEFT();
//    if(G_type==0) computeEFT();
    if(G_type!=4)
    {
//        computeEFT(); !!!!!!!!!!!!!
//        double EFTU=this->EFTarray[0];
//        if(EFTU==0) return;
    }
    int j=0;
//    double Ub=Vbarrier(this->rand)+0.5*this->Ey;
    double EF00=this->EF0;
//    for (double x =this->Tmax; x >= this->Tmin; x -= dT)
        for (double x =this->Tmin; x <= this->Tmax; x += dT)
    {
      this->T=x;
      this->EFT=EF00;
//    for (int j = 0; j < this->EFTarray.size(); ++j)
//        this->T=this->Tmax-this->dT*j;
/*        if(G_type==4) this->EFT=EF00;
        else
        {
            if(j<this->EFTarray.size())
            {
                double EFTU=this->EFTarray[j];
                this->EFT=EFTU;
                j++;
            }
            else
            {
            QString s;
            s.sprintf("EFTarray at j=%i\n",j);
            QMessageBox::warning(this, tr("is out of bound"),s,
                QMessageBox::Ok,QMessageBox::Ok);
        }
        }
*/        this->computeOneR();
        double y=model->conductivity;
//        y=6.28*y;
        if(y>1e-18)
        {
        data.push_back(x);
        data.push_back(y);
        this->plotterT->setCurveData(this->numOfCurve,data);
        }
    }
    this->numOfCurve++;
}

void MainWindow::computeRT()
{
    winPlotT->show();
    winPlotT->raise();
    winPlotT->activateWindow();
    QVector<double> data;
    int G_type = this->typeCond->currentIndex();
//    if(G_type==0) computeEFT();
//    if(G_type!=4&&G_type!=3) computeEFT();
    if(G_type!=4)
    {
//        computeEFT();
//        double EFTU=this->EFTarray[0];
//        if(EFTU==0) return;
    }
    int j=0;
    double EF00=this->EF0;
//    this->EFT=EF0; // !!!!!!!!!!!!
    for (double x = this->Tmax; x >= this->Tmin; x -= dT)
//    for (double x = this->Tmin; x <= this->Tmax; x += dT)
    {   this->T=x;
        if(this->flgStop)
        {
            this->flgStop=false;
            return;
        }
//        if(G_type!=0) this->EFT=EF0;
/*        if(G_type==4) this->EFT=EF00;
        else
        {
            if(j<this->EFTarray.size())
            {
                double EFTU=this->EFTarray[j];
                this->EFT=EFTU;
                j++;
            }
            else
            {
            QString s;
            s.sprintf("element of array of j=%i\n",j);
            QMessageBox::warning(this, tr("is out of bound"),s,
                QMessageBox::Ok,QMessageBox::Ok);
            break;
        }
        }*/
        this->computeModel();
        double y=model->conductivity;
        y=(this->cols-3)*y/this->rows;
        y=6.28*y;
        data.push_back(x);
        data.push_back(y);
/*            if(model->conductivity<NCUT*CUTOFF_SIGMA)
            {
                this->numOfCurve++;
                break;
            }*/
            this->plotterT->setCurveData(this->numOfCurve,data);
    }
    this->numOfCurve++;
}

void MainWindow::computeReffT()
{
    winPlotT->show();
    winPlotT->raise();
    winPlotT->activateWindow();
    QVector<double> data;
    int r_type = this->typeResistor->checkedId();
    int G_type = this->typeCond->currentIndex();
    if(G_type!=4)
    {
        computeEFT();
//        double EFTU=this->EFTarray[0];
//        if(EFTU==0) return;
    }
    int j=0;
    double y_old=0;
    double EF00=this->EF0;
    for (double x = this->Tmax; x >= this->Tmin; x -= dT)
    {   this->T=x;
        this->T.updateDisplay();

        if(this->flgStop)
        {
            this->flgStop=false;
            return;
        }
        if(G_type==4) this->EFT=EF00;
        else
        {
            if(j<this->EFTarray.size())
            {
                double EFTU=this->EFTarray[j];
                this->EFT=EFTU;
                j++;
            }
            else
            {
            QString s;
            s.sprintf("element of array of j=%i\n",j);
            QMessageBox::warning(this, tr("is out of bound"),s,
                QMessageBox::Ok,QMessageBox::Ok);
            break;
        }
        }
        this->selectSigma(r_type);
        double y=effective_medium(y_old);
        y_old=y;
        double y1=6.28*y;
        data.push_back(x);
        data.push_back(y1);
        this->plotterT->setCurveData(this->numOfCurve,data);
    }
    this->numOfCurve++;
}
/*void MainWindow::compute_pSigma()
{
    winPlotT->show();
    winPlotT->raise();
    winPlotT->activateWindow();
    QVector<double> data;
    int G_type = this->typeCond->currentIndex();
//    if(G_type==0) computeEFT();
//    if(G_type!=4&&G_type!=3) computeEFT();
    if(G_type!=4)
    {
        computeEFT();
        double EFTU=this->EFTarray[0];
        if(EFTU==0) return;
    }
    int j=0;
    for (double x = this->Tmax; x >= this->Tmin; x -= dT)
//    for (double x = this->Tmin; x <= this->Tmax; x += dT)
    {   this->T=x;
        randomizeSigma_2();
        if(this->flgStop)
        {
            this->flgStop=false;
            return;
        }
//        if(G_type!=0) this->EFT=EF0;
        if(G_type==4) this->EFT=EF0;
        else
        {
        double EFTU=this->EFTarray[j];
        this->EFT=EFTU;
        j++;
        }
        double sum=0;
            for (int i = 0; i < model->nI(); ++i)
    {
        sum=sum+log(model->Sigma[i]);
    }
        sum=sum/model->nI();
        double y=exp(sum);
        data.push_back(x);
        data.push_back(y);
            this->plotterT->setCurveData(this->numOfCurve,data);
    }
    this->numOfCurve++;
}
*/


void MainWindow::myMouseMoveEvent(QMouseEvent *e)
{
    static int processing;
    static QLabel *label;
    if (processing)
    {
        e->ignore();
        return;
    }
    processing = 1;
    QString text = "hi there";
    if (label) delete label;
    label = new QLabel(text);
    label->show();
    processing = 0;
}

double MainWindow::cohU(double E, double Ey, double A, double V, double Uc)
{
  double  kaA,G0,G1,U0,EE;
  double sh2,cos2,BB;
  U0=V-Uc;
  EE=E-0.5*Ey-Uc;
  G0=0;
  this->gTun=0;
  this->gOv=0;
  while(EE>0)
  {
  kaA=A*sqrt(EE/E0);
  sh2=sinh(3.14159*kaA);
  sh2=sh2*sh2;
  BB=0.25-U0*A*A/E0;
  if(BB>=0)  cos2=cos(3.14159*sqrt(BB));
  else cos2=cosh(3.14159*sqrt(-BB));
  cos2=cos2*cos2;
  G1=sh2/(sh2+cos2);
  if((EE-U0)>0)this->gOv+=G1;
  else this->gTun+=G1;
  G0+=G1;
  EE-=Ey;
  }
if(G0>this->CUTOFF_SIGMA)  return G0;
else return this->CUTOFF_SIGMA;
}

double MainWindow::sedlo(double E, double Ey, double Ex, double V)
{ double  alpha,G0,g,exp0,EE, Ep,Uc;
  Uc=Vdot();
  this->gTun=0;
  this->gOv=0;
  G0=0;
  EE=E-0.5*Ey-V;
  Ep=E-0.5*Ey;
  g=0.1;
  while(g>1e-15)
//      while(Ep>Uc)
  {
  alpha=-6.2831853*EE/Ex;
  exp0=exp(alpha);
  g=1./(1+exp0);
  if(g<0.5) this->gTun+=g;
  else this->gOv+=g;
  G0+=g;
  EE-=Ey;
  Ep-=Ey;
  }
  return G0;
//if(G0>CUTOFF_SIGMA)  return G0;
//else return CUTOFF_SIGMA;
}

void MainWindow::potential()
{
  winPlotU->show();
  winPlotU->raise();
  winPlotU->activateWindow();
  QVector<double> data;
  QVector<double> data1;
      int G_type = this->typeCond->currentIndex();//checkedId();
    if(G_type!=4)
    {
        computeEF_TU();
    }
//  computeEF_TU();
  double V=Vbarrier(this->rand);
  double Uc=Vdot();
  double a1=a_barrier;
  double deltaV=10;//11;//15;//20;//17;//8;//10;
//  double Va=V-13;
  double Va=V-0.*deltaV;//7.5;//8.500;//meV//БЫЛО
//  V=V*(1+sqrt(0.06/this->T));
//  V=(V+ .75*deltaV)*(1+sqrt(0.07/this->T)); //БЫЛО
  double a0=10;//13;//12;//15;//было
//  double a0=2./Ex*sqrt(E0*(V-Va));
  Ex=2/a0*sqrt(E0*(V-Va));//было
  double a2=a0/a1;
  double U00=V-Va;
  double U01=Va/(1-a2*a2);
//--------------
//  double Va=V-6.;//7.5;//8.50;//meV
//  V=V*(1+sqrt(0.06/this->T));
//  double a0=14;//13;//12;//15;
//  Ex=2/a0*sqrt(E0*(V-Va));
//  double a2=a0/a1;
//  double U00=V-Va;
//-------------
//  double Ex0=sqrt(2*E0*U01)/a1;
  double dx=2*a1/100;
  double aa=a0*a0;
  double aa1=a1*a1;
  for (double x=-a1; x <= a1; x+=dx)
  {
        data.push_back(x);
        double y=this->EFT;
        data.push_back(y);
        this->plotterU->setCurveData(this->numOfCurve,data);

  }
  this->numOfCurve++;
  for (double x=-a1; x < a1; x+=dx)
  {
      double xx=x*x;
      double Ux=0;
      if(x<=a0&&x>=-a0)
      {
      Ux=U00*(1-xx/aa)+U01;
      }
      else
      {
      Ux=U01*(1-xx/aa1);
      if(Ux<Uc) Ux=Uc;
      }
        data1.push_back(x);
        data1.push_back(Ux+0.5*this->Ey);
        this->plotterU->setCurveData(this->numOfCurve,data1);

  }
  this->numOfCurve++;
  }
/*double MainWindow::sedlo(double E, double Ey, double Ex, double V)
{ double  alpha,G0,g,exp0,EE, Ep,Uc;
  Uc=Vdot();
  double a1=a_barrier;//120;//100;//80;//100;//500;//nm
//  V=V+(V-Va)*0.04/this->T;
// if you change parameter here, change them in subroutine potential()
  double deltaV=10;//11;//15;//20;//17;//8.;
  double Va=V-0.*deltaV;//7.5;//8.500;//meV  было
//  double Va=V-13.00;//meV
//  V=V*(1+sqrt(0.06/this->T));//???
//  V=V*(1+pow((0.06/this->T,0),1./3.));
  //V=V*(1+sqrt(0.13/this->T))+1.0*deltaV;
  V=(V+ .75*deltaV)*(1+sqrt(0.07/this->T));
  double a0=10;//13;//12;//15;//comment 11.05.2016
  Ex=2/a0*sqrt(E0*(V-Va)); // Ex>13
//  double a0=2/Ex*sqrt(E0*(V-Va)); //DE comment 11.05.2016!!!!!!!!!!!!!!
  double a2=a0/a1;
  double U00=V-Va;
  double U01=Va/(1-a2*a2);
  this->gTun=0;
  this->gOv=0;
  G0=0;
  EE=E-0.5*Ey-V;
  Ep=E-0.5*Ey;
  while(Ep>Uc)
  {
       if(Ep>Va)
      {
  alpha=-6.2832*EE/Ex;
  exp0=exp(alpha);
  g=1./(1+exp0);
      }
      else
      {
  double b0=sqrt(U00/(V-Ep));
  double b1=a2*sqrt(U01/(U01-Ep));
  double asinb0=asin(b0);
  double asinb1=asin(b1);
  double b2=1/(b0*b0);
  double Z=-2*a0*sqrt(U00/E0)*(sqrt(b2-1)+asinb0*b2);
  b2=1/(b1*b1);
  Z=Z-2*a0*a2*sqrt(U01/E0)*((3.14159/2-asinb1)*b2-sqrt(b2-1));
  g=exp(Z);
      }
  if(g<0.5) this->gTun+=g;
  else this->gOv+=g;
  G0+=g;
  EE-=Ey;//!!!
  Ep-=Ey;//!!!
  }
  return G0;
//if(G0>CUTOFF_SIGMA)  return G0;
//else return CUTOFF_SIGMA;
}
*/
double MainWindow::Vbarrier(double r)
{
  double BB,rr;
//  const double RMIN=125;//nm
//  const double RMAX=475;//nm
//  const double RMIN=150;//nm
//  const double RMAX=450;//nm
  const double RMIN=190;//nm used for report
  const double RMAX=410;//nm
//  const double RMIN=200;//nm
//  const double RMAX=400;//nm
  rr=0.5*(RMIN+r*(RMAX-RMIN))/this->Delta_r;
  rr=rr*rr;
  BB=(1+rr);
  BB=2./BB;
  double aa1=(1000*sqrt(1.25)-350)/this->Delta_r;
//  double aa1=(sqrt(1250.)-350)/this->Delta_r;
  aa1=aa1*aa1;
  aa1=4/(1+aa1);
//  aa1=0;
  return (aa1+BB)*this->U;
}
double MainWindow::Vdot(void)
{double x1,y1,r1;
            x1=500;
            y1=500;
            r1=sqrt(x1*x1+y1*y1)-350;
            r1=r1/this->Delta_r;
            return 4*this->U/(1+r1*r1);
}

double MainWindow::singleSigma(double r, double rEx)
//double MainWindow::singleSigma(double r)
{
    double Gtot, E,dE, Emin,csh,sum,alpha,Ec,Uc,V;
    V=Vbarrier(r);
    Uc=Vdot();
    double kT=this->T;
    dE=0.1*kT;
//    if(dE>=0.6) dE=0.5;
//    if(kT<=0.2) dE=0.01;
//    if(kT>=3) dE=1.;
    Ec=this->EFT;
//    this->Ex=rEx;
//    double g0=cohU(Ec, this->Ey, r1, V, Uc);
//    double g0=sedlo(Ec, this->Ey, this->Ex, V);
//    double g0=sedlo(Ec, this->Ey, rEx, V);
    if(kT<=0.01)
    {
//      if(Ec>Uc)  Gtot=cohU(Ec,this->Ey,r1,V,Uc);
//      if(Ec>Uc)  Gtot=sedlo(Ec, this->Ey,this->Ex, V);
//      if(Ec>Uc)
          Gtot=sedlo(Ec, this->Ey,rEx, V);
          return Gtot;
//      else Gtot=this->CUTOFF_SIGMA;
    }
    else
    {   Gtot=0;
        double GTunnel=0.;
        double GOver=0.;
        int G_type = this->typeCond->currentIndex();//checkedId();
        Emin=Ec-10*kT;
        double sumt=0.;
    double aa=0.25*dE/kT;
    int jmin= (int)(20*kT/dE);
    int jmax= (int)(20*kT/dE);
//    if(Emin<Uc) Emin=Uc+dE;!!!!!!!!!!!!!!
//        for(E=Emin; E<=Ec+40*kT; E+=dE){
    for(int j=-jmin; j<=jmax; j++){
        E = Ec + j*dE;
//        for(E=Emin; E<=Ec+20*kT; E+=dE){
        alpha=0.5*j*dE/kT;
//        alpha=0.5*(E-Ec)/kT;
            csh=1./cosh(alpha);
            sum=csh*csh;
//            sum=aa*csh*csh;
            sumt=sumt+sum;
//            double g=cohU(E,this->Ey,r1,V,Uc);
            double g=sedlo(E, this->Ey, rEx, V);
            GTunnel+= this->gTun*sum;
            GOver+= this->gOv*sum;
            Gtot+=g*sum;
        }
        if(G_type==1) Gtot=GTunnel;
        if(G_type==2) Gtot=GOver;
        //double eps=0;//0.0075*this->U-1.02;
//        if(G_type==3) Gtot=GOver+GTunnel*exp(-eps/kT);
        Gtot = aa*Gtot;
    }
//    Gtot=Gtot*this->G_ser/(Gtot+this->G_ser);
  if(Gtot<this->CUTOFF_SIGMA) return this->CUTOFF_SIGMA;
//  else
      return Gtot;
}
void MainWindow::randRcr()
{
    this->i_Rcr = model->index_of_Rcr();
    elementCr = fabs((model->I[ i_Rcr ]));
    this->sigmaMin=model->Sigma[i_Rcr ];
//    QVector<double> t( model->nI() );
    VSLStreamStatePtr stream;
    vslNewStream( &stream, VSL_BRNG_MCG31, this->seed );
    vdRngUniform( 0, stream, model->nI(), model->Sigma.data(), 0.0, 1.0 );
    vslDeleteStream( &stream );
    double x1=model->Sigma[i_Rcr];
    this->randc=x1;
    this->rand=x1;
    this->sigmaMin.updateDisplay();
    this->rand.updateDisplay();
}
void MainWindow::randomizeSigma_2()
{   double x1,x2;
//   QVector<double> t( model->nI() );
//     static int iseed;
    VSLStreamStatePtr stream;
//    vslNewStream( &stream, VSL_BRNG_MCG31, 0);
/*    iseed=this->seed;
    vslNewStream( &stream, VSL_BRNG_MCG31, iseed++ );
    this->seed=iseed;
*/
    vslNewStream( &stream, VSL_BRNG_MCG31, this->seed );
    vdRngUniform( 0, stream, model->nI(), model->Sigma.data(), 0.0, 1.0 );
    vslDeleteStream( &stream );
//    VSLStreamStatePtr stream1;
//    vslNewStream( &stream1, VSL_BRNG_MCG31, this->seed+111);
//    vdRngUniform( 0, stream1, t.size(), t.data(), 0.0, 1.0 );
//    vslDeleteStream( &stream1);
    for (int i=0; i < model->nI(); ++i)
    {
        QPair<int,int> ends = model->ends(i);
        int from = ends.first;
        int to   = ends.second;
        QPair<double,double> xy0 = model->xy(from);
        QPair<double,double> xy1 = model->xy(to);

        if (xy0.first==0 && xy1.first==0
            || xy0.first==0 && xy1.first==1
            || xy0.first==1 && xy1.first==0
            || xy0.first==1 && xy1.first==1
            || xy0.first==model->xmax() && xy1.first==model->xmax()
            || xy0.first==model->xmax()-1 && xy1.first==model->xmax()
            || xy0.first==model->xmax()   && xy1.first==model->xmax()-1
            || xy0.first==model->xmax()-1 && xy1.first==model->xmax()-1
            )
        {
            model->Sigma[i]=this->sigmaU;
        }
        else  {x1=model->Sigma[i];
            x2=this->Ex;//+this->deltaEx*(0.5-t[i]);
//            this->Ey=x2;
//            x3=5;//1+8*(1-x1);
//            this->Ex=x3;
            if (x1 < this->r_c) model->Sigma[i] = this->CUTOFF_SIGMA;
            else model->Sigma[i] = singleSigma(x1,x2);
//            else model->Sigma[i] = singleSigma(x1);
//            model->Sigma[i] = singleSigma(x1);
//            if(this->r_c>0&&model->Sigma[i]<sigma_m) model->Sigma[i]=CUTOFF_SIGMA;
        }
    }
}
/*
void MainWindow::randomizeSigma_2()
{   double x1,x2,x3;
   QVector<double> t( model->nI() );
    VSLStreamStatePtr stream;
    vslNewStream( &stream, VSL_BRNG_MCG31, this->seed );
    vdRngUniform( 0, stream, model->nI(), model->Sigma.data(), 0.0, 1.0 );
    vslDeleteStream( &stream );
    VSLStreamStatePtr stream1;
    vslNewStream( &stream1, VSL_BRNG_MCG31, this->seed+111);
    vdRngUniform( 0, stream1, t.size(), t.data(), 0.0, 1.0 );
    vslDeleteStream( &stream1);
    for (int i=0; i < model->nI(); ++i)
    {
        QPair<int,int> ends = model->ends(i);
        int from = ends.first;
        int to   = ends.second;
        QPair<double,double> xy0 = model->xy(from);
        QPair<double,double> xy1 = model->xy(to);

        if (xy0.first==0 && xy1.first==0
            || xy0.first==0 && xy1.first==1
            || xy0.first==1 && xy1.first==0
            || xy0.first==1 && xy1.first==1
            || xy0.first==model->xmax() && xy1.first==model->xmax()
            || xy0.first==model->xmax()-1 && xy1.first==model->xmax()
            || xy0.first==model->xmax()   && xy1.first==model->xmax()-1
            || xy0.first==model->xmax()-1 && xy1.first==model->xmax()-1
            )
        {
            model->Sigma[i]=this->sigmaU;
        }
        else  {x1=model->Sigma[i];
            x2=this->Ex+this->deltaEx*(0.5-t[i]);
//            this->Ey=x2;
//            x3=5;//1+8*(1-x1);
//            this->Ex=x3;
            if (x1 < this->r_c) model->Sigma[i] = CUTOFF_SIGMA;
            else model->Sigma[i] = singleSigma(x1,x2);
//            else model->Sigma[i] = singleSigma(x1);
//            model->Sigma[i] = singleSigma(x1);
//            if(this->r_c>0&&model->Sigma[i]<sigma_m) model->Sigma[i]=CUTOFF_SIGMA;
        }
    }
}*/
void MainWindow::randomizeSigma_1()
{   double x1;
   VSLStreamStatePtr stream;
//    vslNewStream( &stream, VSL_BRNG_MCG31, 0);
/*    int iseed=this->seed;
    vslNewStream( &stream, VSL_BRNG_MCG31, iseed++ );
    this->seed=iseed;
*/
    vslNewStream( &stream, VSL_BRNG_MCG31, this->seed );
    vdRngUniform( 0, stream, model->nI(), model->Sigma.data(), 0.0, 1.0 );
    vslDeleteStream( &stream );
    for (int i=0; i < model->nI(); ++i)
    {
        QPair<int,int> ends = model->ends(i);
        int from = ends.first;
        int to   = ends.second;
        QPair<double,double> xy0 = model->xy(from);
        QPair<double,double> xy1 = model->xy(to);

        if (xy0.first==0 && xy1.first==0
            || xy0.first==0 && xy1.first==1
            || xy0.first==1 && xy1.first==0
            || xy0.first==1 && xy1.first==1
            || xy0.first==model->xmax() && xy1.first==model->xmax()
            || xy0.first==model->xmax()-1 && xy1.first==model->xmax()
            || xy0.first==model->xmax()   && xy1.first==model->xmax()-1
            || xy0.first==model->xmax()-1 && xy1.first==model->xmax()-1
            )
        {
            model->Sigma[i]=this->sigmaU;
        }
        else  {x1=model->Sigma[i];
//        if (x1 < 0.4) model->Sigma[i] = this->CUTOFF_SIGMA;
        if (x1 < this->r_c) model->Sigma[i] = this->CUTOFF_SIGMA;
            else model->Sigma[i] =1;
        }

        }

}
void MainWindow::randomizeSigma_0()
{   double x1;
    VSLStreamStatePtr stream;
    vslNewStream( &stream, VSL_BRNG_MCG31, this->seed );
    vdRngUniform( 0, stream, model->nI(), model->Sigma.data(), 0.0, 1.0 );
    vslDeleteStream( &stream );
    for (int i=0; i < model->nI(); ++i)
    {
        QPair<int,int> ends = model->ends(i);
        int from = ends.first;
        int to   = ends.second;
        QPair<double,double> xy0 = model->xy(from);
        QPair<double,double> xy1 = model->xy(to);

        if (xy0.first==0 && xy1.first==0
            || xy0.first==0 && xy1.first==1
            || xy0.first==1 && xy1.first==0
            || xy0.first==1 && xy1.first==1
            || xy0.first==model->xmax() && xy1.first==model->xmax()
            || xy0.first==model->xmax()-1 && xy1.first==model->xmax()
            || xy0.first==model->xmax()   && xy1.first==model->xmax()-1
            || xy0.first==model->xmax()-1 && xy1.first==model->xmax()-1
            )
        {
            model->Sigma[i]=this->sigmaU;
        }
        else  {x1=model->Sigma[i];
            if (x1 < this->r_c) model->Sigma[i] = this->CUTOFF_SIGMA;
            else model->Sigma[i] = exp(-this->kappa*x1);
            if (model->Sigma[i]<this->CUTOFF_SIGMA) model->Sigma[i] = this->CUTOFF_SIGMA;
        }

        }

}

void MainWindow::selectSigma(int i)
{
    this->setModel();
    switch(i)
    {
    case 0: /* uniform Sigma */
        this->randomizeSigma_0();
        break;
    case 1: /* random Sigma */
        this->randomizeSigma_1();
        break;
    case 2: /* unimplemented */
        this->randomizeSigma_2();
        break;

    default:
        break;
    }
}
/*void MainWindow::setConductivity()
{
    double I1,I2;
    for (int i=0; i < model->nI(); ++i)
    {
        QPair<int,int> ends = model->ends(i);
        int from = ends.first;
        int to   = ends.second;
        QPair<double,double> xy0 = model->xy(from);
        QPair<double,double> xy1 = model->xy(to);
        if(xy0.first==0 && xy1.first==0&&
            (xy0.second==0&&xy1.second==1||xy0.second==1&&xy1.second==0)) I1=model->I[i];
        if(xy0.second==0 && xy1.second==0&&
            (xy0.first==0&&xy1.first==1||xy0.first==1&&xy1.first==0)) I2=model->I[i];

    }
 //   if (fabs(I1+I2)>0) this->conductivity = fabs(I1+I2)/2.;
    this->conductivity = fabs(I1+I2)/2.;
        this->conductivity.updateDisplay();
}
*/
void MainWindow::resizeEvent( QResizeEvent * )
{
    /*
    QSize nsz = e->size();
    QSize osz = e->oldSize();
    Q3Canvas *c = cv->canvas();

    c->resize(
        c->width()+nsz.width()-osz.width(),
        c->height()+nsz.height()-osz.height()
        );
        */
}

