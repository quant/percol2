#include "percol2d.h"
#include "mkl.h"
#include <cassert>
#include <cmath>
#include <map>
#include <utility>

#define DATAOF(array) &array[0]
#define MYMAP  std::map

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
{
    int nv = this->nV(); // number of defined nodes
    int nw = this->nW(); // number of undefined nodes
    int ni = this->nI(); // number of current links

    // Build LHS matrix = S_W^T SIGMA S_W
    MYARRAY<double> lhs( nw*nw );
    lhs = 0.0; //lhs.fill(0.0);

    for (int i = 0; i < ni; ++i)
    {
        MYPAIR<int,int> ends = this->ends(i);
        int r = ends.first - nv;
        int c = ends.second - nv;
        double sigma = this->Sigma[i];
        if (r >= 0 && c >= 0)
        {
            double sir = this->S(i,r+nv);
            double sic = this->S(i,c+nv);
#define LHS(r,c) lhs[((r)+nw*(c))]
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
    MYARRAY<double> t( ni );
    t = 0.0;
    for (int i = 0; i < ni; ++i)
    {
        for (int v = 0; v < nv; ++v)
        {
            if (this->S(i,v) == 0) continue;
            t[i] += this->S(i,v) * this->V[v] * this->Sigma[i];
        }
    }

    // Build right hand side: rhs = -S(...) SIGMA S_V V
    MYARRAY<double> rhs( nw );
    rhs = 0.0;

    for (int w = 0; w < nw; ++w)
    {
        for (int i = 0; i < ni; ++i)
        {
            if (S(i,nv + w) == 0) continue;
            rhs[ w ] -= S(i,nv + w) * t[i];
        }
    }

    int ONE = 1;
    MYARRAY<int> ipiv( nw );

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
    dgetrf(&nw,&nw,DATAOF(lhs),&nw,DATAOF(ipiv),&info);
    if (info > 0)
    {
        // The system is degenerate, so we'll only solve part of it
        printf("%s:%i: Factorization failed with info = %i (nw=%i)",
            __FILE__,__LINE__,info,nw);
        return;
    }
    MYARRAY<double> wrk( 4*nw );
    MYARRAY<int> iwk( nw );
#if WANT_1NORM_RCOND
    dgecon("1-Norm",&nw,lhs.data(),&nw,&anorm_1,&this->rcond,wrk.data(),iwk.data(),&info);
#else
    dgecon("Infinity-norm",&nw,DATAOF(lhs),&nw,&anorm_8,&this->rcond,DATAOF(wrk),DATAOF(iwk),&info);
#endif
    // Let's compute cond, which is the product of diagonal elements
    assert( info == 0 );

    dgetrs("No transpose",&nw,&ONE,DATAOF(lhs),&nw,DATAOF(ipiv),DATAOF(rhs),&nw,&info);

    for (int w = 0; w < nw; ++w)
    {
        this->W[w] = rhs[w];
    }

    // Given V and W, compute I
    for (int i = 0; i < ni; ++i)
    {
        MYPAIR<int,int> ends = this->ends(i);
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
    typedef MYMAP<MYPAIR<int,int>,double> RC_D_map;
    RC_D_map nonz; //here we'll keep only nonzero elements of lhs(r,c)
    for (int i = 0; i < ni; ++i)
    {
        MYPAIR<int,int> ends = this->ends(i);
        int r = ends.first - nv;
        int c = ends.second - nv;
        double sigma = this->Sigma[i];
        if (r >= 0 && c >= 0)
        {
            double sir = this->S(i,r+nv);
            double sic = this->S(i,c+nv);
#define NONZ(r,c) nonz[MYPAIR<int,int>(r,c)]
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
    MYARRAY<int> tkl(nw), tku(nw);
    tkl = 0;
    tku = 0;
    for (RC_D_map::const_iterator e = nonz.begin(); e != nonz.end(); ++e)
    {
        MYPAIR<int,int> key = e->first;
        int r = key.first;
        int c = key.second;
        if (r > c) tkl[c] = r - c;
        if (r < c) tku[c] = c - r;
    }
    int kl = vmax( &tkl[0], &tkl[tkl.size()] );
    int ku = vmax( &tku[0], &tku[tku.size()] );

    // Now prepare the lhs matrix with banded storage
    int lhs_rows = kl + ku + 1 + kl; // last kl is for LU factorization with dgbtrf
    MYARRAY<double> lhs( lhs_rows * nw );
    lhs = 0.0;
    for (RC_D_map::const_iterator e = nonz.begin(); e != nonz.end(); ++e)
    {
        MYPAIR<int,int> key = e->first;
        int r = key.first;
        int c = key.second;
        //aij is stored in ab(kl+ku+1+i-j,j) for max(1,j-ku) ? i ? min(n,j+kl).
#undef LHS
#define LHS(r,c) lhs[kl + ku + r - c + c*lhs_rows]
        LHS(r,c) = e->second;
    }

    // Build temp vector = SIGMA S_V V
    MYARRAY<double> t( ni );
    t = 0.0;
    for (int i = 0; i < ni; ++i)
    {
        for (int v = 0; v < nv; ++v)
        {
            if (this->S(i,v) == 0) continue;
            t[i] += this->S(i,v) * this->V[v] * this->Sigma[i];
        }
    }

    // Build right hand side: rhs = -S(...) SIGMA S_V V
    MYARRAY<double> rhs( nw );
    rhs = 0.0;

    for (int w = 0; w < nw; ++w)
    {
        for (int i = 0; i < ni; ++i)
        {
            if (S(i,nv + w) == 0) continue;
            rhs[ w ] -= S(i,nv + w) * t[i];
        }
    }

    int ONE = 1;
    MYARRAY<int> ipiv( nw );

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
    MYARRAY<double> lhs_saved = lhs; //Was lhs.copy(). Stupid STL doesn't support .copy()
    dgbtrf(&nw,&nw,&kl,&ku, DATAOF(lhs),&lhs_rows,DATAOF(ipiv),&info);

    if (info > 0)
    {
        // The system is degenerate, so we'll only solve part of it
        printf("%s:%i: Factorization failed with info = %i (nw=%i)\n",
            __FILE__,__LINE__,info,nw);
        return;
    }
    MYARRAY<double> wrk( 3*nw );
    MYARRAY<int> iwk( nw );
#if WANT_1NORM_RCOND
    dgbcon("1-Norm",&nw,&kl,&ku,lhs.data(),&lhs_rows,ipiv.data(),&anorm_1,&this->rcond,
           wrk.data(), iwk.data(), &info);
#else
    dgbcon("Inf-Norm",&nw,&kl,&ku,DATAOF(lhs),&lhs_rows,DATAOF(ipiv),&anorm_8,&this->rcond,
           DATAOF(wrk), DATAOF(iwk), &info);
#endif
    // Let's compute cond, which is the product of diagonal elements
    assert( info == 0 );

    dgbtrs("No transpose",&nw,&kl,&ku,&ONE,DATAOF(lhs),&lhs_rows,DATAOF(ipiv),DATAOF(rhs),&nw,&info);

    for (int w = 0; w < nw; ++w)
    {
        this->W[w] = rhs[w];
    }

    // Given V and W, compute I
    for (int i = 0; i < ni; ++i)
    {
        MYPAIR<int,int> ends = this->ends(i);
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
    typedef MYMAP<MYPAIR<int,int>,double> RC_D_map;
    RC_D_map nonz; //here we'll keep only nonzero elements of lhs(r,c)
    for (int i = 0; i < ni; ++i)
    {
        MYPAIR<int,int> ends = this->ends(i);
        int r = ends.first - nv;
        int c = ends.second - nv;
        double sigma = this->Sigma[i];
        if (r >= 0 && c >= 0)
        {
            double sir = this->S(i,r+nv);
            double sic = this->S(i,c+nv);
#undef NONZ
#define NONZ(r,c) nonz[MYPAIR<int,int>(r,c)]
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
    MYARRAY<int> tkl(nw), tku(nw);
    tkl = 0;
    tku = 0;
    for (RC_D_map::const_iterator e = nonz.begin(); e != nonz.end(); ++e)
    {
        MYPAIR<int,int> key = e->first;
        int r = key.first;
        int c = key.second;
        if (r > c) tkl[c] = r - c;
        if (r < c) tku[c] = c - r;
    }
    int kl = vmax( &tkl[0], &tkl[tkl.size()] );
    int ku = vmax( &tku[0], &tku[tku.size()] );

    // Now prepare the lhs matrix with banded storage
    int lhs_rows = /*kl +*/ ku + 1 + kl; // last kl is for LU factorization with dgbtrf
    MYARRAY<double> lhs( lhs_rows * nw );
    lhs = 0.0;
    for (RC_D_map::const_iterator e = nonz.begin(); e != nonz.end(); ++e)
    {
        MYPAIR<int,int> key = e->first;
        int r = key.first;
        int c = key.second;
        //aij is stored in ab(kl+ku+1+i-j,j) for max(1,j-ku) ? i ? min(n,j+kl).
#undef LHS
#define LHS(r,c) lhs[/*kl +*/ ku + r - c + c*lhs_rows]
        LHS(r,c) = e->second;
    }

    // Build temp vector = SIGMA S_V V
    MYARRAY<double> t( ni );
    t = 0.0;
    for (int i = 0; i < ni; ++i)
    {
        for (int v = 0; v < nv; ++v)
        {
            if (this->S(i,v) == 0) continue;
            t[i] += this->S(i,v) * this->V[v] * this->Sigma[i];
        }
    }

    // Build right hand side: rhs = -S(...) SIGMA S_V V
    MYARRAY<double> rhs( nw );
    rhs = 0.0;

    for (int w = 0; w < nw; ++w)
    {
        for (int i = 0; i < ni; ++i)
        {
            if (S(i,nv + w) == 0) continue;
            rhs[ w ] -= S(i,nv + w) * t[i];
        }
    }

    int ONE = 1;
    MYARRAY<int> ipiv( nw );
    int info;
    MYARRAY<double> lhs_saved = lhs; //I mean deep copy!
    MYARRAY<double> wrk( 3*nw );
    MYARRAY<int> iwk( nw );
    MYARRAY<double> dr( nw );
    MYARRAY<double> dc( nw );
    char equed;
    int afb_rows = 1 + kl + ku + kl;
    MYARRAY<double> afb( afb_rows*nw );
    dgbsvx("Equilibrate","No transpose",&nw,&kl,&ku,&ONE,
        DATAOF(lhs_saved),&lhs_rows,
        DATAOF(afb), &afb_rows,
        DATAOF(ipiv),
        &equed, DATAOF(dr), DATAOF(dc),
        DATAOF(rhs), &nw,
        DATAOF(this->W), &nw,
        &this->rcond,
        &ferr, &berr, DATAOF(wrk), DATAOF(iwk), &info);

    // Given V and W, compute I
    for (int i = 0; i < ni; ++i)
    {
        MYPAIR<int,int> ends = this->ends(i);
        if (ends.first < nv)
            this->I[i] = - this->Sigma[i] * this->V[ends.first];
        else
            this->I[i] = - this->Sigma[i] * this->W[ends.first - nv];

        if (ends.second < nv)
            this->I[i] += this->Sigma[i] * this->V[ends.second];
        else
            this->I[i] += this->Sigma[i] * this->W[ends.second - nv];
    }
//-------voltage difference
    double V_1,V_2,V12;
    this->difV = 0.0;
    for (int i = 0; i < ni; ++i)
    {
        MYPAIR<int,int> ends_i = this->ends(i);
        if(ends_i.first < nv)
            V_1=this->V[ends_i.first];
        else
            V_1=this->W[ends_i.first-nv];
        if(ends_i.second < nv)
            V_2=this->V[ends_i.second];
        else
            V_2=this->W[ends_i.second-nv];
        V12=(V_1-V_2);
        this->difV[i]=(V_1-V_2);
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
    double I1=0,I2=0;
    for (int i=0; i < ni; ++i)
    {
        MYPAIR<int,int> ends = this->ends(i);
        int from = ends.first;
        int to   = ends.second;
        MYPAIR<double,double> xy0 = this->xy(from);
        MYPAIR<double,double> xy1 = this->xy(to);
        if(xy0.first==0 && xy1.first==0&&
            (xy0.second==0&&xy1.second==1||xy0.second==1&&xy1.second==0)) I1=this->I[i];
        if(xy0.second==0 && xy1.second==0&&
            (xy0.first==0&&xy1.first==1||xy0.first==1&&xy1.first==0)) I2=this->I[i];
    }
    conductivity = fabs(I1+I2)/2;
//-------Joule heat
//TODO: replace this block with some simpler stuff
    double IdVmax = -1e300;
    double ImaxV = conductivity*4.;
    {
        double V_1,V_2,V12;
        this->IdifV.fill(0.0);
        double q;
        for (int i = 0; i < ni; ++i)
        {
            MYPAIR<int,int> ends_i = this->ends(i);
            if(ends_i.first < nv)
                V_1=this->V[ends_i.first];
            else
                V_1=this->W[ends_i.first-nv];
            if(ends_i.second < nv)
                V_2=this->V[ends_i.second];
            else
                V_2=this->W[ends_i.second-nv];
            V12=(V_1-V_2);
            q=fabs(this->I[i]*(V_1-V_2));
            this->IdifV[i]=q/ImaxV;
            if (q > IdVmax)
            {
                IdVmax = q;
            }
        }
    }
//--------------Current Fraction----------------
    {
        int nt=0;
        int nS=0;
        for (int i = 0; i < ni; ++i)
        {
            double q = fabs(this->I[i]);
            if(this->Sigma[i]!=100){
                nS++;
            if(q > imax * 1e-3) nt++;
            }
        }
//   fractionI = double(nt)/double(ni);
//   capacity = double(nt)/double(nS);
    }
//--------------Joule heat Fraction----------------
    {
        int nt=0;
        for (int i = 0; i < ni; ++i)
        {
            double q = fabs(this->IdifV[i]);
//            if(q > IdVmax * 0.1) nt++;
            if(q > IdVmax/ImaxV * 0.1) nt++;
        }
//   fractionIdV = double(nt)/double(ni);
        capacity = double(nt);///double(ni);
    }

//--------------------------

    for (int w = 0; w < nw; ++w)
    {
        MYARRAY<int> from = this->from(nv+w);
        MYARRAY<int> to = this->to(nv+w);
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
    difV.resize( I.size() );
    IdifV.resize(I.size());
    Sigma.resize(I.size());
    V[0] = 1.0;
    V[1] = -1.0;

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
    MYPAIR<int,int> rc(int v)
    {
        if (v == 0) return MYPAIR<int,int>( 0, 0 );
        if (v == 1) return MYPAIR<int,int>( rows-1, cols-1 );
        v -= 1;
        return MYPAIR<int,int>( v % rows, v / rows );
    }
    // compute number ibu of the bottom-up edge coming from node at r,c
    int ibu(int r, int c)
    {
        return r*cols + c; //!
    }
    // compute location r,c of the bottom end of the bottom-up edge i
    MYPAIR<int,int> rcbu(int i)
    {
        return MYPAIR<int,int>( i / cols, i % cols );
    }
    // compute number ilr of the left-right edge coming from node at r,c
    int ilr(int r, int c)
    {
        return (rows-1)*cols + r + rows*c;
    }
    // compute location r,c of the left end of the left-right edge i
    MYPAIR<int,int> rclr(int i)
    {
        i -= (rows-1)*cols;
        return MYPAIR<int,int>( i % rows, i / rows );
    }
};

MYPAIR<int,int> PercolRect::ends(int i) const
{
    RectHelper h(rows,cols);
    if (i < (rows-1)*cols) /* bottom-up edge */
    {
        MYPAIR<int,int> rc = h.rcbu(i);
        int v_from = h.v( rc.first, rc.second );
        int v_to = h.v( rc.first + 1, rc.second );
        return MYPAIR<int,int>( v_from, v_to );
    }
    else /* left-to-right edge */
    {
        MYPAIR<int,int> rc = h.rclr(i);
        int v_from = h.v( rc.first, rc.second );
        int v_to = h.v( rc.first, rc.second + 1 );
        return MYPAIR<int,int>( v_from, v_to );
    }
}

MYARRAY<int> PercolRect::from(int v) const
{
    RectHelper h(rows,cols);
    MYPAIR<int,int> rc = h.rc(v);
    int r = rc.first;
    int c = rc.second;
    int a, b;

    MYARRAY<int> res;

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

MYARRAY<int> PercolRect::to(int v) const
{
    RectHelper h(rows,cols);
    MYPAIR<int,int> rc = h.rc(v);
    int r = rc.first;
    int c = rc.second;
    int a, b;

    MYARRAY<int> res;

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
    MYPAIR<int,int> rc = PercolRect::ends(i);
    if (v == rc.first) return -1;
    if (v == rc.second) return +1;
    return 0;
}

MYPAIR<double,double> PercolRect::xy(int v) const
{
    RectHelper h(rows,cols);
    MYPAIR<int,int> rc = h.rc(v);
    return MYPAIR<double,double>(double(rc.second), double(rc.first));
}

int PercolRect::vnode(double x,double y) const
{
    RectHelper h(rows,cols);
    int r = int(y > 0 ? y+0.5 : y-0.5); //nearest int
    int c = int(x > 0 ? x+0.5 : x-0.5); //nearest int
    return h.v(r,c);
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
        MYPAIR<int,int> ends_i = ends(i);
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
#include <QtAlgorithms>

QVector<int> Percol2D::index_for_sorted_W() const
{
    QVector<int> res(nW());
    for (int i=0; i<res.size(); ++i)
    {
        res[i] = i;
    }

    struct MyLessThan
    {
        const MYARRAY<double>& w;
        MyLessThan(const MYARRAY<double>& w_) : w(w_) {}
        bool operator()(int a, int b) const { return w[a] < w[b]; }
    };

    qSort(res.begin(),res.end(),MyLessThan(W));

    return res;
}
QVector<int> Percol2D::index_for_sorted_I() const
{
    QVector<int> res(nI());
    for (int i=0; i<res.size(); ++i)
    {
        res[i] = i;
    }

    struct MyGreaterThan
    {
        const MYARRAY<double>& w;
        MyGreaterThan(const MYARRAY<double>& w_) : w(w_) {}
        bool operator()(int a, int b) const { return fabs(w[a]) >fabs(w[b]); }
    };

    qSort(res.begin(),res.end(),MyGreaterThan(I));

    return res;
}
QVector<int> Percol2D::index_for_sorted_difV() const
{
    QVector<int> res(nI());
    for (int i=0; i<res.size(); ++i)
    {
        res[i] = i;
    }

    struct MyGreaterThan
    {
        const MYARRAY<double>& w;
        MyGreaterThan(const MYARRAY<double>& w_) : w(w_) {}
        bool operator()(int a, int b) const { return fabs(w[a]) > fabs(w[b]); }
    };

    qSort(res.begin(),res.end(),MyGreaterThan(difV));

    return res;
}
QVector<int> Percol2D::index_for_sorted_IdifV() const
{
    QVector<int> res(nI());
    for (int i=0; i<res.size(); ++i)
    {
        res[i] = i;
    }

    struct MyGreaterThan
    {
        const MYARRAY<double>& w;
        MyGreaterThan(const MYARRAY<double>& w_) : w(w_) {}
        bool operator()(int a, int b) const { return fabs(w[a]) > fabs(w[b]); }
    };

    qSort(res.begin(),res.end(),MyGreaterThan(IdifV));

    return res;
}
QVector<int> Percol2D::index_for_sorted_Sigma() const
{
    QVector<int> res(nI());
    for (int i=0; i<res.size(); ++i)
    {
        res[i] = i;
    }
    struct MyGreaterThan
    {
        const MYARRAY<double>& w;
        MyGreaterThan(const MYARRAY<double>& w_) : w(w_) {}
        bool operator()(int a, int b) const { return w[a] > w[b]; }
    };

    qSort(res.begin(),res.end(),MyGreaterThan(Sigma));

    return res;
}
