/*
 * swm_libgetput.cpp
 *
 * Libgetput implementation from Bale benchmark suite
 * for Booksim scalable workload model
 *
 *  Created on: Dec 24, 2022
 *      Author: hdogan
*/


#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <locale.h>
#include <sys/time.h>
#include "swm_libgetput.hpp"
#include "random_utils.hpp"




void SwmLibgetput::lgp_init(SwmThread * thread, int me, int np)
{
   MYTHREAD = me;
   THREADS = np;
   getShmem()->shmem_init(thread, me, np);
}

/*!
 * \brief Wrapper for atomic compare and swap to help with ifdef noise
 * \return the old value
 * \ingroup libgetputgrp
 */
int64_t SwmLibgetput::lgp_cmp_and_swap(SHARED int64_t * ptr, int64_t index, int64_t cmp_val, int64_t swap_val) {
   int64_t ret;
   int64_t lindex = index/shmem_n_pes();
   int64_t pe = index % shmem_n_pes();
   ret = getShmem()->shmem_atomic_compare_swap(&ptr[lindex], cmp_val, swap_val, (int)pe);
   return(ret);
}
/*!
 * \brief Wrapper for non-blocking atomic add to help with ifdef noise
 * \ingroup libgetputgrp
 */
void SwmLibgetput::lgp_atomic_add_async(SHARED int64_t * ptr, int64_t index, int64_t value){
   int64_t lindex = index/shmem_n_pes();
   int64_t pe = index % shmem_n_pes();
   getShmem()->shmem_atomic_add(&ptr[lindex], value, (int)pe);
}

/*!
 * \brief Wrapper for atomic fetch and inc to help with ifdef noise
 * \ingroup libgetputgrp
 */
int64_t SwmLibgetput::lgp_fetch_and_inc(SHARED int64_t * ptr, int64_t index) {
   int64_t ret;
   int64_t lindex = index/shmem_n_pes();
   int64_t pe = index % shmem_n_pes();
   ret = getShmem()->shmem_atomic_fetch_inc(&ptr[lindex], (int)pe);
   return(ret);
}

/*!
 * \brief Wrapper for atomic fetch and inc to help with ifdef noise
 * \ingroup libgetputgrp
 */
int64_t SwmLibgetput::lgp_fetch_and_add(SHARED int64_t * ptr, int64_t index, int64_t value) {
   int64_t ret;
   int64_t lindex = index/shmem_n_pes();
   int64_t pe = index % shmem_n_pes();
   ret = getShmem()->shmem_atomic_fetch_add(&ptr[lindex], value, (int)pe);
   return(ret);
}

/*!
 * \brief Wrapper for atomic fetch and inc to help with ifdef noise
 * \ingroup libgetputgrp
 */
int64_t SwmLibgetput::lgp_fetch_and_inc_nbi(SHARED int64_t * ptr, int64_t index) {
   int64_t ret;
   int64_t lindex = index/shmem_n_pes();
   int64_t pe = index % shmem_n_pes();
   ret = getShmem()->shmem_atomic_fetch_inc_nbi(&ptr[lindex], (int)pe);
   return(ret);
}

/*!
 * \brief Wrapper for atomic fetch and inc to help with ifdef noise
 * \ingroup libgetputgrp
 */
int64_t SwmLibgetput::lgp_fetch_and_add_nbi(SHARED int64_t * ptr, int64_t index, int64_t value) {
   int64_t ret;
   int64_t lindex = index/shmem_n_pes();
   int64_t pe = index % shmem_n_pes();
   ret = getShmem()->shmem_atomic_fetch_add_nbi(&ptr[lindex], value, (int)pe);
   return(ret);
}


/*!
 * \ingroup libgetputgrp
 */
void  SwmLibgetput::lgp_shmem_write_upc_array_int64(SHARED int64_t *addr, size_t index, size_t blocksize, int64_t val) {
   int pe;
   size_t local_index;
   int64_t *local_ptr;


   pe = index % shmem_n_pes();
   local_index = (index / shmem_n_pes())*blocksize;

   local_ptr =(int64_t*)(( (char*)addr ) + local_index);

   getShmem()->shmem_int64_p ( local_ptr, val, pe );
}

void  SwmLibgetput::lgp_shmem_write_upc_array_double(SHARED double *addr, size_t index, size_t blocksize, double val) {
   int pe;
   size_t local_index;
   double *local_ptr;


   pe = index % shmem_n_pes();
   local_index = (index / shmem_n_pes())*blocksize;

   local_ptr =(double*)(( (char*)addr ) + local_index);

   getShmem()->shmem_double_p ( local_ptr, val, pe );
}

/*!
 * \ingroup libgetputgrp
 */
int64_t  SwmLibgetput::lgp_shmem_read_upc_array_int64(const SHARED int64_t *addr, size_t index, size_t blocksize) {
   int pe;
   size_t local_index;
   int64_t *local_ptr;

   pe = index % shmem_n_pes();
   local_index = (index / shmem_n_pes())*blocksize;

   local_ptr =(int64_t*)(( (char*)addr ) + local_index);

   return getShmem()->shmem_int64_g ( local_ptr, pe );
}

/*!
 * \ingroup libgetputgrp
 */
