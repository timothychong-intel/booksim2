/*
 * swm_basic.hpp
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

#include <fstream>
#include <iostream>

#include <random>
#include <cmath>

//
// a simple SWM behavioral routine example
//
SWM_CLASS(SwmBasic)
{

   SwmSpmat spmat;
   SwmLibgetput lgp;
   SwmStdOptions std;

   SWM_INIT(SwmBasic)

   exponential_distribution<double> * expo;

   default_random_engine generator;
   int packet_size;
   double last_injection_time;
   double send_interval;

   void behavior(int argc, char * argv[]);

   double gen_interval();
   void send_packets(int num_of_packets, unsigned int run_till);
};
