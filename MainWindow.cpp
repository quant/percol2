#include <q3popupmenu.h>
#include <qmenubar.h>
#include <qapplication.h>
#include <qmessagebox.h>
#include <q3canvas.h>
#include <qlabel.h>
#include <qfile.h>
#include <q3textstream.h>
//Added by qt3to4:
#include <QResizeEvent>
#include <QMouseEvent>
#include <Q3VBoxLayout>
#include <Q3MemArray>
#include "mainwindow.h"
#include "mkl.h"
#include <math.h>
#include <qstring.h>
#include <qvalidator.h> 
#include <qlayout.h>  
#include <qlineedit.h> 
#include <q3toolbar.h>
#include <q3hbox.h>
#include <q3vbox.h>
#include <q3filedialog.h>
#include <qfont.h> 
#include <qfontdialog.h> 
//#include <qgroupbox.h> 
#include <q3buttongroup.h> 
#include <qradiobutton.h> 
#include <q3grid.h>
#include <qpushbutton.h>
#include "myparam.h"

const double CUTOFF_SIGMA = 1e-10;
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
ColorScale2 csI; // Current scale and edge coloring
ColorScale csSigma; // Sigma scale and edge coloring

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
    QMenuBar* menu = menuBar(); 

    Q3PopupMenu* file = new Q3PopupMenu( menu );
    file->insertItem("&Save As", this, SLOT(saveAs()), Qt::CTRL + Qt::Key_S);
    file->insertSeparator();
    file->insertItem("Choose Font", this, SLOT(chooseFont()), Qt::ALT + Qt::Key_F);
    file->insertSeparator();
    file->insertItem("E&xit", qApp, SLOT(quit()), Qt::CTRL + Qt::Key_Q);
    file->insertSeparator();
    menu->insertItem("&Menu", file);


    menu->insertSeparator();

    Q3PopupMenu* help = new Q3PopupMenu( menu );
    help->insertItem("&About", this, SLOT(help()), Qt::Key_F1);
    menu->insertItem(tr("Помощь"),help);
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
    this->dispCond = new QLabel("----",
            this->statusBar());
    this->statusBar()->addWidget(this->dispCond);
    this->dispDeltaI = new QLabel("----",
            this->statusBar());
    this->statusBar()->addWidget(this->dispDeltaI);
    this->dispFerr = new QLabel("----",
            this->statusBar());
    this->statusBar()->addWidget(this->dispFerr);
    this->dispBerr = new QLabel("----",
            this->statusBar());
    this->statusBar()->addWidget(this->dispBerr);
    this->dispConduct = new QLabel("----",
            this->statusBar());
    this->statusBar()->addWidget(this->dispConduct);
}



void MainWindow::initControlDockWindow()
{
    Q3ToolBar *control = new Q3ToolBar(this);
    control->setLabel(tr("Control Panel"));
//    control->setOrientation(Qt::Vertical);

    Q3GroupBox *oneResistorBox =new Q3GroupBox(6, Qt::Vertical, "One Resistor", control);
    Q3Grid *gr1=new Q3Grid(2,Qt::Horizontal, oneResistorBox);
    gr1->setMargin(3);
    gr1->setSpacing(10);

    this->T.setDisplay(("T(meV)"), gr1);
    this->U.setDisplay(("U(meV)"), gr1);
    this->Ex.setDisplay(("E_x(meV)"), gr1);
    this->sigma0.setDisplay(("r[0--1]"), gr1);
    this->Tmin.setDisplay(("T_min"),gr1);
    this->Umin.setDisplay(("U_min"),gr1);
    this->Tmax.setDisplay(("T_max"),gr1);
    this->Umax.setDisplay(("U_max"),gr1);
    this->dT.setDisplay(("dT"), gr1);
    this->dU.setDisplay(("dU"), gr1);

//    this->ExEdit->setValidator(&theDoubleValidator);


    Q3ButtonGroup *pt1Buttons =new Q3ButtonGroup(2, Qt::Horizontal, "Compute Conductance G", oneResistorBox);

    QPushButton *TButton1  = new QPushButton(tr("G(T)"), pt1Buttons); 
    TButton1->setMinimumWidth( TButton1->sizeHint().width());
    connect(TButton1,SIGNAL(clicked()), this, SLOT(computeRT1()));

    QPushButton *uButton1 = new QPushButton(tr("G(U)"), pt1Buttons);
    uButton1->setMinimumWidth( uButton1->sizeHint().width());
    connect(uButton1,SIGNAL(clicked()), this, SLOT(computeRU1()));


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

    this->moveDockWindow(control,Qt::Right);
    setDockEnabled(Qt::DockLeft, false);  
    setDockEnabled(Qt::DockBottom, false);  
    setDockEnabled(Qt::DockTop, false); 
    control->setCloseMode(Q3DockWindow::Undocked);
    control->setMinimumWidth( control->sizeHint().width());
} 


