#include <qvbox.h>
#include <qlabel.h>
#include "myparam.h"

void
MyParamD::setDisplay(const QString& label, QWidget *parent)
{
    if (ledit) delete ledit;
    QHBox *hbox = new QHBox(parent);
    new QLabel(label,hbox);
    this->ledit = new QLineEdit(hbox);
    connect(ledit,SIGNAL(returnPressed()),this,SLOT(textToValue()));
//    connect(this,SIGNAL(valueChanged()),this,SLOT(updateDisplay()));
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
MyParamI::setDisplay(const QString& label, QWidget *parent)
{
    if (ledit) delete ledit;
    QHBox *hbox = new QHBox(parent);
    new QLabel(label,hbox);
    this->ledit = new QLineEdit(hbox);
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