double SwmLibgetput::lgp_shmem_read_upc_array_double(const SHARED double *addr, size_t index, size_t blocksize) {
   int pe;
   size_t local_index;
   double *local_ptr;

   /* asupc_init tests that (long long) == (int64_t) */

   pe = index % shmem_n_pes();
   local_index = (index / shmem_n_pes())*blocksize;

   local_ptr =(double*)(( (char*)addr ) + local_index);

   return getShmem()->shmem_double_g ( local_ptr, pe );
}

/*!
 * \ingroup libgetputgrp
 */
int64_t  SwmLibgetput::lgp_shmem_read_upc_array_int64_nbi(const SHARED int64_t *addr, size_t index, size_t blocksize) {
   int pe;
   size_t local_index;
   int64_t *local_ptr;


   pe = index % shmem_n_pes();
   local_index = (index / shmem_n_pes())*blocksize;

   local_ptr =(int64_t*)(( (char*)addr ) + local_index);

   return getShmem()->shmem_int64_get_nbi ( local_ptr, pe );
}

/*!
 * \ingroup libgetputgrp
 */
double SwmLibgetput::lgp_shmem_read_upc_array_double_nbi(const SHARED double *addr, size_t index, size_t blocksize) {
   int pe;
   size_t local_index;
   double *local_ptr;

   /* asupc_init tests that (long long) == (int64_t) */

   pe = index % shmem_n_pes();
   local_index = (index / shmem_n_pes())*blocksize;

   local_ptr =(double*)(( (char*)addr ) + local_index);

   return getShmem()->shmem_double_get_nbi ( local_ptr, pe );
}



/*!
  \brief Compute partial sums across threads.
  \param x input value \f$x_m\f$
  \return \f$\sum_{i<=m} x_i\f$  where \f$m\f$ is MYTHREAD

NB: This function must be called on all threads.
 * \ingroup libgetputgrp
 */
int64_t SwmLibgetput::lgp_partial_add_l(int64_t x) {

   int64_t * tmp = (int64_t*)lgp_all_alloc(THREADS,sizeof(int64_t));
   int64_t out = 0;

   lgp_put_int64(tmp, MYTHREAD, x);

   lgp_barrier();

   for (int i = 0; i <= MYTHREAD; i++) {
      out += lgp_get_int64(tmp, i);
   }

   lgp_barrier();

   return out;
}

/*!
  \brief Compute prior partial sums (not including this value) across threads.
  \param x the value on <tt>MYTHREAD</tt>
  \return \f$\sum_{i<m} x_i\f$, where \f$x_m\f$ is <tt>MYTHREAD</tt>
NB: This function must be called on all threads.
\ingroup libgetputgrp
*/
int64_t SwmLibgetput::lgp_prior_add_l(int64_t x) {
   return lgp_partial_add_l(x) - x;
}

//#define USE_KNUTH   /*!< Default define to set whether we use the Knuth random number generator or rand48 */
#ifdef USE_KNUTH
#define LGP_RAND_MAX 2251799813685248  /*!< max random number depends on which rng we use */
#include "knuth_rng_double_2019.h"
#else
#define LGP_RAND_MAX 281474976710656
#endif

/*!
 * \brief seed for the random number generator
 * \param seed the seed
 * Note: if all thread call this with the same seed they actually get different seeds.
 */
void SwmLibgetput::lgp_rand_seed(int64_t seed){
#ifdef USE_KNUTH
   ranf_start(seed + 1 + MYTHREAD);
#else
   srand48(seed + 1 + MYTHREAD);
#endif
}

/*!
 * \brief return a random integer mod N.
 * \param N the modulus
 */
int64_t SwmLibgetput::lgp_rand_int64(int64_t N){
   assert(N < LGP_RAND_MAX);
#ifdef USE_KNUTH
   return((int64_t)(ranf_arr_next()*N));
#else
   return((int64_t)(RandomFloat()*N));
#endif
}

/*!
 * \brief return a random double in the interval (0,1]
 */
double SwmLibgetput::lgp_rand_double(){
#ifdef USE_KNUTH
   return(ranf_arr_next());
#else
   return(RandomFloat());
#endif
}

//work=setup_shmem_reduce_workdata(&sync,sizeof(STYPE));
/* Macro to define wrapper around shmem reduce functions */
#define Define_Reducer( NAME, XTYPE, STYPE, CTYPE, SHMEM_FUNC)      \
   XTYPE SwmLibgetput::NAME (XTYPE myval) {                           \
      STYPE *buff=NULL, *work=NULL; static long *sync;              \
      assert(sizeof(STYPE) == sizeof(CTYPE));                         \
      if (buff==NULL) {                                               \
         buff=(STYPE*)getShmem()->shmem_malloc(2*sizeof(STYPE));        \
      }                                                               \
      buff[0]=myval;                                                  \
      getShmem()->SHMEM_FUNC(&buff[1], buff,1,0,1,shmem_n_pes(),work,sync);        \
      lgp_barrier();    \
      XTYPE ret = buff[1];   \
      return ret;        \
   }

Define_Reducer(lgp_reduce_add_l, int64_t, int64_t, long, shmem_long_sum_to_all)
Define_Reducer(lgp_reduce_min_l, int64_t, int64_t, long, shmem_long_min_to_all)
Define_Reducer(lgp_reduce_max_l, int64_t, int64_t, long, shmem_long_max_to_all)

Define_Reducer(lgp_reduce_add_d, double, double, double, shmem_double_sum_to_all)
Define_Reducer(lgp_reduce_min_d, double, double, double, shmem_double_min_to_all)
Define_Reducer(lgp_reduce_max_d, double, double, double, shmem_double_max_to_all)



