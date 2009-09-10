#define _CRT_SECURE_NO_WARNINGS 1

#include <mpi.h>

#include "percol2d.h"
#include "mainwindow.h"


double global_Tmin = 0.1, global_Tmax = 5.301, global_dT = 0.1;
double global_Umin = 180, global_Umax = 300, global_dU = 5;
double global_U = 200;
int global_rows=150;
int global_cols=253;
int global_seed=0;

int main(int argc, char **argv)
{
    MPI_Init( &argc, &argv );

    MainWindow mainWindow;

    std::vector<X_of_T> data;
    FILE *f;
    int mysize, myrank;

    MPI_Comm_size(MPI_COMM_WORLD,&mysize);
    MPI_Comm_rank(MPI_COMM_WORLD,&myrank);
    double D=(global_Tmax-global_Tmin)/mysize;
    mainWindow.Tmin=global_Tmin+D*myrank;
    mainWindow.Tmax=global_Tmin+D*(myrank+1);
    mainWindow.dT=global_dT;
    mainWindow.U=global_U;
    mainWindow.rows=global_rows;
    mainWindow.cols=global_cols;
    mainWindow.computeRT(data);
   for (int irank=0; irank<mysize; irank++)
   {
        MPI_Barrier(MPI_COMM_WORLD);
        if (irank == myrank)
        {
            f = fopen("xxxx.dat", myrank == 0 ? "w" : "a"); // write new or append
            if (irank == 0)
            {
                fprintf(f,"Data file for Tmin=%lg Tmax=%lg\n",mainWindow.Tmin,mainWindow.Tmax);
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
