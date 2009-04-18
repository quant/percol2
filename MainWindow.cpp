#include "mainwindow.h"
#include "mkl.h"
#include <math.h>
#include "myparam.h"


const double CUTOFF_SIGMA = 1e-10;
const int NCUT = 1e4;
const double EF=15;//20.;//8.;//meV
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
//    QMenuBar* menu = menuBar();
    exitAction = new QAction("Exit", this);
    saveAction = new QAction("Save As", this);
    chooseFontAction = new QAction("Choose Font", this);
    saveAction->setStatusTip(tr("Сохранить в файле напряжения, токи и параметры расчета"));
    saveAction->setShortcut(tr("Ctrl+S"));
    exitAction->setShortcut(tr("Ctrl+Q"));
    chooseFontAction->setShortcut(tr("Ctrl+F"));
    connect(exitAction, SIGNAL(triggered()), this, SLOT(close()));
    connect(chooseFontAction, SIGNAL(clicked()), this, SLOT(chooseFont()));
    connect(saveAction, SIGNAL(activated()), this, SLOT(saveAs()));
    

    QMenu* file = menuBar()->addMenu("Menu"); 
    file->addAction(saveAction);  
    file->addAction(chooseFontAction);  
    file->addSeparator();
    file->addAction(exitAction);  

/*    menu->insertSeparator();

    Q3PopupMenu* help = new Q3PopupMenu( menu );
    help->insertItem("&About", this, SLOT(help()), Qt::Key_F1);
    menu->insertItem(tr("Помощь"),help);
    */
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
/*      QDockWidget *myDockWidget = new QDockWidget(tr("Control Panel"),this);
      myDockWidget->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea); 
      addDockWidget(Qt::RightDockWidgetArea, myDockWidget);
     QWidget *dockWidgetContents = new QWidget();
    QHBoxLayout *horizontalLayout_3 = new QHBoxLayout(dockWidgetContents);
    QGroupBox *groupBox_2 = new QGroupBox(dockWidgetContents);
    QVBoxLayout *verticalLayout_4 = new QVBoxLayout(groupBox_2);
    QGroupBox *groupBox_4 = new QGroupBox(groupBox_2);
    verticalLayout_4->addWidget(groupBox_4);
    QGroupBox *groupBox_5 = new QGroupBox(groupBox_2);
    verticalLayout_4->addWidget(groupBox_5);
    horizontalLayout_3->addWidget(groupBox_2);
    QGroupBox *groupBox = new QGroupBox(dockWidgetContents);
    QVBoxLayout *verticalLayout_3 = new QVBoxLayout(groupBox);
    QHBoxLayout *horizontalLayout = new QHBoxLayout();
    QVBoxLayout *verticalLayout_2 = new QVBoxLayout();
    QPushButton *pushButton_5 = new QPushButton(groupBox);
    verticalLayout_2->addWidget(pushButton_5);
    QPushButton *pushButton_6 = new QPushButton(groupBox);
    verticalLayout_2->addWidget(pushButton_6);
    horizontalLayout->addLayout(verticalLayout_2);
    QVBoxLayout *verticalLayout = new QVBoxLayout();
    QPushButton *pushButton_3 = new QPushButton(groupBox);
    verticalLayout->addWidget(pushButton_3);
    QPushButton *pushButton_4 = new QPushButton(groupBox);
    verticalLayout->addWidget(pushButton_4);
    horizontalLayout->addLayout(verticalLayout);
    verticalLayout_3->addLayout(horizontalLayout);
    QGroupBox *groupBox_3 = new QGroupBox(groupBox);
    QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    sizePolicy.setHeightForWidth(groupBox_3->sizePolicy().hasHeightForWidth());
    groupBox_3->setSizePolicy(sizePolicy);
    QHBoxLayout *horizontalLayout_2 = new QHBoxLayout(groupBox_3);
    QPushButton *pushButton = new QPushButton(groupBox_3);
    horizontalLayout_2->addWidget(pushButton);
    QPushButton *pushButton_2 = new QPushButton(groupBox_3);
    horizontalLayout_2->addWidget(pushButton_2);
    verticalLayout_3->addWidget(groupBox_3);
    horizontalLayout_3->addWidget(groupBox);
    myDockWidget->setWidget(dockWidgetContents);
*/
    //-----------
   
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
                this->T.setDisplay(("T(meV)"), L);
                this->Ex.setDisplay(("E_x(meV)"), L);
                this->Tmin.setDisplay(("T_min"), L);
                this->Tmax.setDisplay(("T_max"), L);
                this->dT.setDisplay(("dT"), L);
            }
            QVBoxLayout *R = new QVBoxLayout(hlLR);
            {
                this->U.setDisplay(("U(meV)"), R);
                this->rand.setDisplay(("r[0--1]"), R);
                this->Umin.setDisplay(("U_min"), R);
                this->Umax.setDisplay(("U_max"), R);
                this->dU.setDisplay(("dU"), R);
            }
            
            /* now fill gb/Compute Conductance G */
            QHBoxLayout *hl = new QHBoxLayout(gb);
            {
                QPushButton *b1  = new QPushButton(tr("G(T)"));
                connect(b1,SIGNAL(clicked()), this, SLOT(computeRT1()));

                QPushButton *b2 = new QPushButton(tr("G(U)"));
                connect(b2,SIGNAL(clicked()), this, SLOT(computeRU1()));

                hl->addWidget(b1);
                hl->addWidget(b2);             
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

                QRadioButton *type1 = new QRadioButton(tr("Одинаковые сопротивления"));  
                QRadioButton *type2 = new QRadioButton(tr("Случайные с exp разбросом"));  
                QRadioButton *type3 = new QRadioButton(tr("Случ. одномерные сужения"));
                
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

//    this->capacity.setDisplay(("C="), L);
    this->sigmaMin.setDisplay(("sigmaMin"), L);
 
            }
            QVBoxLayout *R = new QVBoxLayout(hlLR);
            {
    this->rows.setDisplay(("rows"), R);
    this->cols.setDisplay(("cols"), R);
    this->seed.setDisplay(("seed"), R);
    this->sigmaU.setDisplay(("sigmaU"), R);
    this->r_c.setDisplay(("r_c"), R);

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

                QPushButton *drawHeatButton = new QPushButton(tr("Draw Heat I*V")); 
                connect(drawHeatButton,SIGNAL(clicked()), this, SLOT(drawModelA()));
                
 
                vl->addWidget(computeButton);
                vl->addWidget(drawResButton);
                vl->addWidget(drawCurButton);             
                vl->addWidget(drawHeatButton);
                vl->addStretch(1);

            }            
