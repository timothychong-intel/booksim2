/*
 * swm_toposort.cpp
 *
 * toposort kernel from Bale benchmark suite
 * for Booksim scalable workload model
 *
 *  Created on: Dec 24, 2022
 *      Author: hdogan
*/




/******************************************************************
//
//
//  Copyright(C) 2020, Institute for Defense Analyses
//  4850 Mark Center Drive, Alexandria, VA; 703-845-2500
//
//
//  All rights reserved.
//
//   This file is a part of Bale.  For license information see the
//   LICENSE file in the top level directory of the distribution.
//
//
 *****************************************************************/

/*! \file toposort.upc
 * \brief Driver routine that runs different implementations of toposort on a permuted upper triangular matrix.
 */

#include "swm_toposort.hpp"
#include "swm_spmat.hpp"

/*!
  \page toposort_page Toposort



  Run with the --help, -?, or --usage flags for run details.
  */


/*!
 * \brief check the result toposort
 * \param mat the original matrix
 * \param rperm the row permutation
 * \param cperm the column permutation
 * \return 0 on success, 1 otherwise
 * We check that the permutations are in fact permutations and the check that applying
 * them to the given matrix yields an upper triangular matrix.
 */
int SwmToposort::check_is_triangle(sparsemat_t * mat, SHARED int64_t * rperm, SHARED int64_t * cperm) {
   int ret = 0;

   int rf = spmat.is_perm(rperm, mat->numrows);
   int cf = spmat.is_perm(cperm, mat->numrows);
   if(!rf || !cf){
      T0_fprintf(stderr,"ERROR: check_is_triangle is_perm(rperm) = %d is_perm(cperm) = %d\n",rf,cf);
      return(1);
   }
   sparsemat_t * mat2 = spmat.permute_matrix(mat, rperm, cperm);
   if(!mat2){
      T0_fprintf(stderr,"ERROR: check_is_triangle mat2 is NULL\n");
      return(1);
   }
   if(!spmat.is_upper_triangular(mat2, 1)) {
      T0_fprintf(stderr,"ERROR: check_is_triangle fails\n");
      ret = 1;
   }
   spmat.clear_matrix(mat2);
   free(mat2);
   return(ret);
}

/*! \brief Generates an input matrix for the toposort algorithm from a unit-triangulare matrix.
 * \param tri_mat An upper (or lower) triangular matrix with unit diagonal.
 * \param rand_seed the seed for random number generator.
 * \return a permuted upper triangular matrix
 */
sparsemat_t * SwmToposort::generate_toposort_input(sparsemat_t * tri_mat, uint64_t rand_seed) {
   sparsemat_t * mat = NULL;
   //double t;
   //write_matrix_mm(tri_mat, "tri_mat");
   if(!spmat.is_upper_triangular(tri_mat, 1)){
      if(spmat.is_lower_triangular(tri_mat, 1)){
         mat = spmat.transpose_matrix(tri_mat);
         if(!MYTHREAD)
            printf("end of transpose_matrix\n");
      }else{
         printf("[%d] ERROR: toposort: input matrix is not triangular with unit diagonal!\n", _me);
         T0_fprintf(stderr, "ERROR: toposort: input matrix is not triangular with unit diagonal!\n");
         return(NULL);
      }
   }else{
      mat = tri_mat;
   }

   if(!mat){T0_printf("ERROR: mat is NULL!\n"); return(NULL);}

   // get row and column permutations
   //t = wall_seconds();
   SHARED int64_t * rperm = spmat.rand_permp(mat->numrows, rand_seed);
   SHARED int64_t * cperm = spmat.rand_permp(mat->numrows, rand_seed + 12345);
   //T0_printf("generate perms time %lf\n", wall_seconds() - t);
   lgp.lgp_barrier();



   if(!rperm || !cperm){
      T0_printf("ERROR: topo_rand_permp returns NULL!\n");fflush(0);
      spmat.clear_matrix(mat);
      free(mat);
      return(NULL);
   }

   lgp.lgp_barrier();
   //t = wall_seconds();
   sparsemat_t * pmat = spmat.permute_matrix(mat, rperm, cperm);
   if(!pmat) {
      T0_printf("ERROR: permute_matrix returned NULL");fflush(0);
      spmat.clear_matrix(mat);
      free(mat);
      return(NULL);
   }
   //T0_printf("permute matrix time %lf\n", wall_seconds() - t);

   lgp.lgp_barrier();
   if(mat != tri_mat){
      spmat.clear_matrix( mat );free(mat);
   }

   lgp.lgp_barrier();

   lgp.lgp_all_free(rperm);
   lgp.lgp_all_free(cperm);

   return( pmat );
}

typedef struct args_t{
  std_args_t std;
  std_graph_args_t gstd;
}args_t;

static int parse_opt(int key, char * arg, struct argp_state * state){
   args_t * args = (args_t *)state->input;
   switch(key)
   {
      case ARGP_KEY_INIT:
         state->child_inputs[0] = &args->std;
         state->child_inputs[1] = &args->gstd;
         break;
   }
   return(0);
}

static struct argp_child children_parsers[] =
  {
    {&std_options_argp, 0, "Standard Options", -2},
    {&std_graph_options_argp, 0, "Standard Graph Options", -3},
    {0, 0, 0, 0}
  };




