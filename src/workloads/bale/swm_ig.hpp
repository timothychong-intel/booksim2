/*
 * swm_behavior.cpp
 *
 *  Created on: Jun 14, 2022
 *      Author: cjbeckma
 */
#include "swm.hpp"
#include "random_utils.hpp"

//
// a simple SWM behavioral routine example
//
SWM_CLASS(SwmIg)
{
    SWM_INIT(SwmIg)

    SwmShmem so;

    void behavior(int argc, char * argv[])
    {
      so.shmem_init(this, _me, _np);
      int w    = getConfig()->GetInt("swm_core_work");          // 10 "cycles" of local work
      int rows = 1000;
	   int msz  = 8;                // 8B messages
      if(argc) {
        rows = atoi(argv[0]);
      }

      SwmMarker(1111);

      for (int j=0; j<rows; j++) {
		   work(w);
	      int pe = RandomInt(_np - 1);       // right neighbor
		   DBGPRINT("GET from " << pe);
		   get(msz, pe);
      }

      SwmMarker(2222);

	   DBGPRINT("DONE");
    }
};
