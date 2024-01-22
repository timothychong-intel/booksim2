/*
 * swm_behavior.cpp
 *
 *  Created on: Jun 14, 2022
 *      Author: cjbeckma
 */
#include "swm.hpp"

//
// a simple SWM behavioral routine example
//
SWM_CLASS(SwmTest1)
{
    SWM_INIT(SwmTest1)

    void behavior(int argc, char * argv[])
    {
        int lnbr = (_me + _np - 1) % _np; // left neighbor
        int rnbr = (_me + 1) % _np;       // right neighbor
        const int msz = 8;                // 8B messages
        const int w = 10;                 // 10 "cycles" of local work
        SwmMarker(1111);
        for (int i=0; i<100; i++) {
            work(w);
            DBGPRINT("GET from " << lnbr);
            get(msz, lnbr);
            DBGPRINT("PUT to " << rnbr);
            put(msz, rnbr);
        }
        DBGPRINT("DONE");
        SwmMarker(2222);
    }
};
