/*
 * =====================================================================================
 *
 *       Filename:  pvc.c
 *
 *    Description:  Producer vs Consumer
 *
 *        Version:  1.0
 *        Created:  2012/06/12 21时30分40秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Levi G (yxguo), yxguo@wisvideo.com.cn
 *   Organization:  WisVideo
 * 
 *           Bugs:  Lack of checking some job results, such as
 *                  user producers and consumers, ring buffer
 *                  operations and much of the pthread operations.
 *
 * =====================================================================================
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "data.h"
#include "pvc.h"

typedef struct {
    void **elems;
    size_t size, len;
    size_t head, tail;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty, not_full;
    int user_count;
} ring_buffer_t;

typedef struct {
    pthread_t tid;
    void *ret;
    volatile int *status;
    pvc_info_t info;
    ring_buffer_t *ring_buffer;
    void *callback;
    pthread_mutex_t *callback_mutex;
    void *arg;
} thread_context_t;

#define PVC_STATUS_CONSUMER_RUNNING 0x01
#define PVC_STATUS_PRODUCER_RUNNING 0x02
#define PVC_STATUS_CLEANNING 0x8000

struct pvc_s {
    int status;
    c_linklist_t * thread_contexts;
    ring_buffer_t ring_buffer;
    pthread_mutex_t mutex_producer;
    pthread_mutex_t mutex_consumer;
    unsigned int n_producer, n_consumer;
};

static pthread_once_t _pvc_once = PTHREAD_ONCE_INIT;
static pthread_key_t _pvc_info_key;

int ring_buffer_empty( ring_buffer_t *rb )
{
    pthread_mutex_t * const mutex = &rb->mutex;
    int result;

    pthread_mutex_lock( mutex );
    result = (rb->head == rb->tail) ? 1 : 0;
    pthread_mutex_unlock( mutex );

    return result;
}
int ring_buffer_full( ring_buffer_t *rb )
{
    pthread_mutex_t * const mutex = &rb->mutex;
    size_t tmp;
    int result;

    pthread_mutex_lock( mutex );
    tmp = rb->tail + 1 + rb->size - rb->head;
    result = (tmp == 0 || tmp == rb->size) ? 1 : 0;
    pthread_mutex_unlock( mutex );

    return result;
}
int ring_buffer_append( ring_buffer_t *rb, void *data )
{
    pthread_mutex_t * const mutex = &rb->mutex;
    int to_signal = 0;

    pthread_mutex_lock( mutex );
    if ( (rb->tail + 1) % rb->size == rb->head ) {
        rb->user_count++;
        pthread_cond_wait( &rb->not_full, mutex );
        rb->user_count--;
    }
    if ( (rb->tail + 1) % rb->size != rb->head ) {
        //printf( "rb: %s elems[%zd]=%p\n", __func__+12, rb->tail, data );
        rb->elems[ rb->tail ] = data;
        rb->tail = (rb->tail + 1) % rb->size;
        to_signal = 1;
    }
    pthread_mutex_unlock( mutex );

    if ( to_signal )
        pthread_cond_signal( &rb->not_empty );

    return 0;
}
int ring_buffer_prepend( ring_buffer_t *rb, void *data )
{
    pthread_mutex_t * const mutex = &rb->mutex;
    int to_signal = 0;

    pthread_mutex_lock( mutex );
    if ( (rb->tail + 1) % rb->size == rb->head ) {
        rb->user_count++;
        pthread_cond_wait( &rb->not_full, mutex );
        rb->user_count--;
    }
    if ( (rb->tail + 1) % rb->size != rb->head ) {
        rb->head = (rb->head + rb->size - 1) % rb->size;
        rb->elems[ rb->head ] = data;
        //printf( "rb: %s elems[%zd]=%p\n", __func__+12, rb->head, data );
        to_signal = 1;
    }
    pthread_mutex_unlock( mutex );

    if ( to_signal )
        pthread_cond_signal( &rb->not_empty );

    return 0;
}
void * ring_buffer_pop( ring_buffer_t *rb )
{
    pthread_mutex_t * const mutex = &rb->mutex;
    int to_signal = 0;
    void * data = NULL;

    pthread_mutex_lock( mutex );
    if ( rb->head == rb->tail ) {
        rb->user_count++;
        pthread_cond_wait( &rb->not_empty, mutex );
        rb->user_count--;
    }
    if ( rb->head != rb->tail ) {
        data = rb->elems[ rb->head ];
        //printf( "rb: %s elems[%zd]=%p\n", __func__+12, rb->head, data );
        rb->head = (rb->head + 1) % rb->size;
        to_signal = 1;
    }
    pthread_mutex_unlock( mutex );

    if ( to_signal )
        pthread_cond_signal( &rb->not_full );

    return data;
}

static void _pvc_init_once( void )
{
    pthread_key_create( &_pvc_info_key, NULL );
}

void pvc_close( pvc_t pvc )
{
    if ( !pvc )
        return;

    pthread_mutex_destroy( &pvc->mutex_producer );
    pthread_mutex_destroy( &pvc->mutex_consumer );

    pthread_mutex_destroy( &pvc->ring_buffer.mutex );
    pthread_cond_destroy( &pvc->ring_buffer.not_empty );
    pthread_cond_destroy( &pvc->ring_buffer.not_full );

    C_linklist_destroy( pvc->thread_contexts );

    free( pvc );
}
pvc_t pvc_open( size_t max_elems )
{
    size_t rb_size = (max_elems + 1) * sizeof(((ring_buffer_t*)NULL)->elems[0]);
    pvc_t pvc = calloc( 1, sizeof( struct pvc_s ) + rb_size );

    assert( pvc );

    pthread_once( &_pvc_once, _pvc_init_once );

    pvc->thread_contexts = C_linklist_create();
    C_linklist_set_destructor( pvc->thread_contexts, free );
    assert( pvc->thread_contexts );

    pvc->ring_buffer.elems = (void**)&pvc[1];
    pvc->ring_buffer.size = max_elems + 1;
    pthread_mutex_init( &pvc->ring_buffer.mutex, NULL );
    pthread_cond_init( &pvc->ring_buffer.not_empty, NULL );
    pthread_cond_init( &pvc->ring_buffer.not_full, NULL );

    pthread_mutex_init( &pvc->mutex_producer, NULL );
    pthread_mutex_init( &pvc->mutex_consumer, NULL );

    return pvc;
}

const pvc_info_t * pvc_get_info( void )
{
    return (const pvc_info_t *) pthread_getspecific( _pvc_info_key );
}

static void * _pvc_producer_thread( void *args )
{
    thread_context_t * const ctx = args;
    ring_buffer_t * const rb = ctx->ring_buffer;
    pvc_cb_produce_func_t produce = ctx->callback;
    void * const arg = ctx->arg;
    pvc_info_t * const info = &ctx->info;
    void * data = NULL;

    pthread_setspecific( _pvc_info_key, info );

    while ( data || ( *ctx->status & PVC_STATUS_PRODUCER_RUNNING ) ) {
        if ( !data ) {
            pthread_mutex_lock( ctx->callback_mutex );
            produce( arg, &data );
            pthread_mutex_unlock( ctx->callback_mutex );
            info->n_round++;
            if ( data/* FIXME: succeed */ )
                info->n_elem++;
        } else {
            ring_buffer_append( rb, data );
            data = NULL;
        }
    }

    assert( data == NULL );

    return NULL;
}
static void * _pvc_chain_thread( void *args )
{
    thread_context_t * const src_ctx = args;
    thread_context_t * const dst_ctx = src_ctx + 1;
    ring_buffer_t * const src_rb = src_ctx->ring_buffer;
    ring_buffer_t * const dst_rb = dst_ctx->ring_buffer;
    pvc_cb_chain_func_t chain = src_ctx->callback;
    void * const arg = src_ctx->arg;
    pvc_info_t * const src_info = &src_ctx->info;
    pvc_info_t * const dst_info = &dst_ctx->info;
    void * data = NULL;

    pthread_setspecific( _pvc_info_key, src_info );

    while ( data ||
            ( ( *src_ctx->status & PVC_STATUS_CONSUMER_RUNNING ) &&
              ( *dst_ctx->status & PVC_STATUS_PRODUCER_RUNNING ) ) ) {
        if ( !data ) {
            data = ring_buffer_pop( src_rb );
            if ( data ) {
                if ( chain ) {
                    pthread_mutex_lock( src_ctx->callback_mutex );
                    chain( arg, &data );
                    pthread_mutex_unlock( src_ctx->callback_mutex );
                }
                src_info->n_round++;
                if ( data/* FIXME: succeed */ )
                    src_info->n_elem++;
            }
        } else {
            dst_info->n_round++;
            if ( data ) {
                dst_info->n_elem++;
                ring_buffer_append( dst_rb, data );
                data = NULL;
            }
        }
    }

    assert( data == NULL );

    return NULL;
}
static void * _pvc_consumer_thread( void *args )
{
    thread_context_t * const ctx = args;
    ring_buffer_t * const rb = ctx->ring_buffer;
    pvc_cb_consume_func_t consume = ctx->callback;
    void * const arg = ctx->arg;
    pvc_info_t * const info = &ctx->info;
    void * data = NULL;

    pthread_setspecific( _pvc_info_key, info );

    while ( data || ( *ctx->status & PVC_STATUS_CONSUMER_RUNNING ) ) {
        if ( !data ) {
            data = ring_buffer_pop( rb );
        } else {
            pthread_mutex_lock( ctx->callback_mutex );
            consume( arg, data );
            pthread_mutex_unlock( ctx->callback_mutex );
            info->n_round++;
            if ( 1/* FIXME: succeed */ ) {
                data = NULL;
                info->n_elem++;
            }
        }
    }

    assert( data == NULL );

    return NULL;
}
static void * _pvc_cleaner_thread( void *args )
{
    thread_context_t * const ctx = args;
    ring_buffer_t * const rb = ctx->ring_buffer;
    void * const arg = ctx->arg;
    pvc_cb_consume_func_t consume = ctx->callback;
    pvc_info_t * const info = &ctx->info;
    void * data = NULL;

    pthread_setspecific( _pvc_info_key, info );

    while ( data || ( *ctx->status & PVC_STATUS_CLEANNING ) ) {
        if ( ctx->ring_buffer->user_count > 0 ) {
            if ( ring_buffer_empty( rb ) ) {
                // unblock all producers
                pthread_cond_broadcast( &rb->not_full );
            } else {
                // unblock all consumers
                pthread_cond_broadcast( &rb->not_empty );
            }
        } else if ( !data ) {
            data = ring_buffer_pop( rb );
        } else {
            pthread_mutex_lock( ctx->callback_mutex );
            consume( arg, data );
            pthread_mutex_unlock( ctx->callback_mutex );
            info->n_round++;
            if ( 1/* FIXME: succeed */ ) {
                data = NULL;
                info->n_elem++;
            }
        }
    }

    return NULL;
}
thread_context_t * _pvc_start_cleaner( pvc_t pvc, pvc_cb_consume_func_t func, void *arg )
{
    thread_context_t * const ctx = calloc( 1, sizeof( thread_context_t ) );
    int ret;

    ctx->ring_buffer = &pvc->ring_buffer;
    ctx->callback = (void*)func;
    ctx->callback_mutex = &pvc->mutex_consumer;
    ctx->status = &pvc->status;
    ctx->info.type = PVC_CONSUMER;
    ctx->arg = arg;
    ctx->info.index = 0; // different from normal
    ctx->info.sub_index = 0; // different from normal

    assert( ! ( pvc->status & PVC_STATUS_CLEANNING ) );
    pvc->status |= PVC_STATUS_CLEANNING;

    ret = pthread_create( &ctx->tid, NULL, _pvc_cleaner_thread, ctx );

    return ctx;
}

