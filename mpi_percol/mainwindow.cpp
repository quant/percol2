#include "mainwindow.h"
#include "mkl.h"
#include <math.h>


const double CUTOFF_SIGMA = 1e-15;
const int NCUT = 10000;
const double Delta_r=30.;//15; //nm
const double E0=560.;//meV
const double Vg0=50;
const double Cg0=-0.12;//-0.05;//-0.04;
const double EF0=20;
double elementCr = 1e+22;
double sigma_m=0.1;
const double G_ser=3.;
void MainWindow::clear(void)
{
}

void MainWindow::setModel()
{   
    if (model) delete model;
    model = new PercolRect(this->rows,this->cols);
}

MainWindow::MainWindow()
: randc(0.5), typeCond(3),sigmaU(1000.0), 
T(0.13), Tmin(0.1),Tmax(5.301), dT(0.1), 
U(190), Umin(150.), Umax(300), dU(5.), 
r_c(0.0), Ex(15.), Ey(4.), rand(0.5), EF(20),EFT(20.),
rows(30), cols(53), seed(0), model(0)

{
    this->setModel();
    this->typeResistor = 2;
    this->selectSigma( this->typeResistor );
}

void MainWindow::computeModel()
{  
    int r_type = this->typeResistor;
    this->selectSigma(r_type);
    model->compute();
}

void MainWindow::computeRT(std::vector<X_of_T> & data)
{
    int G_type = typeCond;
    if (G_type != 4) 
    {
        computeEFT();
    }
    int NT=1 +int((this->Tmax-this->Tmin)/this->dT);
    for(int j=0; j<NT; j++)
    {           
        double x=this->Tmax-this->dT*j;
        this->T=x;
        if(G_type==4) this->EFT=EF0;
        else
        {
                double EFTU=this->EFTarray[j];
                this->EFT=EFTU;
        }
        this->computeModel();
        double y=model->conductivity;
        y=(this->cols-3)*y/this->rows;
        y=6.28*y;
        printf("T=%lg G=%lg\n",x,y);
        X_of_T  xy;
        xy.T = x;
        xy.G = y;
        data.push_back(xy);
    }
}
//!!!!!!
void MainWindow::computeEF_TU()
{   
    double E,EFT0,EFT1,EFT2 ;
    double dE=0.1;
    double sum, sum1, Area, sum10, sum11, sum12;
    double Ucur=this->U;//!!!!!!!!!!!
    this->U=Vg0;
    double Vd0=Vdot();
    this->U=Ucur;
    double aa=(sqrt(1250.)-350)/Delta_r;
    aa=aa*aa;
    aa=4*this->U/(1+aa);
//    aa=0;
    if(this->T==0) 
    {   
        EFT1=aa+EF0+Vdot()-Vd0+Cg0*(Ucur-Vg0);//!!!!!!!!!!
    }
    else
    {    
        //T!=0
    double Vd=Vdot();
    this->EF=aa+EF0+Vd-Vd0+Cg0*(Ucur-Vg0); 
    int NE=int((this->EF+40*this->T-Vd)/dE);
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
//        this->density=sum;
    if(sum!=0) 
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
//        int j=0;
        while(fabs(sum11-sum)>0.001*sum)
        {
            EFT2=EFT1-(sum11-sum)*(EFT1-EFT0)/(sum11-sum10);
            sum12=computeSum(NE, dE, Vd, EFT2);
//            j++;
            if(sum12>sum&&sum11<sum||sum12<sum&& sum11>sum) 
            {
                sum10=sum11;
                EFT0=EFT1;
            }
                sum11=sum12;
                EFT1=EFT2;
        }
    }
    }
        this->EFT=EFT1;
    }

