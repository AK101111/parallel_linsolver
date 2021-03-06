#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
#include <mpi.h>
#include <string.h>
#include <omp.h>
#define DEBUG 1
/*** Skeleton for Lab 1 ***/

/***** Globals ******/
float **a; /* The coefficients */
float *x;  /* The unknowns */
float *b;  /* The constants */
float err; /* The absolute relative error */
int num = 0;  /* number of unknowns */
long NUM_ITER = 0;

/****** Function declarations */
void check_matrix(); /* Check whether the matrix will converge */
void get_input();  /* Read input from file */

/********************************/


/* Function definitions: functions are ordered alphabetically ****/
/*****************************************************************/

/* 
   Conditions for convergence (diagonal dominance):
   1. diagonal element >= sum of all other elements of the row
   2. At least one diagonal element > sum of all other elements of the row
 */
void check_matrix()
{
  int bigger = 0; /* Set to 1 if at least one diag element > sum  */
  int i, j;
  float sum = 0;
  float aii = 0;
  
  for(i = 0; i < num; i++)
  {
    sum = 0;
    aii = fabsf(a[i][i]);
    
    for(j = 0; j < num; j++)
      if( j != i)
        sum += fabsf(a[i][j]);
       
    if( aii < sum)
    {
      printf("The matrix will not converge.\n");
      exit(1);
    }
    
    if(aii > sum)
      bigger++;
    
  }
  
  if( !bigger )
  {
    printf("The matrix will not converge\n");
    exit(1);
  }
}

/******************************************************/
/* Read input from file */
/* After this function returns:
 * a[][] will be filled with coefficients and you can access them using a[i][j] for element (i,j)
 * x[] will contain the initial values of x
 * b[] will contain the constants (i.e. the right-hand-side of the equations
 * num will have number of variables
 * err will have the absolute error that you need to reach
 */
void get_input(char filename[])
{
  FILE * fp;
  int i,j;  
 
  fp = fopen(filename, "r");
  if(!fp)
  {
    printf("Cannot open file %s\n", filename);
    exit(1);
  }
  fscanf(fp,"%d ",&num);
  fscanf(fp,"%f ",&err);

  /* Now, time to allocate the matrices and vectors */
  a = (float**)malloc(num * sizeof(float*));
  if( !a)
  {
    printf("Cannot allocate a!\n");
    exit(1);
  }

  for(i = 0; i < num; i++) 
  {
    a[i] = (float *)malloc(num * sizeof(float)); 
    if( !a[i]){
      printf("Cannot allocate a[%d]!\n",i);
		  exit(1);
  	}
  }
  x = (float *) malloc(num * sizeof(float));
  if( !x){
    printf("Cannot allocate x!\n");
    exit(1);
  }

  b = (float *) malloc(num * sizeof(float));
  if( !b){
    printf("Cannot allocate b!\n");
    exit(1);
  }

 /* Now .. Filling the blanks */ 

 /* The initial values of Xs */
  for(i = 0; i < num; i++)
    fscanf(fp,"%f ", &x[i]);
 
  for(i = 0; i < num; i++){
    for(j = 0; j < num; j++)
      fscanf(fp,"%f ",&a[i][j]);
   
   /* reading the b element */
  fscanf(fp,"%f ",&b[i]);
 }
 
 fclose(fp); 

}


void solve_sequential(){
  // check absolute errors.
  float *x_new, *x_old;
  x_new = (float *) malloc(num * sizeof(float));
  if(!x_new){
    printf("Cannot allocate x!\n");
    exit(1);
  }

  x_old = x;

  float currerr = FLT_MIN;
  do{
  // update x values
    currerr = FLT_MIN;
    // MPI DISTRIBUTE
    for(int i=0; i<num; ++i){
      
      x_new[i] = b[i];
      
      //#pragma omp parallel for
      for(int j = 0; j<i; ++j){
        x_new[i] -= a[i][j]*x_old[j];
      }
      
      //#pragma omp parallel for
      for(int j=i+1; j<num; ++j){
        x_new[i] -= a[i][j]*x_old[j];
      }
      
      x_new[i] /= a[i][i];
    }

    // check errors

    //#pragma omp parallel for reduction(max : currerr)
    for(int i=0; i<num; ++i){
      float new_err = (x_new[i]-x_old[i])/x_new[i];
      if(currerr < new_err){
        currerr = new_err;
      }
    }
    
    memcpy(x_old, x_new, num*sizeof(float));
    NUM_ITER += 1;
  }while(currerr > err); 

  x = x_new;
}

/************************************************************/


int main(int argc, char *argv[])
{
  int i;
  FILE * fp;
  char output[100] ="";
  
  if( argc != 2){
    printf("Usage: ./gs filename\n");
    exit(1);
  }
  
 /* Read the input file and fill the global data structure above */ 
  get_input(argv[1]);
 
 /* Check for convergence condition */
 /* This function will exit the program if the coffeicient will never converge to 
  * the needed absolute error. 
  * This is not expected to happen for this programming assignment.
  */
  check_matrix();
 
 //solve();
  int comm_size;      // Number of processes
  int my_rank;        // My process rank
 
  MPI_Init(NULL, NULL);
  MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
  MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
  if(my_rank == 0)
      printf("%d\n",comm_size);
  int bucket_size = (num + comm_size - 1) / (comm_size);
  int imperfect_div = num % comm_size;

  int lower_bound = my_rank * bucket_size;
  int upper_bound = lower_bound + bucket_size;
  if(imperfect_div){
    upper_bound = num;
  }

  float* x_new;
  float* x_curr;  
  int allocs;
  if(num%comm_size == 0){
    allocs = num;
  }
  else{
    allocs = (num + (comm_size-(num%comm_size)));
  }
  x_new = calloc(allocs, sizeof(float));
  x_curr = calloc(allocs, sizeof(float));
  float err_round;
  do{
    NUM_ITER++;

    for(int i=0; i<upper_bound-lower_bound; ++i){
      x_curr[i] = b[i+lower_bound];
      for(int j=0; j<i+lower_bound; ++j)
        x_curr[i] -= (a[i+lower_bound][j]*x[j]);
      for(int j=i+lower_bound + 1; j<num; ++j)
        x_curr[i] -= (a[i+lower_bound][j]*x[j]);
      x_curr[i] /= a[i+lower_bound][i+lower_bound];
    }

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Allgather(x_curr, bucket_size, MPI_FLOAT, x_new, bucket_size, MPI_FLOAT, MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);

    err_round = err-0.1;
    for (int i = 0; i < num; ++i){
      float curr_error = fabs((x_new[i] - x[i])/x_new[i]);
      if (curr_error > err_round)
        err_round = curr_error;
      x[i] = x_new[i];
    }
  }while(err_round > err);

  free(x_new);
  free(x_curr);

  //MPI_Finalize();

  /* Writing results to file */
  if(my_rank == 0){
    sprintf(output,"%d.sol",num);
    fp = fopen(output,"w");
    if(!fp){
      printf("Cannot create the file %s\n", output);
      exit(1);
    }
    
    for( i = 0; i < num; i++)
      fprintf(fp,"%f\n",x[i]);
 
    printf("total number of iterations: %d\n", NUM_ITER);
 
    fclose(fp);
  }
  MPI_Finalize();
  exit(0);
}