int pvc_start( pvc_t pvc, void *arg )
{
    c_linklist_t * const l = pvc->thread_contexts;
    thread_context_t * ctx;
    int i, ret = 0;

    assert( ! ( pvc->status & (PVC_STATUS_PRODUCER_RUNNING|PVC_STATUS_CONSUMER_RUNNING) ) );
    pvc->status |= PVC_STATUS_PRODUCER_RUNNING|PVC_STATUS_CONSUMER_RUNNING;

    for ( C_linklist_move_head( l ), i=0;
          (ctx = C_linklist_restore( l )) != NULL;
          C_linklist_move_next( l ), i++ ) {
        char * const stype = ctx->info.type == PVC_PRODUCER ? "P" :
                             ctx->info.type == PVC_CONSUMER ? "C" :
                             "O";

        ctx->arg = arg;
        ctx->info.index = i + 1;

        switch ( ctx->info.type ) {
        case PVC_PRODUCER:
            ctx->info.sub_index = pvc->n_producer + 1;
            ret = pthread_create( &ctx->tid, NULL, _pvc_producer_thread, ctx );
            pvc->n_producer++;
            break;
        case PVC_CONSUMER:
            ctx->info.sub_index = pvc->n_consumer + 1;
            ret = pthread_create( &ctx->tid, NULL, _pvc_consumer_thread, ctx );
            pvc->n_consumer++;
            break;
        case PVC_CHAINED_PRODUCER:
            // thread maintained by others, we should NOT see it here.
            abort();
            break;
        case PVC_CHAINED_CONSUMER:
            ctx->info.sub_index = pvc->n_consumer + 1;
            ret = pthread_create( &ctx->tid, NULL, _pvc_chain_thread, ctx );
            pvc->n_consumer++;
            break;
        default:
            continue;
        }

        printf( "start:\tthread #%d(%s%d): tid=%p\n", ctx->info.index, stype, ctx->info.sub_index, ctx->tid );
    }
    printf( "start:\ttotal: %d producers, %d consumers\n", pvc->n_producer, pvc->n_consumer );

    return 0;
}
static int _pvc_join_all( pvc_t pvc, pvc_type_t type )
{
    c_linklist_t * const l = pvc->thread_contexts;
    thread_context_t * ctx;
    int i, ret, n_threads = 0;

    C_linklist_move_head( l );
    for ( i=0; (ctx = C_linklist_restore( l )) != NULL; i++ ) {
        if ( ctx->info.type != type ) {
            C_linklist_move_next( l );
            continue;
        }
        switch ( type ) {
        case PVC_PRODUCER:
            {
                char * const stype = "P";

                ret = pthread_join( ctx->tid, &ctx->ret );
                pvc->n_producer--, n_threads++;

                printf( "stop:\tthread #%d(%s%d): tid=%p, round=%u, elems=%u\n", ctx->info.index, stype, ctx->info.sub_index, ctx->tid, ctx->info.n_round, ctx->info.n_elem );

                C_linklist_delete( l );
            }
            break;
        case PVC_CONSUMER:
            {
                char * const stype = "C";

                ret = pthread_join( ctx->tid, &ctx->ret );
                pvc->n_consumer--, n_threads++;

                printf( "stop:\tthread #%d(%s%d): tid=%p, round=%u, elems=%u\n", ctx->info.index, stype, ctx->info.sub_index, ctx->tid, ctx->info.n_round, ctx->info.n_elem );

                C_linklist_delete( l );
            }
            break;
        case PVC_CHAINED_CONSUMER:
            {
                char * const stype = "C";

                ret = pthread_join( ctx->tid, &ctx->ret );
                pvc->n_consumer--, n_threads++;

                printf( "stop:\tthread #%d(%s%d): tid=%p, round=%u, elems=%u\n", ctx->info.index, stype, ctx->info.sub_index, ctx->tid, ctx->info.n_round, ctx->info.n_elem );

                C_linklist_delete( l );
            }
            break;
        default:
            C_linklist_move_next( l );
        }
    }

    return n_threads;
}
int pvc_stop( pvc_t pvc, pvc_cb_consume_func_t func, void *arg )
{
    c_linklist_t * const l = pvc->thread_contexts;
    thread_context_t * cleaner_ctx = NULL;
    int ret = 0, n_producer = 0, n_consumer = 0;

    if ( C_linklist_length( l ) == 0 ) {
        // nothing needs to be stop
        return 0;
    }

    // tell all producer threads to exit
    assert( pvc->status & PVC_STATUS_PRODUCER_RUNNING );
    pvc->status &= ~PVC_STATUS_PRODUCER_RUNNING;

    // start cleaner
    if ( pvc->n_consumer == 0 ) {
        cleaner_ctx = _pvc_start_cleaner( pvc, func, arg );
        assert( cleaner_ctx );
    }

    // join all producer threads
    n_producer = _pvc_join_all( pvc, PVC_PRODUCER );

    // wait for all data in buffer cosumed
    while ( ! ring_buffer_empty( &pvc->ring_buffer ) ||
            pvc->ring_buffer.user_count < pvc->n_consumer )
        usleep( 100 );

    // tell all consumer threads to exit
    assert( pvc->status & PVC_STATUS_CONSUMER_RUNNING );
    pvc->status &= ~PVC_STATUS_CONSUMER_RUNNING;

    // unblock all consumer threads
    pthread_cond_broadcast( &pvc->ring_buffer.not_empty );

    n_consumer = _pvc_join_all( pvc, PVC_CONSUMER );
    n_consumer += _pvc_join_all( pvc, PVC_CHAINED_CONSUMER );

    assert( C_linklist_length( l ) == 0 );

    // stop the cleaner
    if ( cleaner_ctx ) {
        assert( pvc->status & PVC_STATUS_CLEANNING );
        pvc->status &= ~PVC_STATUS_CLEANNING;

        pthread_cond_broadcast( &pvc->ring_buffer.not_empty );

        ret = pthread_join( cleaner_ctx->tid, &cleaner_ctx->ret );

        printf( "stop:\tthread cleaner: tid=%p, round=%u, elems=%u\n", cleaner_ctx->tid, cleaner_ctx->info.n_round, cleaner_ctx->info.n_elem );

        free( cleaner_ctx );
    }

    printf( "stop:\ttotal: %d producers, %d consumers\n", n_producer, n_consumer );

    return 0;
}

