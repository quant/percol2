#include "percol2d.h"
#include "mkl.h"
#include <cassert>
#include <qmessagebox.h>
#include <qstring.h>
#include <qfile.h>
//Added by qt3to4:
#include <Q3MemArray>
#include <cmath>
#include <q3textstream.h>
#include <QMap>

// Find maximum value in a container
template<typename T>
T vmax(T *begin, T *end)
{
    T res;
    for (res = *begin; begin != end; ++begin)
    {
        if (res < *begin) res = *begin;
    }
    return res;
}

Percol2D::Percol2D(void)
{
}

Percol2D::~Percol2D(void)
{
}

// Compute using general matrix
void Percol2D::compute_general()
//void Percol2D::compute()
{
    int nv = this->nV(); // number of defined nodes
    int nw = this->nW(); // number of undefined nodes
    int ni = this->nI(); // number of current links

    // Build LHS matrix = S_W^T SIGMA S_W
    Q3MemArray<double> lhs( nw*nw );
    lhs.fill(0.0);

    for (int i = 0; i < ni; ++i)
    {
        QPair<int,int> ends = this->ends(i);
        int r = ends.first - nv;
        int c = ends.second - nv;
        double sigma = this->Sigma[i];
        if (r >= 0 && c >= 0)
        {
            double sir = this->S(i,r+nv);
            double sic = this->S(i,c+nv);
#define LHS(r,c) lhs.at((r)+nw*(c))
            LHS(r,c) +=  sir * sigma * sic;
            LHS(r,r) +=  sir * sigma * sir;
            LHS(c,r) +=  sic * sigma * sir;
            LHS(c,c) +=  sic * sigma * sic;
        }
        else if (r >= 0)
        {
            double sir = this->S(i,r+nv);
            LHS(r,r) +=  sir * sigma * sir;
        }
        else if (c >= 0)
        {
            double sic = this->S(i,c+nv);
            LHS(c,c) +=  sic * sigma * sic;
        }
    }

    // Build temp vector = SIGMA S_V V
    Q3MemArray<double> t( ni );
    t.fill(0.0);
    for (int i = 0; i < ni; ++i)
    {
        for (int v = 0; v < nv; ++v)
        {
            if (this->S(i,v) == 0) continue;
            t.at(i) += this->S(i,v) * this->V[v] * this->Sigma[i];
        }
    }

    // Build right hand side: rhs = -S(...) SIGMA S_V V
    Q3MemArray<double> rhs( nw );
    rhs.fill(0.0);

    for (int w = 0; w < nw; ++w)
    {
        for (int i = 0; i < ni; ++i)
        {
            if (S(i,nv + w) == 0) continue;
            rhs[ w ] -= S(i,nv + w) * t[i];
        }
    }

    int ONE = 1;
    Q3MemArray<int> ipiv( nw );

    // Before factoring with dgetrf we compute anorm, used later to estimate rcond 
#if WANT_1NORM_RCOND
    double anorm_1 = 0;
    for (int c = 0; c < nw; ++c)
    {
        double colnorm = 0;
        for (int r = 0; r < nw; ++r)
        {
            colnorm += fabs(LHS(r,c));
        }
        if (colnorm > anorm_1) anorm_1 = colnorm;
    }
#else
    double anorm_8 = 0;
    for (int r = 0; r < nw; ++r)
    {
        double rownorm = 0;
        for (int c = 0; c < nw; ++c)
        {
            rownorm += fabs(LHS(r,c));
        }
        if (rownorm > anorm_8) anorm_8 = rownorm;
    }
#endif

    int info;
    //dgesv(&nw, &ONE, lhs.data(), &nw, ipiv.data(), rhs.data(), &nw, &info );
    dgetrf(&nw,&nw,lhs.data(),&nw,ipiv.data(),&info);
    if (info > 0)
    {
        // The system is degenerate, so we'll only solve part of it
        QString msg;
        msg.sprintf("Factorization failed with info = %i (nw=%i)",info,nw);
        switch( QMessageBox::warning( 0, "LU failed",
            msg,"Continue", "Abort", NULL, 0, 0) )
        {
        case 0: // Continue
            return;
        case 1: // Abort
            exit(1);
        }
    }
    Q3MemArray<double> wrk( 4*nw );
    Q3MemArray<int> iwk( nw );
#if WANT_1NORM_RCOND
    dgecon("1-Norm",&nw,lhs.data(),&nw,&anorm_1,&this->rcond,wrk.data(),iwk.data(),&info);
#else
    dgecon("Infinity-norm",&nw,lhs.data(),&nw,&anorm_8,&this->rcond,wrk.data(),iwk.data(),&info);
#endif
    // Let's compute cond, which is the product of diagonal elements
    assert( info == 0 );

    dgetrs("No transpose",&nw,&ONE,lhs.data(),&nw,ipiv.data(),rhs.data(),&nw,&info);

    for (int w = 0; w < nw; ++w)
    {
        this->W[w] = rhs[w];
    }

    // Given V and W, compute I
    for (int i = 0; i < ni; ++i)
    {
        QPair<int,int> ends = this->ends(i);
        if (ends.first < nv)
            this->I[i] = - this->Sigma[i] * this->V[ends.first];
        else
            this->I[i] = - this->Sigma[i] * this->W[ends.first - nv];

        if (ends.second < nv)
            this->I[i] += this->Sigma[i] * this->V[ends.second];
        else
            this->I[i] += this->Sigma[i] * this->W[ends.second - nv];
    }
}

