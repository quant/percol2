#ifndef PLOTTER_H_INCLUDED
#define PLOTTER_H_INCLUDED

//#include <qpixmap.h>
//#include <qwidget.h>
//Added by qt3to4:
#include <QtGui>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPaintEvent>
#include <vector>
#include <map>

class QToolButton;
class PlotSettings;

typedef std::vector<double> CurveData;

class Plotter : public QWidget
{
    Q_OBJECT
public:
    Plotter(QWidget *parent = 0, Qt::WindowFlags flags=0);
//old    Plotter(QWidget *parent = 0, const char *name=0,
//old        Qt::WFlags flags=0);
    void setPlotSettings(const PlotSettings &settings);
    void setCurveData(int id, const CurveData &data);
    void clearCurve(int id);
    int firstUnusedId() const;
    QSize minimumSizeHint() const;
    QSize sizeHint() const;
public slots:
    void scaleX();
    void scaleY();
    void zoomIn();
    void zoomOut();
    void zoomAll();
    void clearAll();
protected:
    void paintEvent(QPaintEvent *event);
    void resizeEvent(QResizeEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void keyPressEvent(QKeyEvent *event);
    void wheelEvent(QWheelEvent *event);
    void enterEvent(QEvent *event);
private: 
    void moveButtons();
    void updateRubberBandRegion();
    void refreshPixmap();
    void drawGrid(QPainter *painter);
    void drawCurves(QPainter *painter);
    void captureBoundsToSettings();
    enum {Margin = 60};
    QToolButton *zoomInButton;
    QToolButton *zoomOutButton;
    QToolButton *zoomAllButton;
    QToolButton *eraseButton;
    QToolButton *scaleXButton;
    QToolButton *scaleYButton;
    std::map<int, CurveData> curveMap;
    std::vector<PlotSettings> zoomStack;
    int curZoom,yScale,xScale;
    bool rubberBandIsShown;
    bool savePlot();
    bool openPlot();
    bool savePlotAs();
    QRect rubberBandRect;
    QPixmap pixmap;
    QString curFileCurve;
};

class PlotSettings
{
public:
    PlotSettings();
    void scroll(int dx, int dy);
    void adjust();
    double spanX() const {return maxX-minX;}
    double spanY() const {return maxY-minY;}
    double minX;
    double maxX;
    int numXTicks;
    double minY;
    double maxY;
    int numYTicks;
private:
    void adjustAxis(double &min, double &max, int &numTicks);
    double nicenum(double x, int round);
    double expt(double a, int n);

};

#endif /*PLOTTER_H_INCLUDED*/