static inline int _pvc_add_thread( pvc_t pvc, void *callback, pvc_type_t type, pthread_mutex_t *mutex )
{
    thread_context_t * const ctx = calloc( 1, sizeof( thread_context_t ) );

    assert( ctx );

    ctx->ring_buffer = &pvc->ring_buffer;
    ctx->callback = callback;
    ctx->callback_mutex = mutex;
    ctx->status = &pvc->status;
    ctx->info.type = type;

    C_linklist_append( pvc->thread_contexts, ctx );

    return 0;
}
int pvc_add_producer( pvc_t pvc, pvc_cb_produce_func_t func, int count )
{
    while ( count-- > 0 )
        _pvc_add_thread( pvc, (void*)func, PVC_PRODUCER, &pvc->mutex_producer );
    return 0;
}
int pvc_add_consumer( pvc_t pvc, pvc_cb_consume_func_t func, int count )
{
    while ( count-- > 0 )
        _pvc_add_thread( pvc, (void*)func, PVC_CONSUMER, &pvc->mutex_consumer );
    return 0;
}
int pvc_chain( pvc_t src, pvc_t dst, pvc_cb_chain_func_t func, int count )
{
    while ( count-- > 0 ) {
        thread_context_t * const ctx = calloc( 2, sizeof( thread_context_t ) );

        assert( ctx );

        ctx[0].ring_buffer = &src->ring_buffer;
        ctx[0].callback = func;
        ctx[0].callback_mutex = &src->mutex_consumer;
        ctx[0].status = &src->status;
        ctx[0].info.type = PVC_CHAINED_CONSUMER;

        ctx[1].ring_buffer = &dst->ring_buffer;
        ctx[1].callback = NULL;
        ctx[1].callback_mutex = &dst->mutex_producer;
        ctx[1].status = &dst->status;
        ctx[1].info.type = PVC_CHAINED_PRODUCER;

        C_linklist_append( src->thread_contexts, ctx );
    }
    return 0;
}

