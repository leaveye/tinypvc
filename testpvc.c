/*
 * =====================================================================================
 *
 *       Filename:  testpvc.c
 *
 *    Description:  Test PvC module
 *
 *        Version:  1.0
 *        Created:  2012/06/12 21时30分49秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Levi G (yxguo), yxguo@wisvideo.com.cn
 *   Organization:  WisVideo
 *
 * =====================================================================================
 */
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "pvc.h"

#if PROFILE
#ifdef printf
#undef printf
#endif
#define printf(fmt,args...) ((void)(fmt,##args))
#endif

typedef struct {
    int running;
    int acc_max;
    int acc;
    int counter_p;
    int counter_c;
} prog_context_t;

static int produce_data( void *ctx, void **pdata )
{
    const pvc_info_t * const info = pvc_get_info();
    prog_context_t * const c = ctx;
    int *value = malloc( sizeof(int) );
    *value = 1 + c->acc;
    c->acc = ( c->acc + 1 ) % c->acc_max;
    *pdata = value;
    printf( "P#%d:\tthread #%d(P%d): tid=%p, produce %d(%p)\n", ++c->counter_p, info->index, info->sub_index, pthread_self(), *value, value );
    //usleep( 997 ); // to simulate I/O blocking
    return 0;
}
static int consume_data( void *ctx, void *data )
{
    const pvc_info_t * const info = pvc_get_info();
    prog_context_t * const c = ctx;
    int *value = data;
    //usleep( 1313 ); // to simulate I/O blocking
    printf( "C#%d:\tthread #%d(C%d): tid=%p, consume %d(%p)\n", ++c->counter_c, info->index, info->sub_index, pthread_self(), *value, value );
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

#if PROFILE
#undef printf
#endif

int main( int argc, char *argv[] )
{
    prog_context_t ctx = { 1 };
    size_t n_max_elems;
    int n_producer, n_consumer;
    pvc_t pvc;

    setbuf( stdout, NULL );

    if ( argc > 1 && !strcmp( argv[ argc - 1 ], "-h" ) ) {
        printf( "Usage: %s [NPROD] [NCONS] [ELEMS] [ACCMAX]\n", argv[0] );
        exit( 0 );
    }

    n_producer = argc > 1 ? atoi( argv[1] ) : 6;
    n_consumer = argc > 2 ? atoi( argv[2] ) : 10;
    n_max_elems = argc > 3 ? atoi( argv[3] ) : 4;
    ctx.acc_max = argc > 4 ? atoi( argv[4] ) : 40;

    assert( n_producer > 0 );
    assert( n_consumer > 0 );
    assert( n_max_elems > 0 );
    assert( ctx.acc_max > 0 );

    printf( "using pvc: rb-max-elems=%zd, producers=%d, consumers=%d\n", n_max_elems, n_producer, n_consumer );

    pvc = pvc_open( n_max_elems );

    pvc_add_producer( pvc, produce_data, n_producer );
    pvc_add_consumer( pvc, consume_data, n_consumer );

    global_running = &ctx.running;
    handle_signals( sig_notify, SIGINT, SIGTERM, SIGQUIT, 0 );

    pvc_start( pvc, &ctx );

    //while ( ctx.running )
        usleep( 5000 );

    pvc_stop( pvc, consume_data, &ctx );

    printf( "summary: produced=%d, consumed=%d\n", ctx.counter_p, ctx.counter_c );

    pvc_close( pvc );

    if ( ctx.counter_c != ctx.counter_p )
        exit( -1 );

    return 0;
}