// Compute using banded matrix
void Percol2D::computeOld()
//void Percol2D::compute_general()
{
    int nv = this->nV(); // number of defined nodes
    int nw = this->nW(); // number of undefined nodes
    int ni = this->nI(); // number of current links

    // Build band-stored LHS matrix = S_W^T SIGMA S_W
    typedef QMap<QPair<int,int>,double> RC_D_map;
    RC_D_map nonz; //here we'll keep only nonzero elements of lhs(r,c)
    for (int i = 0; i < ni; ++i)
    {
        QPair<int,int> ends = this->ends(i);
        int r = ends.first - nv;
        int c = ends.second - nv;
        double sigma = this->Sigma[i];
        if (r >= 0 && c >= 0)
        {
            double sir = this->S(i,r+nv);
            double sic = this->S(i,c+nv);
#define NONZ(r,c) nonz[qMakePair(r,c)]
            NONZ(r,c) +=  sir * sigma * sic;
            NONZ(r,r) +=  sir * sigma * sir;
            NONZ(c,r) +=  sic * sigma * sir;
            NONZ(c,c) +=  sic * sigma * sic;
        }
        else if (r >= 0)
        {
            double sir = this->S(i,r+nv);
            NONZ(r,r) +=  sir * sigma * sir;
        }
        else if (c >= 0)
        {
            double sic = this->S(i,c+nv);
            NONZ(c,c) +=  sic * sigma * sic;
        }
    }
    // Now determine kl and ku - numbers of sub- and super- diagonals.
    Q3MemArray<int> tkl(nw), tku(nw);
    tkl.fill(0);
    tku.fill(0);
    for (RC_D_map::const_iterator e = nonz.constBegin(); e != nonz.constEnd(); ++e)
    {
        int r = e.key().first;
        int c = e.key().second;
        if (r > c) tkl[c] = r - c;
        if (r < c) tku[c] = c - r;
    }
    int kl = vmax(tkl.begin(),tkl.end());
    int ku = vmax(tku.begin(),tku.end());

    // Now prepare the lhs matrix with banded storage
    int lhs_rows = kl + ku + 1 + kl; // last kl is for LU factorization with dgbtrf
    Q3MemArray<double> lhs( lhs_rows * nw );
    lhs.fill(0);
    for (RC_D_map::const_iterator e = nonz.constBegin(); e != nonz.constEnd(); ++e)
    {
        int r = e.key().first;
        int c = e.key().second;
        //aij is stored in ab(kl+ku+1+i-j,j) for max(1,j-ku) ? i ? min(n,j+kl).
#undef LHS
#define LHS(r,c) lhs[kl + ku + r - c + c*lhs_rows]
        LHS(r,c) = e.data();
    }

    // Build temp vector = SIGMA S_V V
    Q3MemArray<double> t( ni );
    t.fill(0.0);
    for (int i = 0; i < ni; ++i)
    {
        for (int v = 0; v < nv; ++v)
        {
            if (this->S(i,v) == 0) continue;
            t.at(i) += this->S(i,v) * this->V[v] * this->Sigma[i];
        }
    }

    // Build right hand side: rhs = -S(...) SIGMA S_V V
    Q3MemArray<double> rhs( nw );
    rhs.fill(0.0);

    for (int w = 0; w < nw; ++w)
    {
        for (int i = 0; i < ni; ++i)
        {
            if (S(i,nv + w) == 0) continue;
            rhs[ w ] -= S(i,nv + w) * t[i];
        }
    }

    int ONE = 1;
    Q3MemArray<int> ipiv( nw );

    // Before factoring with dgbtrf we compute anorm, used later to estimate rcond
//#define WANT_1NORM_RCOND 1
#if WANT_1NORM_RCOND
    double anorm_1 = 0;
    for (int c = 0; c < nw; ++c)
    {
        double colnorm = 0;
        for (int r = 0; r < lhs_rows; ++r)
        {
            colnorm += fabs(LHS(r,c));
        }
        if (colnorm > anorm_1) anorm_1 = colnorm;
    }
#else
    double anorm_8 = 0;
    for (int r = 0; r < lhs_rows; ++r)
    {
        double rownorm = 0;
        for (int c = 0; c < nw; ++c)
        {
            rownorm += fabs(LHS(r,c));
        }
        if (rownorm > anorm_8) anorm_8 = rownorm;
    }
#endif

    int info;
    Q3MemArray<double> lhs_saved = lhs.copy();
    dgbtrf(&nw,&nw,&kl,&ku, lhs.data(),&lhs_rows,ipiv.data(),&info);

    if (info > 0)
    {
        // The system is degenerate, so we'll only solve part of it
        QString msg;
        msg.sprintf("Factorization failed with info = %i (nw=%i)",info,nw);
        switch( QMessageBox::warning( 0, "LU failed",
            msg,"Continue", "Abort", NULL, 0, 0) )
        {
        case 0: // Continue
            return;
        case 1: // Abort
            exit(1);
        }
    }
    if (0) 
    {
        QFile f("tmp.txt");
        f.open(QIODevice::WriteOnly);
        Q3TextStream ts(&f);
        for (int i=0; i < nw; ++i)
            ts << i << " " << ipiv[i] << "\n";
        f.close();
    }

    Q3MemArray<double> wrk( 3*nw );
    Q3MemArray<int> iwk( nw );
#if WANT_1NORM_RCOND
    dgbcon("1-Norm",&nw,&kl,&ku,lhs.data(),&lhs_rows,ipiv.data(),&anorm_1,&this->rcond,
           wrk.data(), iwk.data(), &info);
#else
    dgbcon("Inf-Norm",&nw,&kl,&ku,lhs.data(),&lhs_rows,ipiv.data(),&anorm_8,&this->rcond,
           wrk.data(), iwk.data(), &info);
#endif
    // Let's compute cond, which is the product of diagonal elements
    assert( info == 0 );

    dgbtrs("No transpose",&nw,&kl,&ku,&ONE,lhs.data(),&lhs_rows,ipiv.data(),rhs.data(),&nw,&info);

    for (int w = 0; w < nw; ++w)
    {
        this->W[w] = rhs[w];
    }

    // Given V and W, compute I
    for (int i = 0; i < ni; ++i)
    {
        QPair<int,int> ends = this->ends(i);
        if (ends.first < nv)
            this->I[i] = - this->Sigma[i] * this->V[ends.first];
        else
            this->I[i] = - this->Sigma[i] * this->W[ends.first - nv];

        if (ends.second < nv)
            this->I[i] += this->Sigma[i] * this->V[ends.second];
        else
            this->I[i] += this->Sigma[i] * this->W[ends.second - nv];
    }
}