void MainWindow::initCanvasView() 
{

//    QCanvas *c = new QCanvas(800,800);
    Q3Canvas *c = new Q3Canvas(600,600);
    c->setAdvancePeriod(30);
    cv = new Q3CanvasView(c,this);
    setCentralWidget(cv);
}
void MainWindow::initPlotterU() 
{
    winPlotU = new QDialog(this);
    winPlotU->setCaption(tr("Dependences G(U):"));
    Q3VBoxLayout *layout=new Q3VBoxLayout(winPlotU);
    this->plotterU= new Plotter(winPlotU);
    this->plotterU = new Plotter(winPlotU);
    this->plotterU->setCurveData(1, this->plotdata);
    layout->addWidget(this->plotterU);
}


void MainWindow::initPlotterT() 
{
    winPlotT = new QDialog(this);
    winPlotT->setCaption(tr("Dependences G(T):"));
    Q3VBoxLayout *layout=new Q3VBoxLayout(winPlotT);
    this->plotterT= new Plotter(winPlotT);
    this->plotterT->setCurveData(1, this->plotdata);
    layout->addWidget(this->plotterT);
}
void MainWindow::initPlotterCT() 
{
    winPlotCT = new QDialog(this);
    winPlotCT->setCaption(tr("Capacity(T):"));
    Q3VBoxLayout *layout=new Q3VBoxLayout(winPlotCT);
    this->plotterCT= new Plotter(winPlotCT);
    this->plotterCT->setCurveData(1, this->plotdata);
    layout->addWidget(this->plotterCT);
}
void MainWindow::initPlotterCU() 
{
    winPlotCU = new QDialog(this);
    winPlotCU->setCaption(tr("Capacity(U):"));
    Q3VBoxLayout *layout=new Q3VBoxLayout(winPlotCU);
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
MainWindow::MainWindow(QWidget *parent, const char *name, Qt::WFlags f)
: sigma0(0.5), sigmaU(1000.0), flgStop(false),
T(0.5), Tmin(0.1),Tmax(5.2), dT(0.1), 
U(400), Umin(200.), Umax(700), dU(25.), 
r_c(0.0), Ex(0.5),  Q3MainWindow(parent,name,f),
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
    this->initCanvasView();
    init();
}


bool MainWindow::saveAs()
{
    QString fn = Q3FileDialog::getSaveFileName( QString::null, QString::null, this );
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

    Q3TextStream o(&fV);
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

    Q3TextStream oC(&fC);
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

    Q3CanvasItemList list = cv->canvas()->allItems();
    Q3CanvasItemList::Iterator it = list.begin();
    for (; it != list.end(); ++it) {
	if ( *it )
	    delete *it;
    }
}


class NodeItemFactory
{
    Q3Canvas *canvas;
    ColorScale *cs;
    double scale;
    QPair<double,double> offset;
    int blob_size;
public:
    NodeItemFactory( Q3Canvas *_canvas, ColorScale *_cs, 
        double _scale, QPair<double,double> _offset ) 
        : canvas(_canvas), cs(_cs), scale(_scale), offset(_offset), blob_size(2) {}
    Q3CanvasEllipse *newVnode(double v, QPair<double,double> xy)
    {
        Q3CanvasEllipse *n = new Q3CanvasEllipse(2,2,canvas);
        n->setBrush(QBrush(cs->color(v)));
        n->setPen(QPen(Qt::black,2));
        n->setZ( 128 );
        double canvasx = (xy.first - offset.first) * scale;
        double canvasy = (xy.second - offset.second) * scale;
        n->move( canvasx, canvasy );
        return n;
    }
    Q3CanvasEllipse *newWnode(double v, QPair<double,double> xy)
    {
        Q3CanvasEllipse *n = new Q3CanvasEllipse(blob_size,blob_size,canvas);
        n->setBrush(QBrush(cs->color(v)));
        n->setZ( 128 );
        double canvasx = (xy.first - offset.first) * scale;
        double canvasy = (xy.second - offset.second) * scale;
        n->move( canvasx, canvasy );
        return n;
    }
    void setBlobSize(int s) { blob_size = s; }
};

