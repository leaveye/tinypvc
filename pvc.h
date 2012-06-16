/*
 * =====================================================================================
 *
 *       Filename:  pvc.h
 *
 *    Description:  Producer vs Consumer
 *
 *        Version:  1.0
 *        Created:  2012/06/12 21时30分45秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Levi G (yxguo), yxguo@wisvideo.com.cn
 *   Organization:  WisVideo
 *
 * =====================================================================================
 */

#ifndef _PVC_H_
#define _PVC_H_

typedef struct pvc_s * pvc_t;

typedef enum {
   _PVC_UNKNOWN_TYPE = 0, // avoid uninited type
    PVC_PRODUCER,
    PVC_CONSUMER,
    PVC_CHAINED_PRODUCER,
    PVC_CHAINED_CONSUMER,
    PVC_OTHER,
} pvc_type_t;

typedef struct {
    pvc_type_t type;
    unsigned int index, sub_index;
    unsigned int n_round, n_elem;
} pvc_info_t;

typedef int (*pvc_cb_produce_func_t)( void *arg, void **pdata );
typedef int (*pvc_cb_consume_func_t)( void *arg, void *data );
typedef int (*pvc_cb_chain_func_t)( void *arg, void **pdata );

pvc_t pvc_open( size_t max_elems );
void pvc_close( pvc_t pvc );

int pvc_start( pvc_t pvc, void *arg );
int pvc_stop( pvc_t pvc, pvc_cb_consume_func_t func, void *arg );

int pvc_add_producer( pvc_t pvc, pvc_cb_produce_func_t func, int count );
int pvc_add_consumer( pvc_t pvc, pvc_cb_consume_func_t func, int count );
int pvc_chain( pvc_t src, pvc_t dst, pvc_cb_chain_func_t func, int count );

const pvc_info_t * pvc_get_info( void );

#endif /* _PVC_H_ */