//           QVBoxLayout *vl1 = new QVBoxLayout(hl3);
           QVBoxLayout *vl1 = new QVBoxLayout(gb4);
            {
                QPushButton *stopB = new QPushButton(tr("STOP")); 
                connect(stopB, SIGNAL(clicked()), this, SLOT(stopCalc()));
 
                QPushButton *uB = new QPushButton(tr("Conductivity(U)")); 
                connect(uB,SIGNAL(clicked()), this, SLOT(computeRU()));

                QPushButton *tB = new QPushButton(tr("Conductivity(T)")); 
                connect(tB,SIGNAL(clicked()), this, SLOT(computeRT()));

                QPushButton *rcB = new QPushButton(tr("Conductivity(rc)")); 
                connect(rcB,SIGNAL(clicked()), this, SLOT(computeRrc()));

                vl1->addWidget(stopB);
                vl1->addWidget(uB);
                vl1->addWidget(tB);
                vl1->addWidget(rcB);
                vl1->addStretch(1);
 
            }
//           QVBoxLayout *vl2 = new QVBoxLayout(hl3);
          QVBoxLayout *vl2 = new QVBoxLayout(gb5);
            {
                QPushButton *cuB = new QPushButton(tr("Capacity(U)")); 
                connect(cuB,SIGNAL(clicked()), this, SLOT(computeCapacityU()));

                QPushButton *ctB = new QPushButton(tr("Capacity(T)")); 
                connect(ctB,SIGNAL(clicked()), this, SLOT(computeCapacityT()));

                QPushButton *cpSB = new QPushButton(tr("pSigma")); 
                connect(cpSB,SIGNAL(clicked()), this, SLOT(compute_pSigma()));

                QPushButton *cSpSB = new QPushButton(tr("Sum_pSigma")); 
                connect(cSpSB,SIGNAL(clicked()), this, SLOT(compute_SumpSigma()));
             
                vl2->addWidget(cuB);
                vl2->addWidget(ctB);
                vl2->addWidget(cpSB);
                vl2->addWidget(cSpSB);
                vl2->addStretch(1);
 
            }

 }        

    }
    vl0->addStretch(1);
    myDockWidget->setWidget(control);
 
   


 


