/*
 * swm_behavior.cpp
 *
 *  Created on: Jun 30, 2022
 *      Author: cjbeckma
 */

#ifndef SWM_BARRIER_H
#define SWM_BARRIER_H

#include <stdlib.h>
#include <list>
#include "swm.hpp"
#include "swm_shmem.hpp"
#include "random_utils.hpp"

//
// SWM behavioral routine - a tree barrier (with static topology)
//
SWM_CLASS(SwmBarrier)
{

  SwmShmem so;

  public:
    SWM_INIT(SwmBarrier)

    // top-level workload behavior
    //
    // Usage: <work-base> <work-range>
    //
    void behavior(int argc, char * argv[])
    {
        assert(argc == 3);
        int work_base = atoi(argv[1]);
        int work_range = atoi(argv[2]);

        so.shmem_init(this, _me, _np);

        for (int __attribute__((unused)) i=0; true; i++) {

            SwmMarker(1111);

            int w = work_base + int((RandomFloat() / RAND_MAX) * work_range);
            DBGPRINT("work = " << w);
            work(w);

            SwmMarker(1212);

            DBGPRINT("barrier");
            so.shmem_barrier_all();

            SwmMarker(2222);
        }

        DBGPRINT("DONE");
    }
};

#endif
