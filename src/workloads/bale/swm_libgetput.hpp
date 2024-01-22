/*
 * swm_libgetput.hpp
 *
 * Libgetput implementation from Bale benchmark suite
 * for Booksim scalable workload model 
 *
 *  Created on: Dec 24, 2022
 *      Author: hdogan
*/



#ifndef libgetput_INCLUDED
#define libgetput_INCLUDED  /*!< std trick */



#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <locale.h>
#include <sys/time.h>
#include "swm_globals.hpp"
#include "swm_shmem.hpp"

#define BALE_VERSION 3.0

#define SHARED
#define lgp_local_part(t,p) (( t * )(p)) /*!< wrapper for localizing a global pointer */
#define T0_printf if(MYTHREAD==0) printf /*!< only execute the printf from thread 0 */
#define T0_fprintf if(MYTHREAD==0) fprintf /*!< only execute the fprintf from thread 0 */
//#define shmem_n_pes() so.shmem_n_pes()



class SwmLibgetput
{

   public:
   
   int MYTHREAD; 
   int THREADS; 
   SwmShmem so;

   SwmShmem * getShmem() { return &so; } 
   int64_t getTime() { return so.getTime(); }

   void addSimWork(int64_t work) { so.getSimThread()->WORK(work); }

   int shmem_n_pes() { return so.shmem_n_pes(); }

   void lgp_init(SwmThread * thread, int me, int np);
   void lgp_finalize() {}


   void lgp_barrier() { so.shmem_barrier_all(); } /*!< wrapper for global barrier */
   void lgp_fence() { so.shmem_quiet(); } /*!< wrapper for memory fence */
   void lgp_global_exit(int pe) {}
   void lgp_all_free(void * addr) {}
   void *lgp_all_alloc(int64_t nblock_on_all_threads, size_t blocksize) { return so.shmem_malloc( ( ( (size_t)(nblock_on_all_threads)+(so.shmem_n_pes())-1)/(so.shmem_n_pes()) ) *(blocksize) ); }

   void     lgp_shmem_write_upc_array_int64(SHARED int64_t *addr, size_t index, size_t blocksize, int64_t val); /*!< macro magic */
   void     lgp_shmem_write_upc_array_double(SHARED double *addr, size_t index, size_t blocksize, double val); /*!< macro magic */
   int64_t  lgp_shmem_read_upc_array_int64(const SHARED int64_t *addr, size_t index, size_t blocksize); /*!< macro magic */
   double   lgp_shmem_read_upc_array_double(const SHARED double *addr, size_t index, size_t blocksize); /*!< macro magic */
   int64_t  lgp_shmem_read_upc_array_int64_nbi(const SHARED int64_t *addr, size_t index, size_t blocksize); /*!< macro magic */
   double   lgp_shmem_read_upc_array_double_nbi(const SHARED double *addr, size_t index, size_t blocksize); /*!< macro magic */

   void     lgp_put_int64(int64_t * array, size_t index, int64_t val) { lgp_shmem_write_upc_array_int64((array),(index),sizeof(int64_t),(val)); } /*!< user callable global single word put */
   void     lgp_put_double(double * array, size_t index, double  val) { lgp_shmem_write_upc_array_double((array),(index),sizeof(double),(val)); } /*!< user callable global single word put */
   int64_t  lgp_get_int64(int64_t * array, size_t index) { return lgp_shmem_read_upc_array_int64((array),(index),sizeof(int64_t)); } /*!< user callable global single word get */
   double   lgp_get_double(double * array, size_t index) { return lgp_shmem_read_upc_array_double((array),(index),sizeof(double)); } /*!< user callable global single word get */
   int64_t  lgp_get_int64_nbi(int64_t * array, size_t index) { return lgp_shmem_read_upc_array_int64_nbi((array),(index),sizeof(int64_t)); } /*!< user callable global single word get */
   double   lgp_get_double_nbi(double * array, size_t index) { return lgp_shmem_read_upc_array_double_nbi((array),(index),sizeof(double)); } /*!< user callable global single word get */


   int64_t  lgp_prior_add_l(int64_t x); 
   int64_t  lgp_partial_add_l(int64_t x); 
   void     lgp_rand_seed(int64_t seed);
   int64_t  lgp_rand_int64(int64_t N);
   double   lgp_rand_double();

