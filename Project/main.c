#include "helper.h"
#include "visual.h"
#include "init.h"
#include"uvp.h"
#include"boundary_val.h"
#include"sor.h"
#include "surface.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * The main operation reads the configuration file, initializes the scenario and
 * contains the main loop. So here are the individual steps of the algorithm:
 *
 * - read the program configuration file using read_parameters()
 * - set up the matrices (arrays) needed using the matrix() command
 * - create the initial setup init_uvp(), init_flag(), output_uvp()
 * - perform the main loop
 * - trailer: destroy memory allocated and do some statistics
 *
 * The layout of the grid is decribed by the first figure below, the enumeration
 * of the whole grid is given by the second figure. All the unknowns corresond
 * to a two dimensional degree of freedom layout, so they are not stored in
 * arrays, but in a matrix.
 *
 * @image html grid.jpg
 *
 * @image html whole-grid.jpg
 *
 * Within the main loop the following big steps are done (for some of the
 * operations a definition is defined already within uvp.h):
 *
 * - calculate_dt() Determine the maximal time step size.
 * - boundaryvalues() Set the boundary values for the next time step.
 * - calculate_fg() Determine the values of F and G (diffusion and confection).
 *   This is the right hand side of the pressure equation and used later on for
 *   the time step transition.
 * - calculate_rs()
 * - Iterate the pressure poisson equation until the residual becomes smaller
 *   than eps or the maximal number of iterations is performed. Within the
 *   iteration loop the operation sor() is used.
 * - calculate_uv() Calculate the velocity at the next time step.
 */