// Compute using banded matrix
void Percol2D::compute()
//void Percol2D::compute_general()
{
    int nv = this->nV(); // number of defined nodes
    int nw = this->nW(); // number of undefined nodes
    int ni = this->nI(); // number of current links

    // Build band-stored LHS matrix = S_W^T SIGMA S_W
    typedef QMap<QPair<int,int>,double> RC_D_map;
    RC_D_map nonz; //here we'll keep only nonzero elements of lhs(r,c)
    for (int i = 0; i < ni; ++i)
    {
        QPair<int,int> ends = this->ends(i);
        int r = ends.first - nv;
        int c = ends.second - nv;
        double sigma = this->Sigma[i];
        if (r >= 0 && c >= 0)
        {
            double sir = this->S(i,r+nv);
            double sic = this->S(i,c+nv);
#define NONZ(r,c) nonz[qMakePair(r,c)]
            NONZ(r,c) +=  sir * sigma * sic;
            NONZ(r,r) +=  sir * sigma * sir;
            NONZ(c,r) +=  sic * sigma * sir;
            NONZ(c,c) +=  sic * sigma * sic;
        }
        else if (r >= 0)
        {
            double sir = this->S(i,r+nv);
            NONZ(r,r) +=  sir * sigma * sir;
        }
        else if (c >= 0)
        {
            double sic = this->S(i,c+nv);
            NONZ(c,c) +=  sic * sigma * sic;
        }
    }
    // Now determine kl and ku - numbers of sub- and super- diagonals.
    Q3MemArray<int> tkl(nw), tku(nw);
    tkl.fill(0);
    tku.fill(0);
    for (RC_D_map::const_iterator e = nonz.constBegin(); e != nonz.constEnd(); ++e)
    {
        int r = e.key().first;
        int c = e.key().second;
        if (r > c) tkl[c] = r - c;
        if (r < c) tku[c] = c - r;
    }
    int kl = vmax(tkl.begin(),tkl.end());
    int ku = vmax(tku.begin(),tku.end());

    // Now prepare the lhs matrix with banded storage
    int lhs_rows = /*kl +*/ ku + 1 + kl; // last kl is for LU factorization with dgbtrf
    Q3MemArray<double> lhs( lhs_rows * nw );
    lhs.fill(0);
    for (RC_D_map::const_iterator e = nonz.constBegin(); e != nonz.constEnd(); ++e)
    {
        int r = e.key().first;
        int c = e.key().second;
        //aij is stored in ab(kl+ku+1+i-j,j) for max(1,j-ku) ? i ? min(n,j+kl).
#undef LHS
#define LHS(r,c) lhs[/*kl +*/ ku + r - c + c*lhs_rows]
        LHS(r,c) = e.data();
    }

    // Build temp vector = SIGMA S_V V
    Q3MemArray<double> t( ni );
    t.fill(0.0);
    for (int i = 0; i < ni; ++i)
    {
        for (int v = 0; v < nv; ++v)
        {
            if (this->S(i,v) == 0) continue;
            t.at(i) += this->S(i,v) * this->V[v] * this->Sigma[i];
        }
    }

    // Build right hand side: rhs = -S(...) SIGMA S_V V
    Q3MemArray<double> rhs( nw );
    rhs.fill(0.0);

    for (int w = 0; w < nw; ++w)
    {
        for (int i = 0; i < ni; ++i)
        {
            if (S(i,nv + w) == 0) continue;
            rhs[ w ] -= S(i,nv + w) * t[i];
        }
    }

    int ONE = 1;
    Q3MemArray<int> ipiv( nw );
    int info;
    Q3MemArray<double> lhs_saved = lhs.copy();
    Q3MemArray<double> wrk( 3*nw );
    Q3MemArray<int> iwk( nw );
    Q3MemArray<double> dr( nw );
    Q3MemArray<double> dc( nw );
    char equed;
    int afb_rows = 1 + kl + ku + kl;
    Q3MemArray<double> afb( afb_rows*nw );
    dgbsvx("Equilibrate","No transpose",&nw,&kl,&ku,&ONE,
        lhs_saved.data(),&lhs_rows,
        afb.data(), &afb_rows, 
        ipiv.data(), 
        &equed, dr.data(), dc.data(), 
        rhs.data(), &nw,
        this->W.data(), &nw,
        &this->rcond,
        &ferr, &berr, wrk.data(), iwk.data(), &info);

    // Given V and W, compute I
    for (int i = 0; i < ni; ++i)
    {
        QPair<int,int> ends = this->ends(i);
        if (ends.first < nv)
            this->I[i] = - this->Sigma[i] * this->V[ends.first];
        else
            this->I[i] = - this->Sigma[i] * this->W[ends.first - nv];

        if (ends.second < nv)
            this->I[i] += this->Sigma[i] * this->V[ends.second];
        else
            this->I[i] += this->Sigma[i] * this->W[ends.second - nv];
    }
//----------------------------------
    double imax = -1e300;
    double q;
    for (int i = 0; i < ni; ++i)
    {   
        q = this->I[i];
        if (fabs(q) > imax) 
        {
            imax = fabs(q); 
        }
    }

    double deltai_max = 0;
    double I1,I2;
    for (int i=0; i < ni; ++i) 
    {
        QPair<int,int> ends = this->ends(i);
        int from = ends.first;
        int to   = ends.second;
        QPair<double,double> xy0 = this->xy(from);
        QPair<double,double> xy1 = this->xy(to);
        if(xy0.first==0 && xy1.first==0&&
            (xy0.second==0&&xy1.second==1||xy0.second==1&&xy1.second==0)) I1=this->I[i];
        if(xy0.second==0 && xy1.second==0&&
            (xy0.first==0&&xy1.first==1||xy0.first==1&&xy1.first==0)) I2=this->I[i];
    }
       conductivity = fabs(I1+I2)/2;
//--------------CAPACITY----------------
       {     
        Q3MemArray<double> t( nv+nw );
        int nt;
        t.fill(0.0);
        for (int i = 0; i < ni; ++i)
        {   
            double q = fabs(this->I[i]);
            QPair<int,int> ends = this->ends(i);
            t[ends.first] += q;
            t[ends.second] += q;
        }
        nt=0;
        for (int v = 0; v < nv + nw; ++v)  
        {
            if(t[v] > imax * 1e-3) 
                nt++;
        }
        capacity = double(nt)/double(nv+nw);
       }
//--------------------------

    for (int w = 0; w < nw; ++w)
    {
        Q3MemArray<int> from = this->from(nv+w);
        Q3MemArray<int> to = this->to(nv+w);
        double total_i = 0;
        for (size_t i = 0; i < from.size(); ++i)
        {
            total_i +=  this->I[ from[i] ];
        }
        for (size_t i = 0; i < to.size(); ++i)
        {
            total_i -=  this->I[ to[i] ];
        }

        if (fabs(total_i) > deltai_max) deltai_max = fabs(total_i);
    }
    // compute color scale, based on deltai_max
        deltaI = deltai_max/imax;
}

