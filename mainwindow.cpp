#include "mainwindow.h"
#include "mkl.h"
#include <math.h>
#include "myparam.h"


const double CUTOFF_SIGMA = 1e-15;
const int NCUT = 1e4;
//const double Delta_r=35;//15; //nm
const double E0=560.;//meV
const double Vg0=50;
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
    connect(chooseFontAction, SIGNAL(activated()), this, SLOT(chooseFont()));
    connect(saveAction, SIGNAL(activated()), this, SLOT(saveAs()));
    connect(openAction, SIGNAL(activated()), this, SLOT(open()));
    

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
            QHBoxLayout *hlLR = new QHBoxLayout;
           QGroupBox *gb = new QGroupBox(tr("Compute Conductance G"));
            vl2->addLayout(hlLR);
            vl2->addWidget(gb);

            /* now fill LR */
            QVBoxLayout *L = new QVBoxLayout(hlLR);
            {
                this->T.setDisplay(("T[meV]"), L);
//                this->Ex.setDisplay(("a[nm]"), L);
                this->Ex.setDisplay(("E_x[meV]"), L);
                this->Ey.setDisplay(("E_y[meV]"), L);
                this->Tmin.setDisplay(("T_min"), L);
                this->Tmax.setDisplay(("T_max"), L);
                this->dT.setDisplay(("dT"), L);
                this->a_barrier.setDisplay(("a_bar"), L);
                this->Delta_r.setDisplay(("Delta_r"), L);
                this->EFT.setDisplay(("EF(T,U)"), L);

                QPushButton *bL  = new QPushButton(tr("E_F for given T,U"));
                connect(bL,SIGNAL(clicked()), this, SLOT(computeEF_TU()));
                L->addWidget(bL);
            }
            QVBoxLayout *R = new QVBoxLayout(hlLR);
            {
                this->U.setDisplay(("U[meV]"), R);
                this->rand.setDisplay(("r[0--1]"), R);
                this->EF0.setDisplay(("EF0"), R);
                this->Umin.setDisplay(("U_min"), R);
                this->Umax.setDisplay(("U_max"), R);
                this->dU.setDisplay(("dU"), R);
                this->Cg0.setDisplay(("Cg"), R);
                this->G_ser.setDisplay(("G_ser"), R);
                QPushButton *computeDensityButton = new QPushButton(tr("density(U)")); 
                connect(computeDensityButton,SIGNAL(clicked()), this, SLOT(compute_nU()));
                R->addWidget(computeDensityButton);

                typeCond = new QComboBox;
                typeCond->addItem(tr("Total conductance"));
                typeCond->addItem(tr("Tunnel conductance"));
                typeCond->addItem(tr("Over-barrier G"));
                typeCond->addItem(tr("Tunnel+Over-barrier G"));
                typeCond->addItem(tr("G(E_F0)"));
                typeCond->setCurrentIndex(3); 
                R->addWidget(typeCond);             
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
            QVBoxLayout *L = new QVBoxLayout(hlLR);
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

 //   this->sigmaMin.setDisplay(("sigmaMin"), L);
    this->kappa.setDisplay(("kappa"), L);
//    this->portion.setDisplay(("portion"), L);
 
            }
            QVBoxLayout *R = new QVBoxLayout(hlLR);
            {
    this->rows.setDisplay(("rows"), R);
    this->cols.setDisplay(("cols"), R);
    this->seed.setDisplay(("seed"), R);
    this->deviation.setDisplay(("deviation"), R);
//    this->sigmaU.setDisplay(("sigmaU"), R);
//    this->r_c.setDisplay(("r_c"), R);
 
    this->fraction.setDisplay(("fraction x"), R);

     QGroupBox *critElem = new QGroupBox(tr("Red Bond"));
     QVBoxLayout *Hcr = new QVBoxLayout(R);
     this->r_c.setDisplay(("r_c"), Hcr);
     this->sigmaMin.setDisplay(("sigmaMin"), Hcr);
     Hcr->addWidget(critElem);
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

                QPushButton *rcB = new QPushButton(tr("Conductivity(rc)")); 
                connect(rcB,SIGNAL(clicked()), this, SLOT(computeRrc()));

                QPushButton *dB = new QPushButton(tr("Deviation")); 
                connect(dB,SIGNAL(clicked()), this, SLOT(compute_deviation()));

                vl1->addWidget(stopB);
                vl1->addWidget(xB);
                vl1->addWidget(uB);
                vl1->addWidget(ueffB);
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

                QPushButton *cpSB = new QPushButton(tr("pSigma")); 
                connect(cpSB,SIGNAL(clicked()), this, SLOT(compute_pSigma()));

                QPushButton *cSpSB = new QPushButton(tr("Sum_pSigma")); 
                connect(cSpSB,SIGNAL(clicked()), this, SLOT(compute_SumpSigma()));
             
                vl2->addWidget(cuB);
                vl2->addWidget(ctB);
                vl2->addWidget(gtB);
                vl2->addWidget(gteffB);
                vl2->addWidget(cpSB);
                vl2->addWidget(cSpSB);
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
void MainWindow::initPlotterU() 
{
    winPlotU = new QDialog(this);
    winPlotU->setCaption(tr("Dependences G(U):"));
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
    winPlotT->setCaption(tr("Dependences G(T):"));
    QVBoxLayout *layout=new QVBoxLayout(winPlotT);
    this->plotterT= new Plotter(winPlotT);
    this->plotterT->setCurveData(1, this->plotdata);
    layout->addWidget(this->plotterT);
}
void MainWindow::initPlotterE() 
{
    winPlotE = new QDialog(this);
    winPlotE->setFocus();
    winPlotE->setCaption(tr("Area(E):"));
    QVBoxLayout *layout=new QVBoxLayout(winPlotE);
    this->plotterE= new Plotter(winPlotE);
    this->plotterE->setCurveData(1, this->plotdata);
    layout->addWidget(this->plotterE);
}
void MainWindow::initPlotterCT() 
{
    winPlotCT = new QDialog(this);
    winPlotCT->setCaption(tr("Capacity(T):"));
    QVBoxLayout *layout=new QVBoxLayout(winPlotCT);
    this->plotterCT= new Plotter(winPlotCT);
    this->plotterCT->setCurveData(1, this->plotdata);
    layout->addWidget(this->plotterCT);
}
void MainWindow::initPlotterCU()

{
    winPlotCU = new QDialog(this);
    winPlotCU->setCaption(tr("Capacity(U):"));
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
MainWindow::MainWindow(QWidget *parent, Qt::WFlags f)
: randc(0.5), 
//Exc(5.), Eyc(5.), 
density(1000.),
sigmaU(1000.0), flgStop(false),portion(0.01),
T(0.13), Tmin(0.1),Tmax(5.301), dT(0.2), //Gold(0.0),
U(165.), Umin(165.), Umax(235), dU(5.), 
//U(950), Umin(200.), Umax(1500), dU(5.), 
r_c(0.0), Ex(15.), Ey(4.), rand(0.5), EF(20),EFT(20.),kappa(10.), 
a_barrier(150.), Cg0(-0.12),Delta_r(30.),G_ser(3.0),EF0(20.),
QMainWindow(parent,f),
rows(30), cols(53), numOfCurve(1), seed(0), model(0)

{
    this->initMenuBar(); 
    this->initToolBar(); 
    this->initStatusBar();
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
//    QFile f(this->curFile);
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
    int i_Rcr = model->index_of_Rcr();
    double sigma_cr=model->Sigma[i_Rcr ];
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
        MKLGetVersionString(buf,sizeof(buf)-1);
        buf[sizeof(buf)-1] = 0;
        about = new QMessageBox("Percolation",buf,
           QMessageBox::Information, 1, 0, 0, this, 0, FALSE );
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
        else if (c == CUTOFF_SIGMA)
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
//    csI.setRange(imin, imax);
//    csI.setColors(Qt::white,Qt::green);
    csI.setColors(Qt::white,Qt::black);
//    csI.setExtraColor(imin+0.05*(imax-imin),QColor("#007f00"));
    EdgeItemFactory ifactory(scene,&csI,scale,offset);

    //setMouseTracking( FALSE );
    randRcr();
    int i_Rcr = model->index_of_Rcr();
    elementCr = fabs((model->I[ i_Rcr ]));
    for (int e = 0; e < model->nI(); ++e)
    {
        QPair<int,int> ends = model->ends(e);
        QPair<double,double> xy0 = model->xy(ends.first);
        QPair<double,double> xy1 = model->xy(ends.second);
        q=fabs((model->I[e]));
//        if(q>0.1*imax){
        QGraphicsLineItem *n = ifactory.newEdge( q , xy0, xy1 );
//        }
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
    Q3MemArray<double> Vij(nI);
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
    for (int e = 0; e < model->nI(); ++e)
    {
        QPair<int,int> ends = model->ends(e);
        QPair<double,double> xy0 = model->xy(ends.first);
        QPair<double,double> xy1 = model->xy(ends.second);
        q=Vij[e];
//        if(q>0.1*Vmax){
        QGraphicsLineItem *n = ifactory.newEdge( q , xy0, xy1 );
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
    Q3MemArray<double> Jij(nI);
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
//        if(q>0.1*Vmax){
        QGraphicsLineItem *n = ifactory.newEdge( q , xy0, xy1 );
//        }
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
        if (fabs(q) < Smin&&(q>10*CUTOFF_SIGMA&&q!=this->sigmaU)) 
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
        QGraphicsLineItem *n = ifactory.newEdge( q , xy0, xy1 );
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
    this->portion.updateDisplay();
    this->fraction.updateDisplay();
    qApp->processEvents();
}
void MainWindow::computeOneR()
{  
//        model->conductivity=singleSigma(this->rand);
        model->conductivity=singleSigma(this->rand);
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
    winPlotCU->setActiveWindow();
    std::vector<double> data;
    double EF00=this->EF0;
    int G_type = this->typeCond->currentIndex();
    if(G_type!=4) computeEFU();
    int j=0;
    for (double x = this->Umin; x <= this->Umax; x += this->dU)
    {   
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
        this->computeModel();
//        this->setCapacity();
        data.push_back(x);
        data.push_back(model->capacity);
            if(model->conductivity<NCUT*CUTOFF_SIGMA) 
            {
                this->numOfCurve++;
                break;
            }

        this->plotterCU->setCurveData(this->numOfCurve,data);
    }
    this->numOfCurve++;
    
}
void MainWindow::computeCapacityT()
{
    winPlotCT->show();
    winPlotCT->raise();
    winPlotCT->setActiveWindow();
    std::vector<double> data;
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
            if(model->conductivity<NCUT*CUTOFF_SIGMA) 
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
    winPlotT->setActiveWindow();
    double xmin, xmax, dx; 
    xmin=0.3;//this->r0;
    xmax=0.5;//this->r0+5./this->T;
    if(xmax>1.) xmax=1.;
    dx=(xmax-xmin)/100;
    std::vector<double> data;
    double EF00=this->EF0;
    this->EFT=EF00;
    for (double x = xmin; x <= xmax; x += dx)
    {
        this->r_c = x;
        if(this->flgStop) {this->flgStop=false; return;}
        this->computeModel();
        data.push_back(this->r_c);
        data.push_back(model->conductivity);
        this->plotterT->setCurveData(this->numOfCurve,data);
    }
    this->numOfCurve++;
}
void MainWindow::computeRU1()
{
    std::vector<double> data;
    winPlotU->show();
    winPlotU->raise();
    winPlotU->setActiveWindow();
    int G_type = this->typeCond->currentIndex();
    double EF00=this->EF0;

        if(G_type!=4) 
    {
        computeEFU();
//        double EFTU=this->EFUarray[0];
//        if(EFTU==0) return;
    }

    int j=0;

    for (double x = this->Umin; x <= this->Umax; x += this->dU)
    {
        this->U = x;
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

        computeOneR();
        data.push_back(x);
        double y=model->conductivity;
        y=6.28*y;
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
    winPlotU->setActiveWindow();
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
    std::vector<double> data;
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
    std::vector<double> data;
    std::vector<double> data1;
    winPlotU->show();
    winPlotU->raise();
    winPlotU->setActiveWindow();
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
    for (double x = 0.4; x < 0.5; x += 0.005)
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
        sigma_m=this->portion*singleSigma(this->randc);//0.01*sigma00*jj;
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
    std::vector<double> data;
//    std::vector<double> data1;
    winPlotU->show();
    winPlotU->raise();
    winPlotU->setActiveWindow();
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
    std::vector<double> data;
    winPlotU->show();
    winPlotU->raise();
    winPlotU->setActiveWindow();
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

void MainWindow::computeReffU()
{
    std::vector<double> data;
    std::vector<double> data1;
    winPlotU->show();
    winPlotU->raise();
    winPlotU->setActiveWindow();
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
        while(fabs(y1-y11)>0.0001*y1)
//        while(fabs(sum11-sum10)>0.01*sum10)
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
void MainWindow::compute_pSigma()
//void MainWindow::compute_plogU()
{
    winPlotU->show();
    winPlotU->raise();
    winPlotU->setActiveWindow();
    std::vector<double> data;
/*    int G_type = this->typeCond->currentIndex();
        if(G_type!=4) 
    {
        computeEF_TU();
    }
//    int r_type = this->typeResist!!!or->checkedId();
//    this->selectSigma(r_type);
//    model->compute();
    computeModel(); */
    double Vmin = 1e300, Vmax = -1e300;
    double q,V_1,V_2,V_i;
    int nv=model->nV();
    V_2=model->W[0];
    int nI=model->nI();
    Q3MemArray<double> Vij(nI);
    for (int i = 0; i < model->nI(); ++i)
    {
        if(fabs(model->Sigma[i])!=this->sigmaU)
        {
	QPair<int,int> ends_i = model->ends(i);
    if(ends_i.first < nv) V_1=model->V[ends_i.first];
    else V_1=model->W[ends_i.first-nv];
    if(ends_i.second < nv) V_2=model->V[ends_i.second];
    else V_2=model->W[ends_i.second-nv];
/*
	V_1 = ends_i.first < nv
	    ? model->V[ends_i.first]
	    : model->W[ends_i.first - nv];
	V_2 = ends_i.second < nv
	    ? model->V[ends_i.second]
	    : model->W[ends_i.second - nv];
 */       
        V_i=fabs((V_1-V_2));
//        V_i=fabs(model->I[i]*(V_1-V_2));
//        if(V_i<1e-20) V_i=1e-20;
        if(V_i<0.1) V_i=0.1;
        V_i=fabs(V_i);
//        V_i=log10(fabs(V_i));
        Vij[i]=V_i;
//	double IV_i = fabs(I_i * V_i);
        double q = V_i;
        if (q > Vmax) Vmax = q; 
        if (q < Vmin) Vmin = q;
        }
    }
    int nS=21;//1001;
    Q3MemArray<double> pS( nS );
//    Q3MemArray<int> line( nS );
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
            this ->plotterU->setCurveData(this->numOfCurve,data);
        }
    this->numOfCurve++;
}
/*
void MainWindow::compute_pSigma()
{
    winPlotU->show();
    winPlotU->raise();
    winPlotU->setActiveWindow();
    std::vector<double> data;
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
    Smin=0;
    Q3MemArray<double> pS( nS );
    pS.fill(0.0);
    double dS=(Smax-Smin)/(nS-1);
    int e;
        for (int i = 0; i < model->nI(); ++i)
        {   
            
            q = (model->Sigma[i]);
//            if(q!=this->sigmaU) 
//            if(q!=this->sigmaU&&q>CUTOFF_SIGMA) 
            if(q!=this->sigmaU) 
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
        int isum1=model->nI()-6*(this->rows)-4;
        for (int i = 0; i < nS; ++i)
        {   
            double x=Smin+i*dS+0.5*dS;
            data.push_back(x);
            double p=pS[i]/isum;
            sum=sum+p;
            data.push_back(p);
//            data.push_back(sum);
            this->plotterU->setCurveData(this->numOfCurve,data);
        }
    this->numOfCurve++;
}
*/
    void MainWindow::compute_SumpSigma()
{
    winPlotU->show();
    winPlotU->raise();
    winPlotU->setActiveWindow();
    std::vector<double> data;
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
    Q3MemArray<double> pS( nS );
    pS.fill(0.0);
    Smin=0;
    double dS=(Smax-Smin)/(nS-1);
    int e;
    int ni=model->nI();
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
            if((x-500)*(x-500)+(y-500)*(y-500)>=122500) 
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

    double aa=(sqrt(1250.)-350)/this->Delta_r;
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
    this->AreaEf.resize(NE,0.0);
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
    this->EFUarray.resize(NU,0.0);
    double Ucur=this->U;//!!!!!!!!!!!
    this->U=Vg0;
    double Vd0=Vdot();
    double aa1=(sqrt(1250.)-350)/this->Delta_r;
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
            
            }
    }
    return;
    }
    int j=0;
    for(double x=this->Umin; x<=this->Umax; x+=this->dU)
    {
        this->U=x;
        double Vd=Vdot();
        double aa=aa1*this->U;
//        aa=0;
        this->EF=aa+EF00+Vd-Vd0+this->Cg0*(this->U-Vg0); 
    int NE=(this->EF+40*this->T-Vd)/dE;
    this->AreaEf.resize(NE,0.0);
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
            s.sprintf("dot is empty at  U=%lg\n",x);
            QMessageBox::warning(this, tr("Warning!!!"),s,
                QMessageBox::Ok,QMessageBox::Ok);
             j++;
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
            break;
            }
    
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
    winPlotU->setActiveWindow();
    std::vector<double> data;
    double E,EFT0,EFT1,EFT2 ;
    double dE=0.1;
    double sum, sum1, Area, sum10, sum11, sum12;
    int NU=1+(this->Umax-this->Umin)/this->dU;
    this->EFUarray.resize(NU,0.0);
    this->U=Vg0;
    double Vd0=Vdot();
    double aa1=(sqrt(1250.)-350)/this->Delta_r;
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
    this->AreaEf.resize(NE,0.0);
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
    winPlotT->setActiveWindow();
    std::vector<double> data;
//    std::vector<double> AreaEf;

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
//    Q3MemArray<double> AreaEf(NE);
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
    double aa1=(sqrt(1250.)-350)/this->Delta_r;
    aa1=aa1*aa1;
    aa1=4/(1+aa1);
    double aa=aa1*this->U;
//    aa=0;
    double EF00=this->EF0;
    this->EF=aa+EF00+Vd-Vd0+this->Cg0*(this->U-Vg0);
    int NE=(int)( (this->EF+40*this->Tmax-Vdot())/dE );
    this->AreaEf.resize(NE,0.0);
    double sum, sum1, Area, sum10, sum11, sum12,x;
    int NT=1 +int((this->Tmax-this->Tmin)/this->dT);
    this->EFTarray.resize(NT,0.0);
    sum=0;
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
            s.sprintf("dot is empty at  U=%lg\n",Ucur);
            QMessageBox::warning(this, tr("Warning!!!"),s,
                QMessageBox::Ok,QMessageBox::Ok);
            return;
    }
    else
    {
//    int j=0;
    EFT1=this->EF-1.;
    for(int j=0; j<NT; j++)
//    for(double kT=this->Tmax; kT>=this->Tmin; kT-=this->dT)
    {
//        this->T=kT;
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
//        j++;
    }
    }
}

/*void MainWindow::computeRT1()
{   
    double Gtot, E,dE, Emin,csh,sum,alpha,Ec,Uc,V;
    winPlotT->show();
    winPlotT->raise();
    winPlotT->setActiveWindow();
    std::vector<double> data;
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
    winPlotT->setActiveWindow();
    std::vector<double> data;
    int G_type = this->typeCond->currentIndex();//checkedId();
//    if(G_type!=4&&G_type!=3) computeEFT();
//    if(G_type==0) computeEFT();
    if(G_type!=4) 
    {
        computeEFT();
//        double EFTU=this->EFTarray[0];
//        if(EFTU==0) return;
    }
    int j=0;
//    double Ub=Vbarrier(this->rand)+0.5*this->Ey;
    double EF00=this->EF0;
    for (double x =this->Tmax; x >= this->Tmin; x -= dT)
    {        
      this->T=x;
//    for (int j = 0; j < this->EFTarray.size(); ++j)
//        this->T=this->Tmax-this->dT*j;
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
        }
        }
        this->computeOneR();
        double y=model->conductivity;
        y=6.28*y;
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
    winPlotT->setActiveWindow();
    std::vector<double> data;
    int G_type = this->typeCond->currentIndex();
//    if(G_type==0) computeEFT();
//    if(G_type!=4&&G_type!=3) computeEFT();
    if(G_type!=4) 
    {
        computeEFT();
//        double EFTU=this->EFTarray[0];
//        if(EFTU==0) return;
    }
    int j=0;
    double EF00=this->EF0;
    for (double x = this->Tmax; x >= this->Tmin; x -= dT)
//    for (double x = this->Tmin; x <= this->Tmax; x += dT)
    {   this->T=x;
        if(this->flgStop) 
        {
            this->flgStop=false; 
            return;
        }
//        if(G_type!=0) this->EFT=EF0;
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
    winPlotT->setActiveWindow();
    std::vector<double> data;
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
    winPlotT->setActiveWindow();
    std::vector<double> data;
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
    label = new QLabel(this,text);
    label->show();
    processing = 0;
}

/*void MainWindow::randomizeSigma_1()
{
//    static int seed;
    VSLStreamStatePtr stream;
    vslNewStream( &stream, VSL_BRNG_MCG31, this->seed );
    vdRngUniform( 0, stream, model->nI(), model->Sigma.data(), 0.0, 1.0 );
    vslDeleteStream( &stream );

    for (int i=0; i < model->nI(); ++i)
           model->Sigma[i] *= - 1./this->T;

    vdExp(model->nI(), model->Sigma.data(), model->Sigma.data());
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
    }
}*/
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
if(G0>CUTOFF_SIGMA)  return G0;
else return CUTOFF_SIGMA;
}

/*double MainWindow::sedlo(double E, double Ey, double Ex, double V)
{ double  alpha,G0,g,exp0,EE, Ep,Uc;
  Uc=Vdot();
  this->gTun=0;
  this->gOv=0;
  G0=0;
  EE=E-0.5*Ey-V;
  Ep=E-0.5*Ey;
  while(Ep>Uc)
  {
  alpha=-6.2832*EE/Ex;
  exp0=exp(alpha);
  g=1./(1+exp0);
  if(g<0.5) this->gTun+=g;
  else this->gOv+=g;
  G0+=g;
  EE-=Ey;
  Ep-=Ey;
  }
if(G0>CUTOFF_SIGMA)  return G0;
else return CUTOFF_SIGMA;
}
*/
void MainWindow::potential()
{ 
  winPlotU->show();
  winPlotU->raise();
  winPlotU->setActiveWindow();
  std::vector<double> data;
  std::vector<double> data1;
  computeEF_TU();
  double V=Vbarrier(this->rand);
  double Uc=Vdot();
  double a1=a_barrier;
    
  //  double a1=100;
  double Va=V-12.50;//meV
  V=V+2*exp(-this->T/0.3);
  double a0=2/this->Ex*sqrt(E0*(V-Va));
  double a2=a0/a1;
  double U00=V-Va;
  double U01=Va/(1-a2*a2);
  double Ex0=sqrt(2*E0*U01)/a1;
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
double MainWindow::sedlo(double E, double Ey, double Ex, double V)
{ double  alpha,G0,g,exp0,EE, Ep,Uc;
  Uc=Vdot();
  double a1=a_barrier;//120;//100;//80;//100;//500;//nm
//  double Va=V-17.5;//meV
//  double Va=V-7.50;//meV
  double Va=V-12.50;//meV
  V=V+2*exp(-this->T/0.6);//T/0.3
  double a0=2/Ex*sqrt(E0*(V-Va));
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
  EE-=Ey;
  Ep-=Ey;
  }
//  if(G0>5) return 5;
  return G0;
//if(G0>CUTOFF_SIGMA)  return G0;
//else return CUTOFF_SIGMA;
}

double MainWindow::Vbarrier(double r)
{
  double AA, BB,rr;
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
  double aa1=(sqrt(1250.)-350)/this->Delta_r;
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

double MainWindow::singleSigma(double r)
{   
    double Gtot, E,dE, Emin,csh,sum,alpha,Ec,Uc,V;
    V=Vbarrier(r);
    Uc=Vdot();
    double kT=this->T;
    dE=0.1;
    if(dE>=0.6) dE=0.5;
    Ec=this->EFT;
//    double g0=cohU(Ec, this->Ey, r1, V, Uc);
    double g0=sedlo(Ec, this->Ey, this->Ex, V);
    if(kT==0) 
    {
//      if(Ec>Uc)  Gtot=cohU(Ec,this->Ey,r1,V,Uc);
      if(Ec>Uc)  Gtot=sedlo(Ec, this->Ey,this->Ex, V);
      else Gtot=CUTOFF_SIGMA;
    }
    else
    {   Gtot=0;
        double GTunnel=0.;
        double GOver=0.;
        int G_type = this->typeCond->currentIndex();//checkedId();
    Emin=Ec-40*kT;
    double sumt=0.;
    double aa=0.25*dE/kT;
    if(Emin<Uc) Emin=Uc+dE;
        for(E=Emin; E<=Ec+40*kT; E+=dE){
//        for(E=Emin; E<=Ec+20*kT; E+=dE){
            alpha=0.5*(E-Ec)/kT;
            csh=1./cosh(alpha);
            sum=aa*csh*csh;
            sumt=sumt+sum; 
//            double g=cohU(E,this->Ey,r1,V,Uc);
            double g=sedlo(E, this->Ey, this->Ex, V);
            GTunnel+= this->gTun*sum;
            GOver+= this->gOv*sum;
            Gtot+=g*sum;
        }
        if(G_type==1) Gtot=GTunnel;
        if(G_type==2) Gtot=GOver;
//        double eps=0.0011*this->U-0.99;
//        double eps=0.0011*this->U-0.88;
        double eps=0.0075*this->U-1.02;
//        double eps=0.0011*this->U-0.66;
//        double eps=0.003*this->U-0.34;
//        double eps=0.0037*this->U-0.48;
        if(G_type==3) Gtot=GOver+GTunnel*exp(-eps/kT);
//    if(G_type==3) Gtot=Gtot-g0*(1-exp(-eps/kT));
//    if(G_type==3) Gtot=Gtot-g0*(1-exp(-eps/kT));
//    if(GOver!=0) {this->Gold=Gtot*exp(eps/kT);}
//    if(G_type==3&&GOver==0) Gtot=this->Gold*exp(-eps/kT);
//    if(G_type==3) Gtot=-(-g0+g0*exp(-eps/kT));
//    if(G_type==3) Gtot=GOver+GTunnel;165
    }
    Gtot=Gtot*this->G_ser/(Gtot+this->G_ser);
  if(Gtot<CUTOFF_SIGMA) return CUTOFF_SIGMA;
//  if(Gtot>5) return 5;
  else 
      return Gtot;
}
void MainWindow::randRcr()
{   
    int i_Rcr = model->index_of_Rcr();
    elementCr = fabs((model->I[ i_Rcr ]));
    this->sigmaMin=model->Sigma[i_Rcr ];
//    Q3MemArray<double> t( model->nI() );
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
{   double x1,x2,x3;
   Q3MemArray<double> t( model->nI() );
//    static int seed;
    VSLStreamStatePtr stream;
//    vslNewStream( &stream, VSL_BRNG_MCG31, 0);
/*    int iseed=this->seed;
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
//            x2=5;//4+2*t[i];
//            this->Ey=x2;
//            x3=5;//1+8*(1-x1);
//            this->Ex=x3;
            if (x1 < this->r_c) model->Sigma[i] = CUTOFF_SIGMA;
            else model->Sigma[i] = singleSigma(x1);
//            model->Sigma[i] = singleSigma(x1);
//            if(this->r_c>0&&model->Sigma[i]<sigma_m) model->Sigma[i]=CUTOFF_SIGMA;   
        }     
    }
}
void MainWindow::randomizeSigma_1()
{   double x1,x2,x3;
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
        if (x1 < this->r_c) model->Sigma[i] = CUTOFF_SIGMA;
            else model->Sigma[i] =1;
        }
 
        }     
    
}
void MainWindow::randomizeSigma_0()
{   double x1,x2,x3;
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
            if (x1 < this->r_c) model->Sigma[i] = CUTOFF_SIGMA;
            else model->Sigma[i] = exp(-kappa*(1-x1));
            if (model->Sigma[i]<CUTOFF_SIGMA) model->Sigma[i] = CUTOFF_SIGMA;
        }
 
        }     
    
}

void MainWindow::selectSigma(int i)
{
    this->setModel();
    switch(i)
    {
    case 0: 
        this->randomizeSigma_0();
        break;
        /* uniform Sigma */
/*        {
//            double x3=this->Exc; 
//            double x2=this->Eyc;
            double x1=this->randc;
//            this->Ey=x2;
//            this->Ex=x3;
            this->rand=x1; 
            this->Ex.updateDisplay();
            this->Ey.updateDisplay();
            this->rand.updateDisplay();
        for (int i = 0; i < model->nI(); ++i) 
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
        else 
        {
            model->Sigma[i] = singleSigma(this->rand);
        }
     }
        }
        break;
*/
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
void MainWindow::resizeEvent( QResizeEvent *e )
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

