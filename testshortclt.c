/*
 * =====================================================================================
 *
 *       Filename:  testshortclt.c
 *
 *    Description:  Short Connection Client
 *
 *        Version:  1.0
 *        Created:  2012/06/16 16时16分52秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Levi G (yxguo), yxguo@wisvideo.com.cn
 *   Organization:  WisVideo
 *
 * =====================================================================================
 */
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "pvc.h"

typedef struct {
    int *running;
    int *threads;
    struct sockaddr_in toaddr;
    uint32_t gen_seq, xmit_seq, recv_seq;
    int counter_p, counter_x, counter_c;
} context_t;

#if 0
void *xmit_thread( void *arg )
{
    context_t * const ctx = arg;
    int fd = socket( AF_INET, SOCK_STREAM, 0 );
    if ( fd < 0 ) {
        printf( "Err: socket(IPv4,TCP): E%d: %s\n", errno, strerror( errno ) );
        exit( -1 );
    } else {
        if ( connect( fd, &ctx->toaddr, sizeof(ctx->toaddr) ) ) {
            printf( "Err: connect(): E%d: %s\n", errno, strerror( errno ) );
            exit( -1 );
        }
    }

    return NULL;
}
#endif

typedef struct {
    size_t len;
    uint32_t tags[3];
} data_t;

static int produce_data( void *ctx, void **pdata )
{
    const pvc_info_t * const info = pvc_get_info();
    context_t * const c = ctx;
    data_t *value = calloc( 1, sizeof(data_t) );
    value->len = sizeof(*value->tags);
    value->tags[0] = c->gen_seq++;
    *pdata = value;
    printf( "P#%d:\tthread #%d(P%d): tid=%p, produce: l=%zd, data={%u,%u}\n", ++c->counter_p, info->index, info->sub_index, pthread_self(), value->len, value->tags[0], value->tags[1] );
    //usleep( 997 ); // to simulate I/O blocking
    return 0;
}
static int xmit_data( void *ctx, void **pdata )
{
    const pvc_info_t * const info = pvc_get_info();
    context_t * const c = ctx;
    data_t *value = *pdata;
    // do nothing, just hand over data
    value->len += sizeof(*value->tags);
    value->tags[1] = c->xmit_seq++;
    printf( "X#%d:\tthread #%d(X%d): tid=%p, handover: l=%zd, data={%u,%u}\n", ++c->counter_x, info->index, info->sub_index, pthread_self(), value->len, value->tags[0], value->tags[1] );
    //usleep( 997 ); // to simulate I/O blocking
    return 0;
}
static int consume_data( void *ctx, void *data )
{
    const pvc_info_t * const info = pvc_get_info();
    context_t * const c = ctx;
    data_t *value = data;
    //usleep( 1313 ); // to simulate I/O blocking
    printf( "C#%d:\tthread #%d(C%d): tid=%p, consume: l=%zd, data={%u,%u}\n", ++c->counter_c, info->index, info->sub_index, pthread_self(), value->len, value->tags[0], value->tags[1] );
    free( value );
    return 0;
}
static int cleanup_data( void *ctx, void *data )
{
    const pvc_info_t * const info = pvc_get_info();
    context_t * const c = ctx;
    data_t *value = data;
    //usleep( 1313 ); // to simulate I/O blocking
    printf( "-#%d:\tthread #%d(-%d): tid=%p, consume: l=%zd, data={%u,%u}\n", ++c->counter_c, info->index, info->sub_index, pthread_self(), value->len, value->tags[0], value->tags[1] );
    free( value );
    return 0;
}

static int *global_running = NULL;
static void sig_notify( int sig )
{
    printf( "caught signal %s(%d)\n", strsignal( sig ), sig );
    if ( global_running && *global_running ) {
        *global_running = 0;
        printf( "set global stop.\n" );
        global_running = NULL;
    }
}
static void handle_signals( void (*func)( int ), ... )
{
    int sig;
    va_list ap;
    va_start( ap, func );
    while ( (sig = va_arg( ap, int )) != 0 )
        signal( sig, func );
    va_end( ap );
}

int main( int argc, char *argv[] )
{
    int running = 1;
    context_t ctx = { &running };
    pvc_t pvc_send, pvc_recv;
    int n_max_elems, n_producer, n_xmitter, n_consumer;

    n_max_elems = 10;
    n_producer = 2;
    n_xmitter = 1;
    n_consumer = 3;

    pvc_send = pvc_open( n_max_elems );
    pvc_recv = pvc_open( n_max_elems );

    pvc_add_producer( pvc_send, produce_data, n_producer );
    pvc_chain( pvc_send, pvc_recv, xmit_data, n_xmitter );
    pvc_add_consumer( pvc_recv, consume_data, n_consumer );

    global_running = ctx.running;
    handle_signals( sig_notify, SIGINT, SIGTERM, SIGQUIT, 0 );

    pvc_start( pvc_recv, &ctx );
    pvc_start( pvc_send, &ctx );

    usleep( 1000 );

    pvc_stop( pvc_send, cleanup_data, &ctx ); // have to stop source chain first
    pvc_stop( pvc_recv, cleanup_data, &ctx );

    pvc_close( pvc_send );
    pvc_close( pvc_recv );

    return 0;
}