/*
//    QButtonGroup *pt1Buttons =new QButtonGroup(2, Qt::Horizontal, "Compute Conductance G", Rb);

//    QPushButton *TButton1  = new QPushButton(tr("G(T)"), pt1Buttons); 
    QPushButton *tBtn1  = new QPushButton(tr("G(T)"), rb); 
//    TButton1->setMinimumWidth( TButton1->sizeHint().width());
    connect(tBtn1,SIGNAL(clicked()), this, SLOT(computeRT1()));
    gr1->addWidget(tBtn1, 1, 0);

//    QPushButton *uButton1 = new QPushButton(tr("G(U)"), pt1Buttons);
    QPushButton *uBtn1 = new QPushButton(tr("G(U)"), rb);
//    uButton1->setMinimumWidth( uButton1->sizeHint().width());
    connect(uBtn1,SIGNAL(clicked()), this, SLOT(computeRU1()));
    gr1->addWidget(uBtn1, 1, 1);
*/
/*
    //------------------------------
    Q3GroupBox *networkBox =new Q3GroupBox(8, Qt::Vertical, "Resistor Network", control);
    Q3Grid *gr2=new Q3Grid(2,Qt::Horizontal, networkBox);
    gr2->setMargin(3);
    gr2->setSpacing(10);

    this->typeResistor = new Q3ButtonGroup(3, Qt::Vertical, "Resistor Network Type", gr2);
    new QRadioButton(tr("Одинаковые сопротивления"),typeResistor);  
    new QRadioButton(tr("Случайные с exp разбросом"),typeResistor);  
    QRadioButton *type3 = new QRadioButton(tr("Случ. одномерные сужения"),typeResistor);
    type3->setChecked(true);
    connect(typeResistor,SIGNAL(clicked(int)),this,SLOT(selectSigma(int)));
    Q3VBox *vb=new Q3VBox(gr2);
    this->rows.setDisplay(("rows"), vb);
    this->cols.setDisplay(("cols"), vb);
    this->seed.setDisplay(("seed"), vb);
    Q3Grid *gr3=new Q3Grid(2,Qt::Horizontal, networkBox);
    gr3->setMargin(3);
    gr3->setSpacing(10);
    this->sigmaU.setDisplay(("sigmaU"), gr3);
    this->r_c.setDisplay(("r_c"), gr3);

    QString s;
    s.sprintf("G_net %lg",model ? model->conductivity : 0);
    this->dispConduct->setText(s);
    this->capacity.setDisplay(("C="), gr3);

    s.sprintf("error I: %lg",model ? model->deltaI : 0);
    this->dispDeltaI->setText(s);
    this->sigmaMin.setDisplay(("sigmaMin"), gr3);

//    rowsEdit->setValidator(&thePosIntValidator);
 
    Q3ButtonGroup *p2Buttons =new Q3ButtonGroup(2, Qt::Horizontal, "Compute Model", networkBox);

    QPushButton *computeButton = new QPushButton(tr("Compute"), p2Buttons); 
    computeButton->setDefault(true);
    connect(computeButton, SIGNAL(clicked()), this, SLOT(computeModel()));

    QPushButton *stopButton = new QPushButton(tr("STOP"), p2Buttons); 
    connect(stopButton, SIGNAL(clicked()), this, SLOT(stopCalc()));

//    QComboBox *combo = this->comboSigmaType = new QComboBox(0,control);
//    combo->insertItem(tr("Сеть одинаковых сопротивлений"));
//    combo->insertItem(tr("Сеть случайных сопротивлений"));
//    combo->insertItem(tr("Сеть одномодовых сужений"));
//    combo->setCurrentItem(2);
//    connect(combo,SIGNAL(activated(int)),this,SLOT(selectSigma(int)));

    Q3Grid *gr4=new Q3Grid(2,Qt::Horizontal, networkBox);
//-----------
    Q3ButtonGroup *cvButtons =new Q3ButtonGroup(3, Qt::Vertical, "Canvas Images", gr4);

    QPushButton *drawResButton = new QPushButton(tr("Conductivity network"), cvButtons); 
    connect(drawResButton,SIGNAL(clicked()), this, SLOT(drawModelR()));

    QPushButton *drawCurButton = new QPushButton(tr("Draw Current I"), cvButtons); 
    connect(drawCurButton,SIGNAL(clicked()), this, SLOT(drawModelI()));

    QPushButton *drawHeatButton = new QPushButton(tr("Draw Heat I*V"), cvButtons); 
    connect(drawHeatButton,SIGNAL(clicked()), this, SLOT(drawModelA()));
//-----------     
    Q3ButtonGroup *ptButtons =new Q3ButtonGroup(3, Qt::Vertical, "Plotter Images", gr4);

    QPushButton *r0Button = new QPushButton(tr("Conductivity(U)"), ptButtons); 
    connect(r0Button,SIGNAL(clicked()), this, SLOT(computeRU()));

    QPushButton *TButton = new QPushButton(tr("Conductivity(T)"), ptButtons); 
    connect(TButton,SIGNAL(clicked()), this, SLOT(computeRT()));

    QPushButton *CUButton = new QPushButton(tr("Capacity(U)"), ptButtons); 
    connect(CUButton,SIGNAL(clicked()), this, SLOT(computeCapacityU()));

    QPushButton *CTButton = new QPushButton(tr("Capacity(T)"), ptButtons); 
    connect(CTButton,SIGNAL(clicked()), this, SLOT(computeCapacityT()));

    QPushButton *rcButton = new QPushButton(tr("Conductivity(rc)"), ptButtons); 
    connect(rcButton,SIGNAL(clicked()), this, SLOT(computeRrc()));

//    this->moveDockWindow(control,Qt::Right);
//    setDockEnabled(Qt::DockLeft, false);  
//    setDockEnabled(Qt::DockBottom, false);  
//    setDockEnabled(Qt::DockTop, false); 
//    control->setCloseMode(Q3DockWindow::Undocked);
//    control->setMinimumWidth( control->sizeHint().width());
*/
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
    winPlotT->setCaption(tr("Dependences G(T):"));
    QVBoxLayout *layout=new QVBoxLayout(winPlotT);
    this->plotterT= new Plotter(winPlotT);
    this->plotterT->setCurveData(1, this->plotdata);
    layout->addWidget(this->plotterT);
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
//    model = new PercolRect(this->rows,this->cols);
}
//--------------------------------------------------------------------------------------------
//MainWindow::MainWindow(QWidget *parent, const char *name, Qt::WFlags f)
MainWindow::MainWindow(QWidget *parent, Qt::WFlags f)
: rand(0.5), sigmaU(1000.0), flgStop(false),
T(0.5), Tmin(0.1),Tmax(5.2), dT(0.1), 
U(400), Umin(200.), Umax(700), dU(25.), 
r_c(0.0), Ex(0.5), QMainWindow(parent,f),
//Q3MainWindow(parent,name,f),
rows(30), cols(50), numOfCurve(1), seed(0), model(0)

