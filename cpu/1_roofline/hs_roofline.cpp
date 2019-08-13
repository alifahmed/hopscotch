/*******************************************************************************
 *
 * File: hs_roofline.cpp
 * Description: Contains roofline kernel.
 * 
 * Author: Alif Ahmed
 * Date: Aug 06, 2019
 *
 ******************************************************************************/
#include "common.h"
#include <cstdio>
#include <cfloat>
#include <immintrin.h>

#ifndef FLOPS_PER_ELEM
#define FLOPS_PER_ELEM       512
#endif


// Select intrinsic based on supported ISA extensions and data type
#if defined(__AVX512F__)
	#define VEC_WIDTH				512
	#if DATA_T_ENC == DATA_TYPE_FLOAT
		#define VEC_DATA_T			__m512
		#define VEC_FUNC(func,...)	_mm512_##func##_ps(__VA_ARGS__)
	#elif DATA_T_ENC == DATA_TYPE_DOUBLE
		#define VEC_DATA_T			__m512d
		#define VEC_FUNC(func,...)	_mm512_##func##_pd(__VA_ARGS__)
	#endif
#elif defined(__AVX2__) || defined(__AVX__)
	#define VEC_WIDTH				256
	#if DATA_T_ENC == DATA_TYPE_FLOAT
		#define VEC_DATA_T			__m256
		#define VEC_FUNC(func,...)	_mm256_##func##_ps(__VA_ARGS__)
	#elif DATA_T_ENC == DATA_TYPE_DOUBLE
		#define VEC_DATA_T			__m256d
		#define VEC_FUNC(func,...)	_mm256_##func##_pd(__VA_ARGS__)
	#endif
#else
	#define VEC_WIDTH				128
	#if DATA_T_ENC == DATA_TYPE_FLOAT
		#define VEC_DATA_T			__m128
		#define VEC_FUNC(func,...)	_mm128_##func##_ps(__VA_ARGS__)
	#elif DATA_T_ENC == DATA_TYPE_DOUBLE
		#define VEC_DATA_T			__m128d
		#define VEC_FUNC(func,...)	_mm128_##func##_pd(__VA_ARGS__)
	#endif
#endif

#define VEC_ELEMS					(VEC_WIDTH/8/sizeof(DATA_T))


/**
 * Roofline kernel
 */
double kernel_roofline(DATA_T* __restrict__ x, DATA_T r, DATA_T s) {
	// Set constants for multiply and add
    const VEC_DATA_T rv = VEC_FUNC(set1,r);
    const VEC_DATA_T sv = VEC_FUNC(set1,s);

	// Get start time
    auto start = hs_get_time();

	// Run kernel. Manually unrolled 8 times to have enough independent
    // instrustions in the reservation station to keep all the FMA units
	// busy. Unrolling more may result in register spilling.
    #pragma omp parallel for
    for(uint64_t i = 0; i < HS_ARRAY_ELEM; i+=VEC_ELEMS*8){
		// Load 8 array elements into registers
        VEC_DATA_T xv1 = VEC_FUNC(load, &x[i+(VEC_ELEMS*0)]);
        VEC_DATA_T xv2 = VEC_FUNC(load, &x[i+(VEC_ELEMS*1)]);
        VEC_DATA_T xv3 = VEC_FUNC(load, &x[i+(VEC_ELEMS*2)]);
        VEC_DATA_T xv4 = VEC_FUNC(load, &x[i+(VEC_ELEMS*3)]);
        VEC_DATA_T xv5 = VEC_FUNC(load, &x[i+(VEC_ELEMS*4)]);
        VEC_DATA_T xv6 = VEC_FUNC(load, &x[i+(VEC_ELEMS*5)]);
        VEC_DATA_T xv7 = VEC_FUNC(load, &x[i+(VEC_ELEMS*6)]);
        VEC_DATA_T xv8 = VEC_FUNC(load, &x[i+(VEC_ELEMS*7)]);
        
		// Do FMA operations required number of times
        for(uint64_t j = 0; j < FLOPS_PER_ELEM/2; ++j) {
            xv1 = VEC_FUNC(fmadd, xv1, rv, sv);
            xv2 = VEC_FUNC(fmadd, xv2, rv, sv);
            xv3 = VEC_FUNC(fmadd, xv3, rv, sv);
            xv4 = VEC_FUNC(fmadd, xv4, rv, sv);
            xv5 = VEC_FUNC(fmadd, xv5, rv, sv);
            xv6 = VEC_FUNC(fmadd, xv6, rv, sv);
            xv7 = VEC_FUNC(fmadd, xv7, rv, sv);
            xv8 = VEC_FUNC(fmadd, xv8, rv, sv);
        }
       
		// Do an additional operation if FLOPS_PER_ELEM is odd
		#if (FLOPS_PER_ELEM & 1) == 1
		xv1 = VEC_FUNC(add, xv1, sv);
        xv2 = VEC_FUNC(add, xv2, sv);
        xv3 = VEC_FUNC(add, xv3, sv);
        xv4 = VEC_FUNC(add, xv4, sv);
        xv5 = VEC_FUNC(add, xv5, sv);
        xv6 = VEC_FUNC(add, xv6, sv);
        xv7 = VEC_FUNC(add, xv7, sv);
        xv8 = VEC_FUNC(add, xv8, sv);
		#endif

		// Store the result
        VEC_FUNC(store, &x[i+(VEC_ELEMS*0)], xv1);
        VEC_FUNC(store, &x[i+(VEC_ELEMS*1)], xv2);
        VEC_FUNC(store, &x[i+(VEC_ELEMS*2)], xv3);
        VEC_FUNC(store, &x[i+(VEC_ELEMS*3)], xv4);
        VEC_FUNC(store, &x[i+(VEC_ELEMS*4)], xv5);
        VEC_FUNC(store, &x[i+(VEC_ELEMS*5)], xv6);
        VEC_FUNC(store, &x[i+(VEC_ELEMS*6)], xv7);
        VEC_FUNC(store, &x[i+(VEC_ELEMS*7)], xv8);
    }

	// return measured execution time
    return hs_duration(start);
}


int main(){
	// Allocate and initialize working set
    DATA_T* a = (DATA_T*)hs_alloc(HS_ARRAY_SIZE_BYTE);
    hs_init_const(a, HS_ARRAY_ELEM, 1.0);

	// Run kernel NTRIES time and take the best one
    double time_min = DBL_MAX;
    for(uint64_t i = 0; i < NTRIES; ++i){
        double time_curr = kernel_roofline(a, 1.0 + 1e-6, 1e-6);
        if(time_curr < time_min){
            time_min = time_curr;
        }
    }
    
	// Print result
	printf("%-8s%11d%17.2f%19.2f%19.2f\n",
			XSTR(DATA_T),
			FLOPS_PER_ELEM,
			FLOPS_PER_ELEM / 2.0 / sizeof(DATA_T),
			(HS_ARRAY_SIZE_BYTE * 2.0) / time_min / 1000.0 / 1000.0 / 1000.0,
			(HS_ARRAY_ELEM * FLOPS_PER_ELEM) / time_min / 1000.0 / 1000.0 / 1000.0);

	// Done, free allocated memory.
    free(a);

    return 0;
}