/************************************************************************
* PercolRect
************************************************************************/

PercolRect::PercolRect(int _rows, int _cols) : rows(_rows), cols(_cols)
{
    V.resize(2);
    W.resize(rows*cols - 2);
    I.resize((rows-1)*cols + rows*(cols-1));
    Sigma.resize(I.size());
    V[0] = 1.0;
    V[1] = -1.0;
//    selectSigma(combo->setCurrentItem());
//    Sigma.fill(1.0);
    //Sigma[ 5 ] = 0.0;
    //Sigma[ 11 ] = 0.0;
    //Sigma[ 20 ] = 0.0;
}

PercolRect::~PercolRect()
{
}

struct RectHelper
{
    const int rows, cols;
    RectHelper(int _rows,int _cols) : rows(_rows), cols(_cols) {}

    // compute number v of the node located at r,c
    int v(int r, int c) 
    { 
        int i = r + rows * c;
        if (r == 0 && c == 0) return 0;
        if (r == rows-1 && c == cols-1) return 1;
        return i + 1;
    }
    // compute location r,c of node number v
    QPair<int,int> rc(int v) 
    { 
        if (v == 0) return qMakePair( 0, 0 );
        if (v == 1) return qMakePair( rows-1, cols-1 );
        v -= 1;
        return qMakePair( v % rows, v / rows ); 
    }
    // compute number ibu of the bottom-up edge coming from node at r,c
    int ibu(int r, int c) 
    { 
        return r*cols + c; //! 
    }
    // compute location r,c of the bottom end of the bottom-up edge i
    QPair<int,int> rcbu(int i) 
    { 
        return qMakePair( i / cols, i % cols );
    }
    // compute number ilr of the left-right edge coming from node at r,c
    int ilr(int r, int c) 
    { 
        return (rows-1)*cols + r + rows*c; 
    }
    // compute location r,c of the left end of the left-right edge i
    QPair<int,int> rclr(int i) 
    {  
        i -= (rows-1)*cols;
        return qMakePair( i % rows, i / rows ); 
    }
};