int main(int argn, char** args){

	printf("Start of Run... \n");
			printf("CFD Project, Group D \n");
			printf("Please select the problem from the list below by typing 1-9 \n");
			printf("P1. Tanker SideSection 50percent full without baffle \n");
			printf("P2. Tanker SideSection 50percent full with baffle \n");
			printf("P3. Tanker SideSection 80percent full without baffle \n");
			printf("P4. Tanker SideSection 80percent full with baffle \n");
			printf("P5. Tanker FrontSection 50percent full without baffle \n");
			printf("P6. Tanker FrontSection 50percent full with baffle \n");
			printf("P7. Tanker FrontSection 80percent full without baffle \n");
			printf("P8. Tanker FrontSection 80percent full with baffle \n");
			printf("P9. Breaking dam \n");
			
			int select;
			char* geometry = (char*)(malloc(sizeof(char)*100));
			char* problem = (char*)(malloc(sizeof(char)*100));
			scanf("%d",&select);
			//select problem
			const char* filename = "0";
			switch(select)
			{
			case 1:
			filename = "Tanker_SS_50_1.dat";
			break;

			case 2:
			filename = "Tanker_SS_50_2.dat";
			break;

			case 3:
			filename = "Tanker_SS_80_1.dat";
			break;

			case 4:
			filename = "Tanker_SS_80_2.dat";
			break;

			case 5:
			filename = "tanker2_50_FS.dat";
			break;

			case 6:
			filename = "tanker1_50_FS.dat";
			break;

			case 7:
			filename = "tanker2_80_FS.dat";
			break;

			case 8:
			filename = "tanker1_80_FS.dat";
			break;

			case 9:
			filename = "breaking_dam.dat";
			break;
			}

    //define parameter variables
    double Re;                /* reynolds number   */
    double UI;                /* velocity x-direction */
    double VI;                /* velocity y-direction */
    double PI;                /* pressure */
    double GX;                /* gravitation x-direction */
    double GY;                /* gravitation y-direction */
    double t_end;             /* end time */
    double xlength;           /* length of the domain x-dir.*/
    double ylength;           /* length of the domain y-dir.*/
    double dt;                /* time step */
    double dx;                /* length of a cell x-dir. */
    double dy;                /* length of a cell y-dir. */
    int  imax;                /* number of cells x-direction*/
    int  jmax;                /* number of cells y-direction*/
    double alpha;             /* uppwind differencing factor*/
    double omg;               /* relaxation factor */
    double tau;               /* safety factor for time step*/
    int  itermax;             /* max. number of iterations  */
    double eps;               /* accuracy bound for pressure*/
    double dt_value;           /* time for output */
	int ppc;					/* particles per cell */

    //Read and assign the parameter values from file
    read_parameters(filename, &imax, &jmax, &xlength, &ylength,
			&dt, &t_end, &tau, &dt_value, &eps, &omg, &alpha, &itermax,
			&GX, &GY, &Re, &UI, &VI, &PI, &ppc, &dx, &dy, geometry, problem);

    //Allocate the matrices for P(pressure), U(velocity_x), V(velocity_y), F, and G on heap
    printf("PROGRESS: Starting matrix allocation... \n");
    double **P = matrix(0, imax-1, 0, jmax-1);
    double **U = matrix(0, imax-1, 0, jmax-1);
    double **V = matrix(0, imax-1, 0, jmax-1);
    double **F = matrix(0, imax-1, 0, jmax-1);
    double **G = matrix(0, imax-1, 0, jmax-1);
    double **RS = matrix(0, imax-1, 0, jmax-1);
    int **flag = imatrix(0, imax-1, 0, jmax-1);
	printf("PROGRESS: Matrix allocated... \n");

	int num_particlelines = 0;

	//Initilize flags
	init_flag(geometry, imax, jmax, flag);

	for(int j = 0; j<jmax; ++j)
	{
		for(int i = 0; i<imax; ++i)
		{
			printf("%d ",flag[i][jmax-1-j]);
		}
		printf("\n");
	}

	struct particleline *pline;

	// Initialize particles
	printf("PROGRESS: Initializing Particles... \n");
	pline = INIT_PARTICLES (&num_particlelines, imax, jmax, dx, dy, ppc, flag,select);
	printf("PROGRESS: Particles initialized... \n");

	
	
    //Initialize the U, V and P
   	init_uvp(UI, VI, PI, imax, jmax, U, V, P, flag);


	//Make solution folder
	struct stat st = {0};
	char sol_folder[80];
	sprintf( sol_folder,"Solution_%s",problem);
	if (stat(sol_folder, &st) == -1) {
    		mkdir(sol_folder, 0700);
	}
	char sol_directory[80];
	sprintf( sol_directory,"Solution_%s/sol", problem);

	//create log file
	char LogFileName[80];
 	FILE *fp_log = NULL;
	sprintf( LogFileName, "%s.log", problem );
	fp_log = fopen( LogFileName, "w");
	fprintf(fp_log, "It.no.|   Time    |time step |SOR iterations | residual | SOR converged \n");


	char LogFileName1[80];
 	FILE *fp_fxfyke = NULL;
	sprintf( LogFileName1, "%s.FxFyKE.csv", problem );
	fp_fxfyke = fopen( LogFileName1, "w");

	MARK_CELLS(flag,  imax,  jmax,  dx,  dy,  num_particlelines, pline);

	boundaryvalues(imax, jmax, U, V, flag);

    printf("PROGRESS: Starting the flow simulation...\n");
    double t=0; int n=0; int n1=0;

	while (t < t_end) {
        char* is_converged = "Yes";

		calculate_dt(Re,tau,&dt,dx,dy,imax,jmax, U, V);
   		printf("t = %f ,dt = %f,  ",t,dt);
		
		MARK_CELLS(flag,  imax,  jmax,  dx,  dy,  num_particlelines, pline);
		
		
		SET_UVP_SURFACE(U,V, P, flag,  imax,  jmax,  Re,  dx,  dy,  dt, GX,GY);
		
		calculate_fg(Re,GX,GY,alpha,dt,dx,dy,imax,jmax,U,V,F,G,flag);
    		
		calculate_rs(dt,dx,dy,imax,jmax,F,G,RS,flag);
		

		int it = 0;
		double res = 10.0;

    	do {
    		sor(omg,dx,dy,imax,jmax,P,RS,&res,flag);
			++it;

    	} while(it<itermax && res>eps);
		
		

		printf("SOR itertions = %d ,residual = %f \n", it-1, res);
		if((it==itermax)&&(res>eps)){
			printf("WARNING: Iteration limit reached before convergence. \n");
			is_converged = "No";
		}
  		fprintf(fp_log, "    %d |  %f | %f |      %d      | %f | %s \n", n, t, dt, it-1, res, is_converged);

		calculate_uv(dt,dx,dy,imax,jmax,U,V,F,G,P,flag);
		
		boundaryvalues(imax, jmax, U, V, flag);
		
		SET_UVP_SURFACE(U,V, P, flag,  imax,  jmax,  Re,  dx,  dy,  dt, GX,GY);

		ADVANCE_PARTICLES(U, V, dx, dy, dt, num_particlelines, pline, flag, imax, jmax);
		
		
		set_gravity(&GX, &GY, t, 1);
		
		if ((t >= n1*dt_value)&&(t!=0.0))
  		{
   			write_vtkFile(sol_directory ,n ,xlength ,ylength ,imax-2 ,jmax-2 , dx ,dy ,U ,V ,P);

			write_vtkParticleFile(sol_directory,n,xlength,ylength,imax,jmax,num_particlelines,dx,dy,pline, flag);

			fprintf(fp_fxfyke, "%f, %f, %f, %f \n", t, Force_x(imax, jmax, dy, P, flag),
					Force_y(imax, jmax, dx, P, flag), KE(imax, jmax, U, V, flag) );

			printf("writing result at %f seconds \n",n1*dt_value);
    		n1=n1+ 1;
  		}
    	t =t+ dt;
    	n = n+ 1;

    }

	fclose(fp_log);
    printf("PROGRESS: flow simulation completed...\n \n");

    printf("PROGRESS: Freeing allocated memory...\n");
    //Free memory
    free_matrix( P, 0, imax-1, 0, jmax-1);
    free_matrix( U, 0, imax-1, 0, jmax-1);
    free_matrix( V, 0, imax-1, 0, jmax-1);
    free_matrix( F, 0, imax-1, 0, jmax-1);
    free_matrix( G, 0, imax-1, 0, jmax-1);
    free_matrix(RS, 0, imax-1, 0, jmax-1);
    free_imatrix(flag, 0, imax-1, 0, jmax-1);

	free(geometry);
	free(problem);

	FREE_PARTICLELINES(pline, num_particlelines, imax, jmax, dx, dy, ppc, flag );

    printf("PROGRESS: allocated memory released...\n \n");

	printf("PROGRESS: End of Run.\n");
  	
	return -1;

}
