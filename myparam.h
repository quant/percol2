#ifndef myparam_h_included
#define myparam_h_included

#include <qwidget.h>
#include <qlineedit.h>

class MyParamD : public QObject
{
    Q_OBJECT
public:
    MyParamD(double _v = 0) : v(_v), ledit(0) {}
    MyParamD& operator=(const MyParamD&);
    double& operator=(double d) { return v = d; }
    double& operator+=(double d) { return v += d; }
    operator double() const { return v; }
    void setDisplay(const QString& label, QWidget *parent);
    ~MyParamD() { if (ledit) delete ledit; }

public slots:
    void updateDisplay();
    void textToValue();

signals:
    void valueChanged();

private:
    QLineEdit *ledit;
    double v;
};

class MyParamI : public QObject
{
    Q_OBJECT
public:
    MyParamI(int _v = 0) : v(_v), ledit(0) {}    
    int& operator=(int d) { return v = d; }
    int& operator+=(int d) { return v += d; }
    operator int() const { return v; }
    void setDisplay(const QString& label, QWidget *parent);
    ~MyParamI() { if (ledit) delete ledit; }

public slots:
    void updateDisplay();
    void textToValue();

signals:
    void valueChanged();

private:
    QLineEdit *ledit;
    int v;
};

#endif