//!!!!!!!
void MainWindow::computeEFT()
{   
    double E,EFT0,EFT1,EFT2 ;
    int NT=1+int( (this->Tmax-this->Tmin)/this->dT );
    this->EFTarray.resize(NT,0.0);
//    printf("EFTarray has %i points",NT);
/*    for(int j=0; j<NT; j++)
    {
        this->EFTarray[j]=EF0;
    }
*/
    double dE=0.1;
    double Ucur=this->U;
    this->U=Vg0;
    double Vd0=Vdot();
    this->U=Ucur;
    double Vd=Vdot();
    double aa1=(sqrt(1250.)-350)/Delta_r;
    aa1=aa1*aa1;
    aa1=4/(1+aa1);
    double aa=aa1*this->U;
//    double EF00=this->EF0;
    this->EF=aa+EF0+Vd-Vd0+Cg0*(this->U-Vg0);
//    return;
    int NE = 1+int( (this->EF+40*this->Tmax-Vdot())/dE );
    if(NE<0) return;
    printf("AreaEf has %i points",NE);
    this->AreaEf.resize(NE,0.0);
    double sum, sum1, Area, sum10, sum11, sum12;
    sum=0;
    for (int i=0; i< NE; ++i)
    {
        E=dE*(i+1)+Vd;
        Area=AreaE(E)/10000;
        this->AreaEf[i]=Area;
        if(E<=this->EF) sum=sum+Area;
    }
    if(sum==0) return;
    else
    {
    EFT1=this->EF-1.;
    for(int j=0; j<NT; j++)
    {
        this->T=this->Tmax-this->dT*j;
        EFT0=EFT1;
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

void MainWindow::computeReffT(std::vector<X_of_T> & data)
{
//    std::vector<double> data;
    int r_type = this->typeResistor;
    int G_type = typeCond;
    if(G_type!=4) 
    {
        computeEFT();
//        double EFTU=this->EFTarray[0];
//        if(EFTU==0) return;
    }
    int j=0;
    double y_old=0;
//     double EF00=this->EF0;
  for (double x = this->Tmax; x >= this->Tmin; x -= dT)
    {   this->T=x;
        if(G_type==4) this->EFT=EF0;
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
            break;
        }
        }
        this->selectSigma(r_type);
        double y=effective_medium(y_old);
        y_old=y;
//        y=(this->rows)*y/(this->cols-3);
        double y1=6.28*y;
        X_of_T  xy;
        xy.T = x;
        xy.G = y1;
        data.push_back(xy);
    }
}
void MainWindow::computeRU(std::vector<X_of_T> & data)
{
//    std::vector<double> data;
//    std::vector<double> data1;
    int G_type = typeCond;
        if(G_type!=4) 
    {
        computeEFU();
//        double EFTU=this->EFUarray[0];
//        if(EFTU==0) return;
    }
    int j=0;
//    double EF00=this->EF0;
    for (double x = this->Umin; x <= this->Umax; x += this->dU)
  {   
    this->U =x;
        if(G_type==4) this->EFT=EF0;
        else
        {
            if(j<this->EFUarray.size()) 
            {
                double EFTU=this->EFUarray[j];
                this->EFT=EFTU;
                if(EFTU<=0) break; 
                j++;
            }
            else 
            {
            break;
        }
        }
    this->computeModel();
    double y=model->conductivity;
        y=(this->cols-3)*y/(this->rows);
        double y1=6.28*y;
        X_of_T  xy;
        xy.T = x;
        xy.G = y1;
        data.push_back(xy);
    }
}
void MainWindow::computeReffU(std::vector<X_of_T> & data)
{
        int r_type = this->typeResistor;
        int G_type = typeCond;
        if(G_type!=4) 
    {
        computeEFU();
//        double EFTU=this->EFUarray[0];
 //       if(EFTU==0) return;
    }
    int j=0;
    double y_old=0;
    for (double x = this->Umin; x <= this->Umax; x += this->dU)
    {   
    this->U =x;
        if(G_type==4) this->EFT=EF0;
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
            break;
        }
//        if(EFTU==0) break;
        }
        this->selectSigma(r_type);
        double y=effective_medium(y_old);
        y_old=y;
        double y1=6.28*y;
        X_of_T  xy;
        xy.T = x;
        xy.G = y1;
        data.push_back(xy);
    }
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
if(G0>CUTOFF_SIGMA)  return G0;
else return CUTOFF_SIGMA;
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
        {
            y2=y1-sum11*(y1-y0)/(sum11-sum10);
            double sum12=average(y2)/Totsum;
            j++;
            if(sum12>0&&sum11<0||sum12<0&& sum11>0) 
            {
                sum10=sum11;
                y0=y1;
            }
              sum11=sum12;
              y11=y1;
              y1=y2;
        }

     return y1;
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
            r1=r1/Delta_r;
            r2=r2/Delta_r;
            r3=r3/Delta_r;
            r4=r4/Delta_r;
            Uxy=this->U*(1/(1+r1*r1)+1/(1+r2*r2)+1/(1+r3*r3)+1/(1+r4*r4));
            if(Uxy<=E) Area=Area+1;
            }
            }
        }
    }
    return Area;
}