int SwmToposort::toposort(int argc, char * argv[]) {

   /* process command line */
   args_t args = args_t(); // initialize args struct to all zero
   struct argp argp = {NULL, parse_opt, 0,
      "Parallel topological sort.", children_parsers, 0, 0};


   args.gstd.loops = 1; // force loops into graph
   args.gstd.l_numrows = 100;
   int ret = std.bale_app_init(argc, argv, &args, sizeof(args_t), &argp, &args.std);
   if(!MYTHREAD)
      printf("bale_app_init\n");
   if(ret < 0) return(ret);
   else if(ret) return(0);


   /* force input graph to be undirected and to have loops, no matter what the options */
   if(args.gstd.directed == 1){
      T0_fprintf(stderr, "toposort needs undirected graph input, overriding -d flag.\n");
      args.gstd.directed = 0;
   }

   if(!MYTHREAD){
      std.write_std_graph_options(&args.std, &args.gstd);
      std.write_std_options(&args.std);
   }

   if(!MYTHREAD)
      printf("input matrix\n");

   // read in a matrix or generate a random graph
   sparsemat_t * inmat = std.get_input_graph(&args.std, &args.gstd);
   if(!inmat){T0_printf("Error! toposort: inmat is NULL");lgp.lgp_global_exit(-1);}

   //if(!MYTHREAD)
   //   printf("input matrix\n");

   //printf("[%d] inmat %p\n", _me, inmat);
   // permate the rows and columns of the matrix randomly
   sparsemat_t * mat = generate_toposort_input(inmat, args.std.seed + 23456);
   if(!mat){T0_printf("Error! toposort: mat is NULL");lgp.lgp_global_exit(-1);}

   if(!MYTHREAD)
      printf("toposort input matrix\n");

   // get the transpose of mat (needed for toposort implemmentations)
   sparsemat_t * tmat = spmat.transpose_matrix(mat);
   if(!tmat){T0_printf("Error! toposort: tmat is NULL"); lgp.lgp_global_exit(-1);}

   T0_printf("transpose_matrix!\n"); fflush(0);

   if(!MYTHREAD)
      printf("tranpose matrix\n");

   if(args.std.dump_files){
      char c1[] = "topo_inmat";
      char c2[] = "topo_permuted_mat";
      spmat.write_matrix_mm(inmat, c1);
      spmat.write_matrix_mm(mat, c2);
   }

   lgp.lgp_barrier();
   spmat.clear_matrix(inmat); free(inmat);


   // arrays to hold the row and col permutations
   SHARED int64_t *rperm2 = (int64_t *)lgp.lgp_all_alloc(mat->numrows, sizeof(int64_t));
   lgp.getShmem()->getSimThread()->YIELD();
   SHARED int64_t *cperm2 = (int64_t *)lgp.lgp_all_alloc(mat->numrows, sizeof(int64_t));
   lgp.getShmem()->getSimThread()->YIELD();
   //T0_printf("clear_matrix!\n"); fflush(0);

   int64_t use_model;
   //double laptime = 0.0;
   char model_str[32];
   int error = 0;
   for( use_model=1L; use_model < 32; use_model *=2 ) {

      switch( use_model & args.std.models_mask ) {
         case AGP_Model:
            sprintf(model_str, "AGP");
            //laptime = toposort_matrix_agp(rperm2, cperm2, mat, tmat);
            toposort_matrix_agp(rperm2, cperm2, mat, tmat);
            break;

         case EXSTACK_Model:
            sprintf(model_str, "Exstack");
            //laptime = toposort_matrix_exstack(rperm2, cperm2, mat, tmat, args.std.buf_cnt);
            break;

         case EXSTACK2_Model:
            sprintf(model_str, "Exstack2");
            //laptime = toposort_matrix_exstack2(rperm2, cperm2, mat, tmat, args.std.buf_cnt);
            break;

         case CONVEYOR_Model:
            sprintf(model_str, "Conveyor");
            //laptime = toposort_matrix_convey(rperm2, cperm2, mat, tmat);
            break;

         case ALTERNATE_Model:
            //T0_fprintf(stderr,"There is no alternate model here!\n"); continue;
            sprintf(model_str, "Alternate");
            //laptime = toposort_matrix_cooler(rperm2, cperm2, mat, tmat);
            break;

         default:
            continue;

      }
      lgp.lgp_barrier();

      //std.bale_app_write_time(&args.std, model_str, laptime);

      if( check_is_triangle(mat, rperm2, cperm2) ) {
         T0_fprintf(stderr,"\nERROR: After toposort_matrix_upc: mat2 is not upper-triangular!\n");
         error = 1;
      }
   }

   lgp.lgp_barrier();

   std.bale_app_finish(&args.std);


   return(error);
}

void SwmToposort::behavior(int argc, char * argv[])
{
   THREADS  = _np;
   MYTHREAD = _me;

   // Init objects
   lgp.lgp_init(this, _me, _np);
   spmat.init_spmat(&lgp, _me, _np);
   std.init_std_options(&lgp, &spmat, _me, _np);


   toposort(argc, argv);

}

