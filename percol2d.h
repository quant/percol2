// Define abstract model of 2D percolation and some realizations of it
#if defined(_WIN32)
#pragma once
#endif

#ifndef PERCOL2D_H_INCLUDED
#define PERCOL2D_H_INCLUDED

#include <q3memarray.h>
#include <qpair.h>

#define MYARRAY  Q3MemArray
#define MYPAIR   QPair
#define MYVECTOR QVector

class Percol2D
{
public:
    Percol2D(void);
    virtual ~Percol2D(void);

    // Electrical properties of the grid
    MYARRAY<double> Sigma;   // nI conductivities (defined)
    MYARRAY<double> I; // nI currents (computed)
    MYARRAY<double> V; // nV defined voltages
    MYARRAY<double> W; // nW computed voltages
    MYARRAY<double> difV; // nI voltage difference (computed)
    MYARRAY<double> IdifV; // nI voltage difference (computed)

    int nV() const { return V.size(); } // number of defined voltage nodes
    int nW() const { return W.size(); } // number of unknown voltage nodes
    int nI() const { return I.size(); } // number of links
    // Defined nodes have numbers 0...nV()-1
    // Unknown nodes have numbers nV()...nV()+nW()-1

    int index_of_Rcr() const; // returns the index of edge with max I*V

    // These functions define geometry and topology of the grid.
    // They are defined by the implementation of the model
    virtual int S(int i,int v) const = 0;
    virtual MYPAIR<int,int> ends(int i) const = 0;
    virtual MYARRAY<int> from(int v) const = 0;
    virtual MYARRAY<int> to(int v) const = 0;
    virtual MYPAIR<double,double> xy(int v) const = 0;
    virtual double xmax() const = 0;
    virtual double xmin() const = 0;
    virtual double ymax() const = 0;
    virtual double ymin() const = 0;

    // Compute I and W
    void compute(); // using banded matrix  (~1/rows of NxN storage)
    void computeOld(); // using banded matrix  (~1/rows of NxN storage)
    void compute_general(); // using general matrix (NxN storage)

    // Reciprocal condition number of the last computation
    double rcond, ferr, berr, deltaI, conductivity,capacity;

    MYVECTOR<int> index_for_sorted_W() const;
    MYVECTOR<int> index_for_sorted_difV() const;
    MYVECTOR<int> index_for_sorted_IdifV() const;
    MYVECTOR<int> index_for_sorted_I() const;
    MYVECTOR<int> index_for_sorted_Sigma() const;

};

// Define a rectangular percolation grid with source drain at opposite
// corners
class PercolRect : public Percol2D
{
    int rows, cols;
public:
    PercolRect(int rows, int cols);
    virtual ~PercolRect();

    virtual int S(int i,int v) const;
    virtual MYPAIR<int,int> ends(int i) const;
    virtual MYARRAY<int> from(int v) const;
    virtual MYARRAY<int> to(int v) const;
    virtual MYPAIR<double,double> xy(int v) const;
    virtual int vnode(double x,double y) const;
    virtual double xmax() const;
    virtual double xmin() const;
    virtual double ymax() const;
    virtual double ymin() const;
};

#endif /* PERCOL2D_H_INCLUDED */