{
    this->initMenuBar(); 
    this->initStatusBar();
    this->initPlotterT();
    this->initPlotterU();
    this->initPlotterCT();
    this->initPlotterCU();
    this->initControlDockWindow();
    this->setModel();
    this->initGraphicsView();
    init();
}


bool MainWindow::saveAs()
{
     QString fn = QFileDialog::getSaveFileName( this, 
                         tr("Choose a file name"), ".",
                         tr("*.dat*"));
//         QString::null, QString::null);
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

        o << "x y Voltage:\n";
/*        o << "Fixed voltage nodes:\n";
    for (int v = 0; v < model->nV(); ++v)
    {
        QString s;
        QPair<double,double> xy = model->xy(v);
        s.sprintf("%lg %lg %lg\n",xy.first,xy.second,model->V[v]);
//        s.sprintf("%i %lg %lg %lg\n",v,xy.first,xy.second,model->V[v]);
        o << s;
    }
*/
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
        double x=0.5*(xy0.first+xy1.first);  
        double y=0.5*(xy0.second+xy1.second);  
        QString s;
        QPair<int,int> from_to = model->ends(e);
//        s.sprintf("%lg %lg %lg\n",x,y, (model->I[e]));
        s.sprintf("%lg %lg %lg\n",x,y, fabs(model->I[e]));
//        s.sprintf("%lg %lg %lg\n",x,y,(model->I[e])/(model->Sigma[e]));
//        s.sprintf("%lg %lg %lg\n",x,y,(model->I[e])*(model->I[e])/(model->Sigma[e]));
//        s.sprintf("%i %i---%i %lg %lg %lg\n",e,from_to.first,from_to.second,
//            model->Sigma[e], model->I[e], (model->I[e])/(model->Sigma[e]));
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
    {
        int canvasx0 = (xy0.first - offset.first) * scale;
        int canvasy0 = (xy0.second - offset.second) * scale;
        int canvasx1 = (xy1.first - offset.first) * scale;
        int canvasy1 = (xy1.second - offset.second) * scale;
        QPen pen;
        if (c == 0)
            pen = QPen(Qt::white);
        else if (c == CUTOFF_SIGMA)
            pen = QPen(Qt::blue);
        else
            pen = QPen(cs->color(c),2);
//          if(c==sigmaMin) n->setPen(QPen(Qt::red)); 
//        n->setPoints(canvasx0,canvasy0,canvasx1,canvasy1);
        QGraphicsLineItem *n = canvas->addLine(canvasx0,canvasy0,canvasx1,canvasy1,pen);
        return n;
    }
};