void MainWindow::computeEFU()
{   
    double E,EFT0,EFT1,EFT2 ;
    double dE=0.1;
    double sum, sum1, Area, sum10, sum11, sum12;
    int NU=1+int( (this->Umax-this->Umin)/this->dU );
    this->EFUarray.resize(NU,0.0);
 //   double Ucur=this->U;
    this->U=Vg0;
    double Vd0=Vdot();
    double aa1=(sqrt(1250.)-350)/Delta_r;
    aa1=aa1*aa1;
    aa1=4/(1+aa1);
//    double EF00this->EF0;
    if(this->T==0) {
    int j=0;  
    for(double x=this->Umin; x<=this->Umax; x+=this->dU)
    {   this->U=x; 
        double aa=aa1*this->U; 
//        aa=0;
        EFT1=aa+EF0+Vdot()-Vd0+Cg0*(this->U-Vg0);
        if(j<this->EFUarray.size())  
            {   
                this->EFUarray[j]=EFT1;
                j++;
                this->EFT=EFT1;
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
        this->EF=aa+EF0+Vd-Vd0+Cg0*(this->U-Vg0); 
    int NE = int( (this->EF+40*this->T-Vd)/dE );
    this->AreaEf.resize(NE,0.0);
    sum=0;
    EFT1=this->EF-1;
    for (int i=0; i< NE; ++i)
    {
        E=dE*(i+1)+Vd;
        Area=AreaE(E)/10000;
        this->AreaEf[i]=Area;
        if(E<=this->EF) sum=sum+Area;
    }  
    if(sum==0) 
    {
             j++;
    }
    else
    {
        EFT0=EFT1;
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
            if(j<this->EFUarray.size())  
            {   
                this->EFUarray[j]=EFT1;
                j++;
                this->EFT=EFT1;
            }
            else break; 
    }
    }
}

double MainWindow::sedlo(double E, double Ey, double Ex, double V)
{ double  alpha,G0,g,exp0,EE, Ep,Uc;
  Uc=Vdot();
  double a1=150;//80;//100;//500;//nm
  double Va=V-12.50;//meV
  V=V+2*exp(-this->T/0.6);
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
  return G0;
}

double MainWindow::Vbarrier(double r)
{
  double BB,rr;
//  const double RMIN=150;//nm
//  const double RMAX=450;//nm
  const double RMIN=190;//nm
  const double RMAX=410;//nm
//  const double RMIN=200;//nm
//  const double RMAX=400;//nm
  rr=0.5*(RMIN+r*(RMAX-RMIN))/Delta_r;
  rr=rr*rr;
  BB=(1+rr);
  BB=2./BB;
  double aa1=(sqrt(1250.)-350)/Delta_r;
  aa1=aa1*aa1;
  aa1=4/(1+aa1);
  return (aa1+BB)*this->U;
}
double MainWindow::Vdot(void)
{double x1,y1,r1;
            x1=500;
            y1=500;
            r1=sqrt(x1*x1+y1*y1)-350;
            r1=r1/Delta_r;
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
//    double g0=sedlo(Ec, this->Ey, this->Ex, V);
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
        int G_type = this->typeCond;
    Emin=Ec-40*kT;
    double sumt=0.;
    double aa=0.25*dE/kT;
    if(Emin<Uc) Emin=Uc+dE;
        for(E=Emin; E<=Ec+40*kT; E+=dE){
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
        double eps=0.0075*this->U-1.02;
        if(G_type==3) Gtot=GOver+GTunnel*exp(-eps/kT);
    }
    Gtot=Gtot*G_ser/(Gtot+G_ser);

  if(Gtot<CUTOFF_SIGMA) return CUTOFF_SIGMA;
  else 
      return Gtot;
}
void MainWindow::randRcr()
{   
    int i_Rcr = model->index_of_Rcr();
    elementCr = fabs((model->I[ i_Rcr ]));
    this->sigmaMin=model->Sigma[i_Rcr ];
    VSLStreamStatePtr stream;
    vslNewStream( &stream, VSL_BRNG_MCG31, this->seed );
    vdRngUniform( 0, stream, model->nI(), &model->Sigma[0], 0.0, 1.0 );
    vslDeleteStream( &stream );
    double x1=model->Sigma[i_Rcr];
    this->randc=x1;
    this->rand=x1;
}
void MainWindow::randomizeSigma_2()
{   double x1;
    std::valarray<double> t( model->nI() );
    VSLStreamStatePtr stream;
    vslNewStream( &stream, VSL_BRNG_MCG31, this->seed );
    vdRngUniform( 0, stream, model->nI(), &model->Sigma[0], 0.0, 1.0 );
    vslDeleteStream( &stream );
    for (int i=0; i < model->nI(); ++i)
    {
        std::pair<int,int> ends = model->ends(i);
        int from = ends.first;
        int to   = ends.second;
        std::pair<double,double> xy0 = model->xy(from);
        std::pair<double,double> xy1 = model->xy(to);
 
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
            else model->Sigma[i] = singleSigma(x1);
        }     
    }
}
void MainWindow::randomizeSigma_1()
{   double x1;
    VSLStreamStatePtr stream;
    vslNewStream( &stream, VSL_BRNG_MCG31, this->seed );
    vdRngUniform( 0, stream, model->nI(), &model->Sigma[0], 0.0, 1.0 );
    vslDeleteStream( &stream );
    for (int i=0; i < model->nI(); ++i)
    {
        std::pair<int,int> ends = model->ends(i);
        int from = ends.first;
        int to   = ends.second;
        std::pair<double,double> xy0 = model->xy(from);
        std::pair<double,double> xy1 = model->xy(to);
 
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
            if (x1 < this->fraction) model->Sigma[i] = CUTOFF_SIGMA;
            else model->Sigma[i] =1;
        }
 
        }     
    
}

void MainWindow::selectSigma(int i)
{
    this->setModel();
    switch(i)
    {
    case 0: /* uniform Sigma */
        {
        for (int i = 0; i < model->nI(); ++i) 
            {
        std::pair<int,int> ends = model->ends(i);
        int from = ends.first;
        int to   = ends.second;
        std::pair<double,double> xy0 = model->xy(from);
        std::pair<double,double> xy1 = model->xy(to);
 
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