QPair<int,int> PercolRect::ends(int i) const
{
    RectHelper h(rows,cols);
    if (i < (rows-1)*cols) /* bottom-up edge */
    {
        QPair<int,int> rc = h.rcbu(i);
        int v_from = h.v( rc.first, rc.second );
        int v_to = h.v( rc.first + 1, rc.second );
        return qMakePair( v_from, v_to );
    }
    else /* left-to-right edge */
    {
        QPair<int,int> rc = h.rclr(i);
        int v_from = h.v( rc.first, rc.second );
        int v_to = h.v( rc.first, rc.second + 1 );
        return qMakePair( v_from, v_to );
    }
}

Q3MemArray<int> PercolRect::from(int v) const
{
    RectHelper h(rows,cols);
    QPair<int,int> rc = h.rc(v);
    int r = rc.first;
    int c = rc.second;
    int a, b;

    Q3MemArray<int> res;

    if (r < rows-1 && c < cols-1)
    {
        res.resize(2);
        res[0] = a = h.ibu(r,c);
        res[1] = b = h.ilr(r,c);
    }
    else if (r == rows-1 && c < cols-1)
    {
        res.resize(1);
        res[0] = a = h.ilr(r,c);
    }
    else if (r < rows-1 && c == cols-1)
    {
        res.resize(1);
        res[0] = a = h.ibu(r,c);
    }
    else
    {
        res.resize(0);
    }
    return res;
}

