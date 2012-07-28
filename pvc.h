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

/**
 * @defgroup modpvc Producer vs. Consumer 
 * A simple interface to handle produce and consume logic, 
 * named PVC. 
 *  
 * A producer is a function which generates one data block on 
 * succeed per call. 
 *  
 * A consumer is a function which consume one data block on 
 * succeed per call. 
 *  
 * A PVC is focus on the connection between a set of producers 
 * and the corresponding consumers. Data from all producers run
 * into a uniq ring-buffer and will be passed to all consumers.
 *  
 * PVC can be chained up. e.g. task #1 produces a set of number, 
 * task #2 gets the number and double it, task #3 shows the 
 * result. To solve this, we need 2 chained PVC. One for double, 
 * the other for showing. Code like this: @code 
 * pvc_x = pvc_open( nx ); 
 * pvc_y = pvc_open( ny ); 
 *  
 * pvc_add_producer( pvc_x, get_number, n1 ); 
 * pvc_chain( pvc_x, pvc_y, double_number, n2 ); 
 * pvc_add_consumer( pvc_y, show_number, n3 ); 
 *  
 * pvc_start( pvc_y, ctx ); 
 * pvc_start( pvc_x, ctx ); 
 *  
 * wait_for_user_quit(); 
 *  
 * pvc_stop( pvc_x, delete_number, ctx ); 
 * pvc_stop( pvc_y, show_number, ctx ); 
 *  
 * pvc_close( pvc_x ); 
 * pvc_close( pvc_y ); 
 * @endcode 
 *  
 * @note to let all data go through regular flow as much as 
 *       possible, we start downstream PVC first and stop
 *       upstream first.
 *  
 * @version 0.2.0 
 */
/*@{*/

/**
 * PVC handler type
 */
typedef struct pvc_s * pvc_t;

/**
 * PVC thread type
 */
typedef enum {
   _PVC_UNKNOWN_TYPE = 0, /// avoid uninited type
    PVC_PRODUCER,
    PVC_CONSUMER,
    PVC_CHAINED_PRODUCER,
    PVC_CHAINED_CONSUMER,
    PVC_OTHER,
} pvc_type_t;

/**
 * PVC infomation type
 */
typedef struct {
    pvc_type_t type;
    unsigned int index, sub_index;
    unsigned int n_round, n_elem;
} pvc_info_t;

/**
 * PVC producer callback type 
 *  
 * the producer can take an arbitrary data pointer as a 
 * context informatin, which named \c arg, and generate 
 * a data buffer pointer stored in \c pdata.
 *  
 * return value of producer should be 0 when succeed, 
 * otherwise when failed. 
 *  
 * @todo to handle the return value 
 */
typedef int (*pvc_cb_produce_func_t)( void *arg, void **pdata );
/**
 * PVC consumer callback type 
 *  
 * the consumer can take an arbitrary data pointer as a 
 * context informatin, which named \c arg, and a data 
 * buffer pointer \c data to process with.
 *  
 * return value of consumer should be 0 when succeed, 
 * otherwise when failed. 
 *  
 * @todo to handle the return value 
 */
typedef int (*pvc_cb_consume_func_t)( void *arg, void *data );
/**
 * PVC chained up callback type 
 *  
 * a chained up function behavious exactly like a 
 * migration of a consumer and a producer. besides an 
 * arbitrary data pointer as context infomation, which 
 * takes a old data block, process it to be a new data 
 * block and give out from the original input parameter 
 * \c pdata.
 *  
 * return value of chained up function should be 0 when 
 * succeed, otherwise when failed.
 *  
 * @todo to handle the return value 
 */
typedef int (*pvc_cb_chain_func_t)( void *arg, void **pdata );

/**
 * open a PVC, with ring-buffer has given elements.
 * 
 * @param max_elems the ring-buffer capabiliy
 * 
 * @return pvc_t the PVC just opened
 */
pvc_t pvc_open( size_t max_elems );
/**
 * close a PVC.
 * 
 * @param pvc the PVC to close
 */
void pvc_close( pvc_t pvc );

/**
 * start all jobs of a PVC
 * 
 * @param pvc the PVC to start
 * @param arg an arbitrary data pointer to be passed to all 
 *            callbacks
 * 
 * @return int 
 */
int pvc_start( pvc_t pvc, void *arg );
/**
 * stop all jobs of a PVC
 * 
 * @param pvc the PVC to stop
 * @param func the cleanup function to handle all data left 
 *             un-consumed
 * @param arg an arbitrary data pointer to be passed to all 
 *            callbacks
 * 
 * @return int 
 */
int pvc_stop( pvc_t pvc, pvc_cb_consume_func_t func, void *arg );

/**
 * register a set of producer into a PVC
 * 
 * @param pvc the PVC to operate
 * @param func the producer callback function
 * @param count count of this producers
 * 
 * @return int 
 */
int pvc_add_producer( pvc_t pvc, pvc_cb_produce_func_t func, int count );
/**
 * register a set of consumer into a PVC
 * 
 * @param pvc the PVC to operate
 * @param func the consumer callback function
 * @param count count of this consumers
 * 
 * @return int 
 */
int pvc_add_consumer( pvc_t pvc, pvc_cb_consume_func_t func, int count );
/**
 * register a set of chained up jobs into a PVC
 * 
 * @param pvc the PVC to operate
 * @param func the chained up callback function
 * @param count count of this chained up jobs
 * 
 * @return int 
 */
int pvc_chain( pvc_t src, pvc_t dst, pvc_cb_chain_func_t func, int count );

/**
 * to query the active PVC job infomation in a callback
 * 
 * @return const pvc_info_t* the job infomation
 */
const pvc_info_t * pvc_get_info( void );

/*@}*/

#endif /* _PVC_H_ */