class EdgeItemFactory
{
    Q3Canvas *canvas;
    ColorScale2 *cs;
    double scale;
    QPair<double,double> offset;
public:
    EdgeItemFactory( Q3Canvas *_canvas, ColorScale2 *_cs,
        double _scale, QPair<double,double> _offset ) 
        : canvas(_canvas), cs(_cs), scale(_scale), offset(_offset) {}
    Q3CanvasLine *newEdge(double c, QPair<double,double> xy0, QPair<double,double> xy1)
    {
        Q3CanvasLine *n = new Q3CanvasLine(canvas);
        if (c == 0)
            n->setPen(QPen(Qt::white));
        else if (c == CUTOFF_SIGMA)
            n->setPen(QPen(Qt::blue));
        else
            n->setPen(QPen(cs->color(c),2));
//          if(c==sigmaMin) n->setPen(QPen(Qt::red)); 
        int canvasx0 = (xy0.first - offset.first) * scale;
        int canvasy0 = (xy0.second - offset.second) * scale;
        int canvasx1 = (xy1.first - offset.first) * scale;
        int canvasy1 = (xy1.second - offset.second) * scale;
        n->setPoints(canvasx0,canvasy0,canvasx1,canvasy1);
        return n;
    }
};


void MainWindow::drawModelA()
{
    clear();
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
    int emax;
    double imin = 1e300, imax = -1e300;
    double q;
    for (int i = 0; i < model->nI(); ++i)
    {   
             q=(model->I[i])*(model->I[i])/(model->Sigma[i]);
        if (fabs(q) > imax) {imax = fabs(q); 
        emax=i;}
        if (fabs(q) < imin) imin = fabs(q);

    }
//    csI.setRange(0., imax);
    this->sigmaMin=model->Sigma[emax];
        this->sigmaMin.updateDisplay();
    csI.setRange(imin, imax);
//    csI.setColors(Qt::white,Qt::green);
    csI.setColors(Qt::white,Qt::black);
//     csI.setExtraColor(0.1*imax,QColor("#007f00"));

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
//        q=(model->I[e])/(model->Sigma[e]);
        q=(model->I[e])*(model->I[e])/(model->Sigma[e]);
        if(q<0.00000000001) q=0.;
        Q3CanvasLine *n = ifactory.newEdge( q , xy0, xy1 );
        n->show();
    }
    setMouseTracking( TRUE );
}

void MainWindow::drawModelI()
{//   this->selectSigma(QComboBox::currentItem());
    clear();
    Q3Canvas *pCanvas = cv->canvas();
    cv->adjustSize();

    // Set view port
    const double factor = 0.95;
    double cw = pCanvas->width();
    double ch = pCanvas->height();
    double xscale = factor * cw  / (model->xmax() - model->xmin());
    double yscale = factor * ch / (model->ymax() - model->ymin());
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
//    csI.setRange(0., imax);
    this->sigmaMin = model->Sigma[emax];
    this->sigmaMin.updateDisplay();
    csI.setRange(imin, imax);
//    csI.setColors(Qt::white,Qt::green);
    csI.setColors(Qt::white,Qt::black);
//    csI.setExtraColor(0.05*imax,QColor("#007f00"));

    EdgeItemFactory ifactory(pCanvas,&csI,scale,offset);

    for (int v = 0; v < model->nV(); ++v)
    {
        double V = model->V[v];
        QPair<double,double> xy = model->xy(v);
        Q3CanvasEllipse *n = vfactory.newVnode(V, xy);
        n->show();
    }

//#if 1    
    int v = model->nV();
    for (int w = 0; w < model->nW(); ++w)
    {
        double W = model->W[w];
        QPair<double,double> xy = model->xy(v+w);
        Q3CanvasEllipse *n = vfactory.newWnode(W, xy);
        n->show();
    }
//#else   
 /*    ColorScale csDI;
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
//#endif
*/
 
    for (int e = 0; e < model->nI(); ++e)
    {
        QPair<int,int> ends = model->ends(e);
        QPair<double,double> xy0 = model->xy(ends.first);
        QPair<double,double> xy1 = model->xy(ends.second);
        q=fabs(model->I[e]);
        if(q<0.00000000001) q=0.;
        Q3CanvasLine *n = ifactory.newEdge( q , xy0, xy1 );
        n->show();
    }
    setMouseTracking( TRUE );
}
void MainWindow::drawModelR()


{  
    clear();
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
}


