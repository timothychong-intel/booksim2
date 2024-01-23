/*
 * swm_randperm.hpp
 *
 *  Created on: Dec 14, 2022
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
SWM_CLASS(SwmRandPerm)
{

   SwmSpmat spmat;
   SwmLibgetput lgp;
   SwmStdOptions std;


   SWM_INIT(SwmRandPerm)


   void behavior(int argc, char * argv[]);

};
