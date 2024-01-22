/*
 * swm_toposort.hpp
 *
 *  Created on: Jun 14, 2022
 *      Author: hdogan 
 */
#include <getopt.h>
#include "swm.hpp"
#include "swm_globals.hpp"
#include "swm_shmem.hpp"
#include "swm_libgetput.hpp"
#include "swm_spmat.hpp"
#include "swm_std_options.hpp"


//
// a simple SWM behavioral routine example
//
SWM_CLASS(SwmToposort)
{

   SwmSpmat spmat;
   SwmLibgetput lgp;
   SwmStdOptions std;

   int MYTHREAD;
   int THREADS;


   SWM_INIT(SwmToposort)


   void behavior(int argc, char * argv[]);
   int  toposort(int argc, char * argv[]); 
   sparsemat_t * generate_toposort_input(sparsemat_t * tri_mat, uint64_t rand_seed); 
   int check_is_triangle(sparsemat_t * mat, SHARED int64_t * rperm, SHARED int64_t * cperm); 

   double toposort_matrix_agp(SHARED int64_t *rperm, SHARED int64_t *cperm, sparsemat_t *mat, sparsemat_t *tmat);
};