void
MainWindow::computeModel()
{  
//    this->setModel();
    this->selectSigma(this->typeResistor->selectedId());
    model->compute();
    QString s;
    s.sprintf("%lg",model->rcond);
    double z = s.toDouble();
    this->dispCond->setText(s);
    this->dispCond->update();
//    this->statusBar()->update();
    s.sprintf("%lg",model->deltaI);
    z = s.toDouble();
    this->dispDeltaI->setText(s);
    this->dispDeltaI->update();
    s.sprintf("%lg",model->ferr);
    z = s.toDouble();
    this->dispFerr->setText(s);
    this->dispFerr->update();
    s.sprintf("%lg",model->berr);
    z = s.toDouble();
    this->dispBerr->setText(s);
    this->dispBerr->update();
    s.sprintf("%lg",model->conductivity);
    z = s.toDouble();
    this->dispConduct->setText(s);
    this->dispConduct->update();
    this->statusBar()->update();
//    this->setConductivity();
    this->r_c.updateDisplay();
    this->U.updateDisplay();
    this->T.updateDisplay();
    this->dT.updateDisplay();
//    this->conductivity.updateDisplay();
    this->capacity.updateDisplay();
    qApp->processEvents();
}
void MainWindow::computeOneR()
{  
        model->conductivity=singleSigma(this->sigma0, this->Ex);
        this->U.updateDisplay();
        this->T.updateDisplay();
//        model->conductivity.updateDisplay();
        qApp->processEvents();
}
void 
MainWindow::computeCapacityU()
{
    winPlotCU->show();
    winPlotCU->raise();
    winPlotCU->setActiveWindow();
    std::vector<double> data;
    for (double x = this->Umin; x <= this->Umax; x += this->dU)
    {   
        this->U =x;
        if(this->flgStop) {this->flgStop=false; return;}
        this->computeModel();
        this->setCapacity();
        data.push_back(x);
        data.push_back(this->capacity);
        this->plotterCU->setCurveData(this->numOfCurve,data);
    }
    this->numOfCurve++;
    
}

void 
MainWindow::setCapacity()
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
            if(t[v] > imax * 1e-7) 
                nt++;
        }

        this->capacity = double(nt)/double(nv+nw);

}
void 
MainWindow::computeCapacityT()
{
    winPlotCT->show();
    winPlotCT->raise();
    winPlotCT->setActiveWindow();
    std::vector<double> data;
    for (double x = this->Tmin; x <= this->Tmax; x += this->dT)
    {   
        this->T =x;
        if(this->flgStop) {this->flgStop=false; return;}
        this->computeModel();
        this->setCapacity();
        data.push_back(x);
        data.push_back(this->capacity);
        this->plotterCT->setCurveData(this->numOfCurve,data);
    }
    this->numOfCurve++;
    }

void 
MainWindow::computeRrc()
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
void 
MainWindow::computeRU1()
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
void
MainWindow::computeRU()
{
    winPlotU->show();
    winPlotU->raise();
    winPlotU->setActiveWindow();

    std::vector<double> data;
    for (double x = this->Umin; x <= this->Umax; x += this->dU)
    {   this->U =x;
        if(this->flgStop) {this->flgStop=false; return;}
        this->computeModel();

            data.push_back(x);
            data.push_back(model->conductivity);
            this->plotterU->setCurveData(this->numOfCurve,data);
    }
    this->numOfCurve++;
}



void MainWindow::stopCalc()
    {
    this->flgStop=true;
    }

void
MainWindow::computeRT1()
{
    winPlotT->show();
    winPlotT->raise();
    winPlotT->setActiveWindow();
    std::vector<double> data;
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
void
MainWindow::computeRT()
{
    winPlotT->show();
    winPlotT->raise();
    winPlotT->setActiveWindow();
    std::vector<double> data;
    for (double x = this->Tmin; x <= this->Tmax; x += dT)
    {   this->T=x;
        if(this->flgStop) 
        {
            this->flgStop=false; 
            return;
        }
        this->computeModel();
//        y=6.28*y;
//        if (y>CUTOFF_SIGMA) {
            //y=log10(y);
        data.push_back(this->T);
        data.push_back(model->conductivity);
        this->plotterT->setCurveData(this->numOfCurve,data);
//        }
//        if(this->T<1.2) dT=0.1;
//        else dT=0.2;
    }
    this->numOfCurve++;
}


void
MainWindow::myMouseMoveEvent(QMouseEvent *e)
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
//  const double RC=300.;//nm
  const double RMIN=200;//150.;//nm
  const double RMAX=400;//450.;//nm
  const double E0=560.;//meV
  if(E<=0) return CUTOFF_SIGMA;
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
  Delta_r=20+1.5*(V+0.5*Ey-EFc);//0.5*RC/sqrt(2*U/EF-1.);
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
  while(dG>CUTOFF_SIGMA) {
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
{   double Gtot, E,dE, Emin,csh,sum,sumt,alpha,Ec;
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
      else Gtot=CUTOFF_SIGMA;
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
        else model->Sigma[i] = singleSigma(this->sigma0,this->Ex);//1.;//this->sigma0;
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
    QSize nsz = e->size(); 
    QSize osz = e->oldSize();
    Q3Canvas *c = cv->canvas();

    c->resize( c->width()+nsz.width()-osz.width(),
        c->height()+nsz.height()-osz.height() );
}