Q3MemArray<int> PercolRect::to(int v) const
{
    RectHelper h(rows,cols);
    QPair<int,int> rc = h.rc(v);
    int r = rc.first;
    int c = rc.second;
    int a, b;

    Q3MemArray<int> res;

    if (r > 0 && c > 0)
    {
        res.resize(2);
        res[0] = a = h.ibu(r-1,c);
        res[1] = b = h.ilr(r,c-1);
    }
    else if (r == 0 && c > 0)
    {
        res.resize(1);
        res[0] = a = h.ilr(r,c-1);
    }
    else if (r > 0 && c == 0)
    {
        res.resize(1);
        res[0] = a = h.ibu(r-1,c);
    }
    else
    {
        res.resize(0);
    }
    return res;
}

int PercolRect::S(int i, int v) const
{
    QPair<int,int> rc = PercolRect::ends(i);
    if (v == rc.first) return -1;
    if (v == rc.second) return +1;
    return 0;
}

QPair<double,double> PercolRect::xy(int v) const
{
    RectHelper h(rows,cols);
    QPair<int,int> rc = h.rc(v);
    return qMakePair(double(rc.second), double(rc.first));
}

double PercolRect::xmax() const { return cols-1; }
double PercolRect::ymax() const { return rows-1; }
double PercolRect::xmin() const { return 0; }
double PercolRect::ymin() const { return 0; }

// returns the index of I with maximum I^2*R
int Percol2D::index_of_Rcr() const
{
    double IV_max = -1e300;
    int i_max = 0;
    for (int i = 0; i < nI(); ++i)
    {
	double I_i = I[i];
	QPair<int,int> ends_i = ends(i);
	double V_i = ends_i.first < nV()
	    ? V[ends_i.first]
	    : W[ends_i.first - nV()];
	V_i -= ends_i.second < nV()
	    ? V[ends_i.second]
	    : W[ends_i.second - nV()];
	double IV_i = fabs(I_i * V_i);
	if (IV_i > IV_max)
	{
	    IV_max = IV_i;
	    i_max = i;
	}
    }
    return i_max;
}
