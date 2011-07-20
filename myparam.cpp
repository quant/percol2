//#include <q3vbox.h>
#include <qlabel.h>
#include "myparam.h"
#include <QtGui/QHBoxLayout>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>

void
MyParamD::setDisplay(const QString& label, QWidget *parent)
{
    if (this->ledit) delete this->ledit;
    QHBoxLayout *hl = new QHBoxLayout(parent);
    hl->addWidget(new QLabel(label));
    hl->addWidget(this->ledit = new QLineEdit);
    connect(ledit,SIGNAL(returnPressed()),this,SLOT(textToValue()));
    updateDisplay();
}

void
MyParamD::setDisplay(const QString& label, QLayout *parent)
{
    if (this->ledit) delete this->ledit;
    QHBoxLayout *hl = new QHBoxLayout(parent);
    hl->addWidget(new QLabel(label));
    hl->addWidget(this->ledit = new QLineEdit);
    connect(ledit,SIGNAL(returnPressed()),this,SLOT(textToValue()));
    updateDisplay();
}

void MyParamD::textToValue()
{
    if (!ledit) return;
    double t = this->ledit->text().toDouble(); 
    if (t != this->v)
    {
        this->v = t;
        emit valueChanged();
    }
}

void MyParamD::updateDisplay()
{
    if (!ledit) return;
    QString aText;
    aText.sprintf("%lg",this->v);
    this->ledit->setText(aText);
}

//--------------------------------------------------------------------
void
MyParamI::setDisplay(const QString& label, QLayout *parent)
{
    if (ledit) delete ledit;
    QHBoxLayout *hl = new QHBoxLayout(parent);
    hl->addWidget(new QLabel(label));
    hl->addWidget(this->ledit = new QLineEdit);
    connect(ledit,SIGNAL(returnPressed()),this,SLOT(textToValue()));
//    connect(this,SIGNAL(valueChanged()),this,SLOT(updateDisplay()));
    updateDisplay();
}
void
MyParamI::setDisplay(const QString& label, QWidget *parent)
{
    if (this->ledit) delete this->ledit;
    QHBoxLayout *hl = new QHBoxLayout(parent);
    hl->addWidget(new QLabel(label));
    hl->addWidget(this->ledit = new QLineEdit);
    connect(ledit,SIGNAL(returnPressed()),this,SLOT(textToValue()));
//    connect(this,SIGNAL(valueChanged()),this,SLOT(updateDisplay()));
    updateDisplay();
}

void MyParamI::textToValue()
{
    if (!ledit) return;
    int t = this->ledit->text().toInt();
    if (t != this->v)
    {
        this->v = t;
        emit valueChanged();
    }
}

void MyParamI::updateDisplay()
{
    if (!ledit) return;
    QString aText;
    aText.sprintf("%i",this->v);
    this->ledit->setText(aText);
}
