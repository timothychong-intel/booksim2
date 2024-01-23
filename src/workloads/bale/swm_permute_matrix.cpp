/*
 * swm_permute_matrix.cpp
 *
 *  Created on: Dec 14, 2022
 *      Author: hdogan
 */
#include <getopt.h>
#include "swm_permute_matrix.hpp"


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
    {0,0,0,0},
  };



void SwmPermMatrix::behavior(int argc, char * argv[])
{

   use_nbi_version = getConfig()->GetInt("swm_nbi_version");
   int MYTHREAD = _me;
   /******************************************************************************/
   /*! \brief create a global int64_t array with a uniform random permutation
    * \param N the length of the global array
    * \param seed seed for the random number generator
    * \return the permutation
    *
    * This is a collective call.
    * this implements the random dart algorithm to generate the permutation.
    * Each thread throws its elements of the perm array randomly at large target array.
    * Each element claims a unique entry in the large array using compare_and_swap.
    * This gives a random permutation with spaces in it, then you squeeze out the spaces.
    * \ingroup spmatgrp
    */
   // Init objects

   lgp.lgp_init(this, _me, _np);
   spmat.init_spmat(&lgp, _me, _np);
   std.init_std_options(&lgp, &spmat, _me, _np);

   args_t args = args_t();  // initialize args struct to all zero
   struct argp argp = {NULL, parse_opt, 0,
      "Parallel permute sparse matrix.", children_parsers, 0, 0};

   args.gstd.l_numrows = 1000;
   int ret = std.bale_app_init(argc, argv, &args, sizeof(args_t), &argp, &args.std);
   if(ret < 0) return;
   else if(ret) return;


   sparsemat_t * inmat = std.get_input_graph(&args.std, &args.gstd);
   //spmat.print_matrix(inmat);
   if(!inmat){T0_fprintf(stderr, "ERROR: permute_matrix: inmat is NULL!\n");}

   SHARED int64_t * rp = spmat.rand_permp(inmat->numrows, args.std.seed);
   SHARED int64_t * cp = spmat.rand_permp(inmat->numcols, args.std.seed + 12345);

   lgp.lgp_barrier();

   if(args.std.dump_files){
      char c[] = "permute_matrix_inmat";
      spmat.write_matrix_mm(inmat, c);
      if(!MYTHREAD){
         int64_t i;
         FILE * fp = fopen("permute_matrix_rperm", "w");
         for(i = 0; i < inmat->numrows; i++)
            fprintf(fp, "%ld\n", lgp.lgp_get_int64(rp, i));
         fclose(fp);
         fp = fopen("permute_matrix_cperm", "w");
         for(i = 0; i < inmat->numcols; i++)
            fprintf(fp, "%ld\n", lgp.lgp_get_int64(cp, i));
         fclose(fp);
      }
   }

   int64_t use_model;
   sparsemat_t * outmat = NULL;
   sparsemat_t * refmat = NULL;
   char model_str[32];
   for( use_model=1L; use_model < 32; use_model *=2 ) {
      //double t1 = wall_seconds();
      switch( use_model & args.std.models_mask ) {

         case AGP_Model:
            if(!MYTHREAD)
               printf("AGP Permute Matrix\n");
            if(!use_nbi_version)
               outmat = spmat.permute_matrix_agp(inmat, rp, cp);
            else
               outmat = spmat.permute_matrix_agp_nbi(inmat, rp, cp);
            sprintf(model_str, "AGP");
            break;

         case EXSTACK_Model:
            //outmat = permute_matrix_exstack(inmat, rp, cp, args.std.buf_cnt);
            sprintf(model_str, "Exstack");
            break;

         case EXSTACK2_Model:
            //outmat = permute_matrix_exstack2(inmat, rp, cp, args.std.buf_cnt);
            sprintf(model_str, "Exstack2");
            break;

         case CONVEYOR_Model:
            //outmat = permute_matrix_conveyor(inmat, rp, cp);
            sprintf(model_str, "Conveyor");
            break;
         case ALTERNATE_Model:
            T0_fprintf(stderr,"There is no alternate model here!\n"); continue;
            break;
         case 0:
            continue;
      }

      /* if running more than one implmentation, save the first to check against the others*/
      if(!refmat){
         refmat = outmat;
      }else{
         if(spmat.compare_matrix(refmat, outmat)){
            T0_fprintf(stderr,"ERROR: permute_matrix does not match!\n");
            //error = 1;
         }
         spmat.clear_matrix(outmat);
         free(outmat);
      }
   }
   if (refmat){
       free(refmat);
       spmat.clear_matrix(refmat);
   }
   free(inmat);


   lgp.lgp_barrier();

   if(_me == 0)
      printf("Application is completed\n");

   DBGPRINT("DONE");
}