void MainWindow::drawModelA()
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
            s.sprintf("Got %lg at w=%i\n",woltage,w);
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
    int emax;
    double Jmin = 1e300, Jmax = -1e300;
    double q;
    for (int i = 0; i < model->nI(); ++i)
    {   
        q=(model->I[i])*(model->I[i])/(model->Sigma[i]);
        if (fabs(q) > Jmax) 
        {
            Jmax = fabs(q); 
            emax=i;
        }
        if (fabs(q) < Jmin) Jmin = fabs(q);

    }
        this->sigmaMin=model->Sigma[emax];
        this->sigmaMin.updateDisplay();
      csJ.setRange(Jmin, Jmax);
      csJ.setColors(Qt::white,Qt::black);

    EdgeItemFactory ifactory(scene,&csJ,scale,offset);

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

    for (int e = 0; e < model->nI(); ++e)
    {
        QPair<int,int> ends = model->ends(e);
        QPair<double,double> xy0 = model->xy(ends.first);
        QPair<double,double> xy1 = model->xy(ends.second);
        q=(model->I[e])*(model->I[e])/(model->Sigma[e]);
        if(q<0.00000000001) q=0.;
        QGraphicsLineItem *n = ifactory.newEdge( q , xy0, xy1 );
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
    scene->clear();

    // Set view port
    const double factor = 0.95;
    double xscale = factor * scene->width()  / (model->xmax() - model->xmin());
    double yscale = factor * scene->height() / (model->ymax() - model->ymin());
    double scale = xscale < yscale ? xscale : yscale;
    
    QPair<double,double> offset;
    offset.first = -0.5 * (model->xmax() - model->xmin()) * (1.0 - factor) / factor;
    offset.second = -0.5 * (model->ymax() - model->ymin()) * (1.0 - factor) / factor;
#if 0

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
            s.sprintf("Got %lg at w=%i\n",woltage,w);
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

    for (int v = 0; v < model->nV(); ++v)
    {
        double V = model->V[v];
        QPair<double,double> xy = model->xy(v);
        QGraphicsEllipseItem *n = vfactory.newVnode(V, xy);
        n->setZValue(128);
        n->show();
    }

    int v = model->nV();
    for (int w = 0; w < model->nW(); ++w)
    {
        double W = model->W[w];
        QPair<double,double> xy = model->xy(v+w);
        QGraphicsEllipseItem  *n = vfactory.newWnode(W, xy);
        n->setZValue(128);
        n->show();
    }

#endif

    int emax;
    double imin = 1e300, imax = -1e300;
    double q;
    for (int i = 0; i < model->nI(); ++i)
    {   
        q = (model->I[i]);
        if (fabs(q) > imax) 
        {
            imax = fabs(q); 
            emax=i;
        }
        if (fabs(q) < imin) 
            imin = fabs(q);
    }

    csI.setRange(imin, imax);
//    csI.setColors(Qt::white,Qt::green);
    csI.setColors(Qt::white,Qt::black);
//    csI.setExtraColor(imin+0.05*(imax-imin),QColor("#007f00"));
    EdgeItemFactory ifactory(scene,&csI,scale,offset);

    //setMouseTracking( FALSE );
    for (int e = 0; e < model->nI(); ++e)
    {
        QPair<int,int> ends = model->ends(e);
        QPair<double,double> xy0 = model->xy(ends.first);
        QPair<double,double> xy1 = model->xy(ends.second);
        q=fabs((model->I[e]));
        QGraphicsLineItem *n = ifactory.newEdge( q , xy0, xy1 );
        //n->show();
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
void MainWindow::drawModelR()
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
#if 0

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
            s.sprintf("Got %lg at w=%i\n",woltage,w);
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

    for (int v = 0; v < model->nV(); ++v)
    {
        double V = model->V[v];
        QPair<double,double> xy = model->xy(v);
        QGraphicsEllipseItem *n = vfactory.newVnode(V, xy);
        n->setZValue(128);
        n->show();
    }

    int v = model->nV();
    for (int w = 0; w < model->nW(); ++w)
    {
        double W = model->W[w];
        QPair<double,double> xy = model->xy(v+w);
        QGraphicsEllipseItem  *n = vfactory.newWnode(W, xy);
        n->setZValue(128);
        n->show();
    }

#endif

    double Smin = 1e300, Smax = -1e300;
    double Smin1 = 1e300;
    double q;
    for (int i = 0; i < model->nI(); ++i)
    {   
        q = (model->Sigma[i]);
        if (fabs(q) > Smax) Smax = fabs(q); 
        if (q < Smin1 && q>CUTOFF_SIGMA) Smin1 = q;
        if (fabs(q) < Smin) 
            Smin = fabs(q);
    }
        this->sigmaMin=Smin1; 
        this->sigmaMin.updateDisplay();

    csS.setRange(0, Smax);
    csS.setColors(Qt::white,Qt::black);
    csS.setExtraColor(0.01*Smax,QColor("#007f00"));
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
//    setMouseTracking( TRUE );
    scene->update();
    this->gv->update();

//-----
#if 0
    Q3Canvas *pCanvas = cv->canvas();

    // Set view port
    const double factor = 0.95;
    double xscale = factor * pCanvas->width()  / (model->xmax() - model->xmin());
    double yscale = factor * pCanvas->height() / (model->ymax() - model->ymin());
    double scale = xscale < yscale ? xscale : yscale;
    
    QPair<double,double> offset;
    offset.first = -0.5 * (model->xmax() - model->xmin()) * (1.0 - factor) / factor;
    offset.second = -0.5 * (model->ymax() - model->ymin()) * (1.0 - factor) / factor;

    double vmin = 1e300, vmax = -1e300;
    for (int v = 0; v < model->nV(); ++v)
    {
        if (model->V[v] > vmax) vmax = model->V[v];
        if (model->V[v] < vmin) vmin = model->V[v];
    }
    for (int w = 0; w < model->nW(); ++w)
    {
        if (model->W[w] > vmax) vmax = model->W[w];
        if (model->W[w] < vmin) vmin = model->W[w];
    }
    csV.setRange(vmin,vmax);
    csV.setColors(Qt::red,Qt::blue);

    NodeItemFactory vfactory(pCanvas,&csV,scale,offset);

    double rmin = 1e300, rmax = -1e300;
    double rmin1 = 1e300;
    for (int i = 0; i < model->nI(); ++i)
    {
//        double s = model->Sigma[i];
        if (model->Sigma[i] > rmax) rmax = model->Sigma[i];
        if (model->Sigma[i] < rmin1 && model->Sigma[i]>CUTOFF_SIGMA) rmin1 = model->Sigma[i];
            
        if (model->Sigma[i] < rmin) rmin = model->Sigma[i];
    }
        this->sigmaMin=rmax;//rmin1; 
        this->sigmaMin.updateDisplay();
            csI.setRange(0.0, rmax);
//    csI.setColors(Qt::blue,Qt::green);
    csI.setColors(Qt::white,Qt::black);
    csI.setExtraColor(0.01*rmax,QColor("#007f00"));

    EdgeItemFactory ifactory(pCanvas,&csI,scale,offset);

    for (int v = 0; v < model->nV(); ++v)
    {
        double V = model->V[v];
        QPair<double,double> xy = model->xy(v);
        Q3CanvasEllipse *n = vfactory.newVnode(V, xy);
        n->show();
    }

    int v = model->nV();
    for (int w = 0; w < model->nW(); ++w)
    {
        double W = model->W[w];
        QPair<double,double> xy = model->xy(v+w);
        Q3CanvasEllipse *n = vfactory.newWnode(W, xy);
        n->show();
    }

    for (int e = 0; e < model->nI(); ++e)
    {
        QPair<int,int> ends = model->ends(e);
        QPair<double,double> xy0 = model->xy(ends.first);
        QPair<double,double> xy1 = model->xy(ends.second);
        Q3CanvasLine *n = ifactory.newEdge( model->Sigma[e], xy0, xy1 );
        n->show();
     }
    setMouseTracking( TRUE );
#endif
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
//    this->conductivity.updateDisplay();
//    this->capacity.updateDisplay();
    qApp->processEvents();
}
void MainWindow::computeOneR()
{  
        model->conductivity=singleSigma(this->rand, this->Ex);
        this->U.updateDisplay();
        this->T.updateDisplay();
//        model->conductivity.updateDisplay();
        QApplication::processEvents();
}
void MainWindow::computeCapacityU()
{
    winPlotCU->show();
    winPlotCU->raise();
    winPlotCU->setActiveWindow();
    std::vector<double> data;
//    for (double x = this->Umax; x <= this->Umin; x -= this->dU)
    for (double x = this->Umin; x <= this->Umax; x += this->dU)
    {   
        this->U =x;
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
/*
void MainWindow::setCapacity()
{
    int nv = model->nV(); // number of defined nodes
    int nw = model->nW(); // number of undefined nodes
    int ni = model->nI(); // number of current links
    double imax = 1e-300;
    Q3MemArray<double> t( nv+nw );
    int nt;
        t.fill(0.0);
        for (int i = 0; i < ni; ++i)
        {   
            double q = fabs(model->I[i]);
            QPair<int,int> ends = model->ends(i);
            t[ends.first] += q;
            t[ends.second] += q;
            if (q > imax) imax = q; 
        }
        nt=0;
        for (int v = 0; v < nv + nw; ++v)  
        {
            if(t[v] > imax * 1e-3) 
                nt++;
        }

        this->capacity = double(nt)/double(nv+nw);

}*/
void MainWindow::computeCapacityT()
{
    winPlotCT->show();
    winPlotCT->raise();
    winPlotCT->setActiveWindow();
    std::vector<double> data;
    for (double x = this->Tmax; x >= this->Tmin; x -= this->dT)
//    for (double x = this->Tmin; x <= this->Tmax; x += this->dT)
    {   
        this->T =x;
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
    xmin=0;//this->r0;
    xmax=1.;//this->r0+5./this->T;
    if(xmax>1.) xmax=1.;
    dx=(xmax-xmin)/50;
    std::vector<double> data;
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
    for (double x = this->Umin; x <= this->Umax; x += this->dU)
    {
        this->U = x;
        this->U.updateDisplay();
        if(this->flgStop) 
        {
            this->flgStop=false; 
            return;
        }
        computeOneR();
        data.push_back(x);
        data.push_back(model->conductivity);
            this->plotterU->setCurveData(this->numOfCurve,data);
    }
   
    this->numOfCurve++;
}
void MainWindow::computeRU()
{
    winPlotU->show();
    winPlotU->raise();
    winPlotU->setActiveWindow();

    std::vector<double> data;
//    for (double x = this->Umax; x <= this->Umin; x -= this->dU)
    for (double x = this->Umin; x <= this->Umax; x += this->dU)
    {   this->U =x;
        if(this->flgStop) {this->flgStop=false; return;}
        this->computeModel();

            data.push_back(x);
            data.push_back(model->conductivity);
            if(model->conductivity<NCUT*CUTOFF_SIGMA) 
            {
                this->numOfCurve++;
                break;
            }

            this->plotterU->setCurveData(this->numOfCurve,data);
    }
    this->numOfCurve++;
}
void MainWindow::compute_pSigma()
{
    winPlotU->show();
    winPlotU->raise();
    winPlotU->setActiveWindow();
    std::vector<double> data;
    int r_type = this->typeResistor->checkedId();
    this->selectSigma(r_type);
    double Smin = 1e300, Smax = -1e300;
    double q;
    for (int i = 0; i < model->nI(); ++i)
    {   
        q = (model->Sigma[i]);
        if (fabs(q) > Smax&&q!=this->sigmaU) Smax = fabs(q); 
//        if (fabs(q) < Smin&&q!=CUTOFF_SIGMA) Smin = fabs(q);
        if (fabs(q) < Smin) Smin = fabs(q);
    }
    int nS=201;
    Q3MemArray<double> pS( nS );
    pS.fill(0.0);
    double dS=(Smax-Smin)/(nS-1);
    int e;
    int ni=model->nI();
        for (int i = 0; i < model->nI(); ++i)
        {   
            
            q = (model->Sigma[i]);
            if(q!=this->sigmaU) 
//            if(q!=this->sigmaU&&q!=CUTOFF_SIGMA) 
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
        int isum=ni-6*(this->rows)-4;
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

void MainWindow::compute_SumpSigma()
{
    winPlotU->show();
    winPlotU->raise();
    winPlotU->setActiveWindow();
    std::vector<double> data;
    int r_type = this->typeResistor->checkedId();
    this->selectSigma(r_type);
    double Smin = 1e300, Smax = -1e300;
    double q;
    for (int i = 0; i < model->nI(); ++i)
    {   
        q = (model->Sigma[i]);
        if (fabs(q) > Smax&&q!=this->sigmaU) Smax = fabs(q); 
//        if (fabs(q) < Smin&&q!=CUTOFF_SIGMA) Smin = fabs(q);
        if (fabs(q) < Smin) Smin = fabs(q);
    }
    int nS=201;
    Q3MemArray<double> pS( nS );
    pS.fill(0.0);
    double dS=(Smax-Smin)/(nS-1);
    int e;
    int ni=model->nI();
        for (int i = 0; i < model->nI(); ++i)
        {   
            
            q = (model->Sigma[i]);
            if(q!=this->sigmaU) 
//            if(q!=this->sigmaU&&q!=CUTOFF_SIGMA) 
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
        int isum=ni-6*(this->rows)-4;
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

void MainWindow::computeRT1()
{
    winPlotT->show();
    winPlotT->raise();
//    winPlotT->setActiveWindow();
//    winPlotT->activateWindow();
    std::vector<double> data;
//    for (double x =this->Tmax; x <= this->Tmin; x -= dT)
    for (double x =this->Tmin; x <= this->Tmax; x += dT)
    {   
        this->T=x;
        if(this->flgStop) 
        {
            this->flgStop=false; 
            return;
        }
        this->computeOneR();
            data.push_back(this->T);
            data.push_back(model->conductivity);
            this->plotterT->setCurveData(this->numOfCurve,data);
    }
    this->numOfCurve++;
}
void MainWindow::computeRT()
{
    winPlotT->show();
    winPlotT->raise();
    winPlotT->setActiveWindow();
    std::vector<double> data;
    for (double x = this->Tmax; x >= this->Tmin; x -= dT)
//    for (double x = this->Tmin; x <= this->Tmax; x += dT)
    {   this->T=x;
        if(this->flgStop) 
        {
            this->flgStop=false; 
            return;
        }
        this->computeModel();
 //       double y=log10(model->conductivity);
        double y=model->conductivity;
        data.push_back(this->T);
        data.push_back(y);
            if(model->conductivity<NCUT*CUTOFF_SIGMA) 
            {
                this->numOfCurve++;
                break;
            }
            this->plotterT->setCurveData(this->numOfCurve,data);
    }
    this->numOfCurve++;
}


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

void MainWindow::randomizeSigma_1()
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
}
double MainWindow::singleSigmaT0(double E, double r, double rEx, double EFc)
{
  double Ey, V, rr;
  double exp0, G0,dG;
  double Delta_r, AA, BB, alpha;
//  const double RMIN=200;//было//nm
//  const double RMAX=400;//было//nm
  const double RMIN=250;//стало//nm
  const double RMAX=350;//стало//nm
  const double E0=560.;//meV
  if(E<=0) return 0;//CUTOFF_SIGMA;
  Delta_r=20;//15;//13;//15;//0.5*RC/sqrt(2*U/EF-1.);
  rr=0.5*(RMIN+r*(RMAX-RMIN))/Delta_r;
  rr=rr*rr;
  AA=3*rr-1;
  BB=(1+rr);
  V=2*this->U/BB;
  BB=BB*BB*BB;
  Ey=(2/Delta_r)*sqrt(2*E0*this->U*AA/BB);
  if((V+0.5*Ey)>EFc) 
  {
  Delta_r=20+(V+0.5*Ey-EFc);
//  Delta_r=20+1.5*(V+0.5*Ey-EFc);//было
//  Delta_r=15+(V+0.5*Ey-EF)*0.6;//0.5*RC/sqrt(2*U/EF-1.);
//  Delta_r=15+(V+0.5*Ey-EF)*(V+0.5*Ey-EF)*0.01;//0.5*RC/sqrt(2*U/EF-1.);
  rr=0.5*(RMIN+r*(RMAX-RMIN))/Delta_r;
  rr=rr*rr;
  AA=3*rr-1;
  BB=(1+rr);
  V=2*this->U/BB;
  BB=BB*BB*BB;
  Ey=(2/Delta_r)*sqrt(2*E0*this->U*AA/BB);
  }

  alpha=-6.28*(E-0.5*Ey-V)/rEx;
//      if(alpha>700) return CUTOFF_SIGMA;
//      else if(alpha<-700) exp0=0.;
//          else 
              exp0=exp(alpha);
  G0=1./(1+exp0);
  alpha=alpha+6.2832*Ey/rEx;
  dG=1./(1+exp(alpha));
//      if(alpha>700) return G0;
//      else if(alpha<-700) dG=0.;
//          else dG=1./(1+exp(alpha));
  while(dG>1e-15){//CUTOFF_SIGMA) {
  G0=G0+dG;
  alpha=alpha+6.2832*Ey/rEx;
  dG=1./(1+exp(alpha));
 //     if(alpha>700) return G0;
 //     else if(alpha<-700) dG=0.;
 //         else dG=1./(1+exp(alpha));
  }
   return G0;}
//if(G0<CUTOFF_SIGMA)  return G0;}

double MainWindow::singleSigma(double r, double r1)
{   
    double Gtot, E,dE, Emin,csh,sum,sumt,alpha,Ec;
    double kT=this->T;
    dE=(10.*kT)/100.;
    if(dE>=0.6) dE=0.5;
    sumt=0.;
    Ec=EF;
//    Ec=EF-0.05*kT*kT;    
//    Ec=EF-0.2*kT*kT;    
    if(kT==0) 
    {
      if(Ec>0)  Gtot=singleSigmaT0(Ec,r,r1,Ec);
 //     else Gtot=CUTOFF_SIGMA;
    }
    else
    {   Gtot=0;
        Emin=Ec-5*kT;
        if(Emin<0) Emin=dE;
        for(E=Emin; E<=Ec+5*kT; E+=dE){
//        for(E=EF-5*this->T; E<=EF+5*this->T; E+=dE){
        alpha=0.5*(E-Ec)/kT;
        csh=1./cosh(alpha);
//            if(fabs(alpha)>700) csh=0.;
//            else csh=1./cosh(alpha);
  sum=0.25*csh*csh/kT;
  sumt=sumt+sum; 
  Gtot=Gtot+singleSigmaT0(E,r,r1,Ec)*sum;
             }
        Gtot=Gtot/sumt;
    }
  if(Gtot<CUTOFF_SIGMA) return CUTOFF_SIGMA;
  else 
      return Gtot;
}
     
void MainWindow::randomizeSigma_2()
{   double x1,x2;
   Q3MemArray<double> t( model->nI() );
//    static int seed;
    VSLStreamStatePtr stream;
//    vslNewStream( &stream, VSL_BRNG_MCG31, 0);
//    vslNewStream( &stream, VSL_BRNG_MCG31, seed++ );
    vslNewStream( &stream, VSL_BRNG_MCG31, this->seed );
    vdRngUniform( 0, stream, model->nI(), model->Sigma.data(), 0.0, 1.0 );
    vdRngUniform( 0, stream, t.size(), t.data(), 0.0, 1.0 );

    vslDeleteStream( &stream );
//  for (int i = 0; i < model->nI(); ++i) model->Sigma[i] = this->sigma0;
    
 
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
 //              x2=0.0001+t[i];//++// имеет смысл Ex
//               x2=0.0001+3*t[i];//prrr
               x2=this->Ex;//0.001+0.2*t[i];// имеет смысл Ex
            model->Sigma[i] = singleSigma(x1,x2);
            if (x1 < this->r_c) model->Sigma[i] = CUTOFF_SIGMA;
}     
    }
}

void MainWindow::selectSigma(int i)
{
    this->setModel();
    switch(i)
    {
    case 0: /* uniform Sigma */
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
        else model->Sigma[i] = singleSigma(this->rand,this->Ex);//1.;//this->rand;
     }
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

