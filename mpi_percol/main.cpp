#define _CRT_SECURE_NO_WARNINGS 1

#include <mpi.h>

#include "percol2d.h"
#include "mainwindow.h"


double global_Tmin = 0.1, global_Tmax = 5.3, global_dT = 0.1;//1.3;//2.6;//0.2;
double global_Umin = 165., global_Umax = 235.0, global_dU = 5;
double global_U = 235.;//235.;//190;

int global_rows=600;
int global_cols=603;
int global_seed=25;

int main(int argc, char **argv)
{
    MPI_Init( &argc, &argv );

    MainWindow mainWindow;

    std::vector<X_of_T> data;
    int mysize, myrank;

    MPI_Comm_size(MPI_COMM_WORLD,&mysize);
    MPI_Comm_rank(MPI_COMM_WORLD,&myrank);
//    printf("I am %i of %i\n",myrank,mysize);
    mainWindow.U=global_U;
    mainWindow.rows=global_rows;
    mainWindow.cols=global_cols;
    mainWindow.seed=global_seed;

    if(myrank==0)printf( "seed=%i rows=%i cols=%i U=%lg\n", mainWindow.seed, mainWindow.rows, mainWindow.cols,  mainWindow.U);
    int NT=1+int((global_Tmax-global_Tmin)/global_dT);
//    printf( "T      G      EF    myrank\n");
    for (int j=1+myrank; j<=NT; j+=mysize)
    {
	mainWindow.T=global_Tmin+global_dT*(j-1);
	mainWindow.computeEF_TU();
	mainWindow.computeModel();
        double y=mainWindow.model->conductivity;
        y=(mainWindow.cols-3)*y/mainWindow.rows;
        y=6.28*y;
  
	printf( "T=%lg G=%lg EF=%lg myrank=%i\n", mainWindow.T, y,  mainWindow.EFT, myrank );
  }
    MPI_Finalize();
}