   ///////////////////////////
   // ATOMICS
   ///////////////////////////
   void lgp_atomic_add_async(SHARED int64_t * ptr, int64_t index, int64_t value); /*!< wrapper for non-blocking atomic add */
   void       lgp_atomic_add(SHARED int64_t * ptr, int64_t index, int64_t value); /*!< wrapper of atomic add */
   int64_t lgp_fetch_and_inc(SHARED int64_t * ptr, int64_t index); /*!< wrapper of atomic fetch and inc */
   int64_t lgp_fetch_and_add(SHARED int64_t * ptr, int64_t index, int64_t value); /*!< wrapper of atomic fetch and add */
   int64_t lgp_cmp_and_swap(SHARED int64_t * ptr, int64_t index, int64_t cmp_val, int64_t swap_val); /*!< wrapper of atomic compare and swap */
   void    lgp_atomic_add_nbi(SHARED int64_t * ptr, int64_t index, int64_t value); /*!< wrapper of atomic add */
   int64_t lgp_fetch_and_inc_nbi(SHARED int64_t * ptr, int64_t index); /*!< wrapper of atomic fetch and inc */
   int64_t lgp_fetch_and_add_nbi(SHARED int64_t * ptr, int64_t index, int64_t value); /*!< wrapper of atomic fetch and add */


   int64_t lgp_reduce_add_l_(int64_t myval); /*!< collective reduction add on int64_t's */
   int64_t lgp_reduce_add_l(int64_t myval); /*!< collective reduction add on int64_t's */
   int64_t lgp_reduce_min_l(int64_t myval); /*!< collective reduction min on int64_t's */
   int64_t lgp_reduce_max_l(int64_t myval); /*!< collective reduction max on int64_t's */

   double lgp_reduce_add_d(double myval); /*!< collective reduction add on double's */
   double lgp_reduce_min_d(double myval); /*!< collective reduction min on double's */
   double lgp_reduce_max_d(double myval); /*!< collective reduction max on double's */

   /*!< wrappers to make lgp_memput and lgp_memget duplicate the array indexing arithmetic of UPC shared arrays. */
   void lgp_memput_shmemwrap(void * dst, void * src, int n, int pe) {
      (n)==0 ? (void)(0) :                                                
         (n)%16==0 ? so.shmem_put128((dst),(src),(n)/16,(pe)) :                 
         (n)%8==0 ? so.shmem_put64((dst),(src),(n)/8,(pe)) :    
         (n)%4==0 ? so.shmem_put32((dst),(src),(n)/4,(pe)) :                  
         so.shmem_putmem((dst),(src),(n),(pe)); 
   }
   void lgp_memput_bytes_by_pe(void * dst, void * src, int n, int local_offset, int pe) {
      lgp_memput_shmemwrap( ((char*)(dst))+(local_offset), (src), (n), (pe) );  
   }
   template<typename T>
   void lgp_memput(T * dst, T * src, int n, int index) {
      lgp_memput_bytes_by_pe( (dst), (src), (n),  sizeof(*(dst))*(((size_t)(index))/shmem_n_pes()), (index)%shmem_n_pes() );
   }


   /*!< wrappers to make lgp_memput and lgp_memget duplicate the array indexing arithmetic of UPC shared arrays. */
   void lgp_memget_shmemwrap(void * dst, void * src, int n, int pe) {
      (n)==0 ? (void)(0) :                                                
         (n)%16==0 ? so.shmem_get128((dst),(src),(n)/16,(pe)) :                 
         (n)%8==0 ? so.shmem_get64((dst),(src),(n)/8,(pe)) :    
         (n)%4==0 ? so.shmem_get32((dst),(src),(n)/4,(pe)) :                  
         so.shmem_getmem((dst),(src),(n),(pe)); 
   }
   void lgp_memget_bytes_by_pe(void * dst, void * src, int n, int local_offset, int pe) {
      lgp_memget_shmemwrap( dst, ((char*)(src))+(local_offset), (n), (pe) );  
   }
   template<typename T>
   void lgp_memget(T * dst, T * src, int n, int index) {
      lgp_memget_bytes_by_pe( (dst), (src), (n),  sizeof(*(dst))*(((size_t)(index))/shmem_n_pes()), (index)%shmem_n_pes() );
   }
   
   /*!< wrappers to make lgp_memput and lgp_memget duplicate the array indexing arithmetic of UPC shared arrays. */
   void lgp_memget_nbi_shmemwrap(void * dst, void * src, int n, int pe) {
      (n)==0 ? (void)(0) :                                                
         (n)%16==0 ? so.shmem_get128_nbi((dst),(src),(n)/16,(pe)) :                 
         (n)%8==0 ? so.shmem_get64_nbi((dst),(src),(n)/8,(pe)) :    
         (n)%4==0 ? so.shmem_get32_nbi((dst),(src),(n)/4,(pe)) :                  
         so.shmem_getmem_nbi((dst),(src),(n),(pe)); 
   }
   void lgp_memget_nbi_bytes_by_pe(void * dst, void * src, int n, int local_offset, int pe) {
      lgp_memget_nbi_shmemwrap( dst, ((char*)(src))+(local_offset), (n), (pe) );  
   }
   template<typename T>
   void lgp_memget_nbi(T * dst, T * src, int n, int index) {
      lgp_memget_nbi_bytes_by_pe( (dst), (src), (n),  sizeof(*(dst))*(((size_t)(index))/shmem_n_pes()), (index)%shmem_n_pes() );
   }

};


#endif
