/******************************************************************************
 *
 * File: hs_latency.cpp
 * Description: Measures latency. Run by latency.py with different working set 
 * size. Uses single-threaded pointer chasing kernel.
 * 
 * Author: Alif Ahmed
 * Date: Sep 16, 2019
 *
 *****************************************************************************/
#include "common.h"
#include <cstdio>
#include <cstdlib>

#define ALLOWED_RUNTIME 3

int main(){
    // allocate and initialize pointer chain
    void** ptr = (void**)hs_alloc(WSS_ELEMS * sizeof(void*));
    init_pointer_chasing(ptr, WSS_ELEMS);

    // run pointer chasing kernel
    res_t result = run_r_rand_pchase(ALLOWED_RUNTIME, ptr);
    double latency = result.avg_time * 1e9 / WSS_ELEMS;
    
    // print latency
    printf("%21lu%47.2lf\n", WSS_BYTES, latency);
        
    free(ptr);
    
    return 0;
}
