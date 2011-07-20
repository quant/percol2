#define _CRT_SECURE_NO_WARNINGS 1

#include <mpi.h>

#include "percol2d.h"
#include "mainwindow.h"


double global_Tmin = 0.1, global_Tmax = 5.301, global_dT = 0.1;//1.3;//2.6;//0.2;
double global_Umin = 165., global_Umax = 235.0, global_dU = 5;
double global_U = 235.;//235.;//190;

int global_rows=90;
int global_cols=153;
int global_seed=4;

int main(int argc, char **argv)
{
    MPI_Init( &argc, &argv );

    MainWindow mainWindow;

    std::vector<X_of_T> data;
    FILE *f;
    int mysize, myrank;

    MPI_Comm_size(MPI_COMM_WORLD,&mysize);
    MPI_Comm_rank(MPI_COMM_WORLD,&myrank);
printf("I am %i of %i\n",myrank,mysize);

    double D=(global_Tmax-global_Tmin)/mysize;
    mainWindow.Tmin=global_Tmax-D*(myrank+1);
    mainWindow.Tmax=global_Tmax-D*myrank;
    printf( "I am %i Tmax=%lg Tmin=%lg\n", myrank,  mainWindow.Tmax, mainWindow.Tmin);
    mainWindow.dT=global_dT;
    mainWindow.U=global_U;
    mainWindow.rows=global_rows;
    mainWindow.cols=global_cols;
    mainWindow.seed=global_seed;
    mainWindow.computeRT(data);
   for (int irank=0; irank<mysize; irank++)
   {
        MPI_Barrier(MPI_COMM_WORLD);
        if (irank == myrank)
        {
            f = fopen("GTSa235s14.dat", myrank == 0 ? "w" : "a"); // write new or append

            if (irank == 0)
            {
                fprintf(f,"Data file for cols=%i * rows=%i seed=%i\n",global_cols, global_rows, global_seed);
                fprintf(f,"for U=%lg Tmin=%lg Tmax=%lg\n",global_U, global_Tmin,global_Tmax);

            }
            for (std::vector<X_of_T>::iterator i = data.begin(); i != data.end(); ++i)
            {
                fprintf(f,"%lg %lg\n", i->T, i->G);
            }
            fclose(f);
        }
    }

    MPI_Finalize();
}
