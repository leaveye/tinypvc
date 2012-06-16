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
#include <assert.h>
#include <errno.h>
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
#include <sys/time.h>
#include "pvc.h"

#if PROFILE
#ifdef printf
#undef printf
#endif
#define printf(fmt,args...) ((void)(fmt,##args))
#endif

#define MAX_PKT_SIZE 0x1000

typedef struct {
    int *running;
    int *threads;
    struct sockaddr_in toaddr;
    uint32_t gen_seq, xmit_seq, recv_seq;
    int counter_p, counter_x, counter_c;
} context_t;

typedef struct {
    size_t len, size;
    uint8_t data[];
} user_buffer_t;
static inline user_buffer_t * user_buffer_create( size_t size )
{
    user_buffer_t * const p = malloc( sizeof(user_buffer_t) + size );
    assert( p );
    memset( p, 0, sizeof(*p) );
    p->size = size;
    return p;
}
static inline void user_buffer_destroy( user_buffer_t *data )
{
    free( data );
}
static inline int user_buffer_append( user_buffer_t *data, void *buf, size_t len )
{
    if ( data->len + len > data->size )
        return -1;
    memcpy( data->data + data->len, buf, len );
    data->len += len;
    return 0;
}

int get_timestamp( void *buf, size_t size )
{
    struct timeval *data = buf;
    if ( size < sizeof(*data) )
        return -1;
    gettimeofday( data, NULL );
    return sizeof(*data);
}

static struct timeval ts_ref;
#define TS_COPY(src,dst) memcpy( dst, src, sizeof(struct timeval) )
#define TS_FMT() "%.3g"
#define TS_ARG(p) \
((((struct timeval *)p)->tv_usec-ts_ref.tv_usec)*1.0E-3\
+(((struct timeval *)p)->tv_sec-ts_ref.tv_sec)*1.0E3)
#define SHOWDATA(c,caption,idx,info,value,fmt,args...) \
printf( "%c#%d:\tthread #%d(%c%d): tid=%p, %s: l=%zd, " fmt,\
        (c), (idx), ((const pvc_info_t *)(info))->index, (c), ((const pvc_info_t *)(info))->sub_index, pthread_self(),\
        (caption), ((user_buffer_t*)(value))->len, ##args )

static int produce_data( void *ctx, void **pdata )
{
    const pvc_info_t * const info = pvc_get_info();
    context_t * const c = ctx;
    user_buffer_t *value = user_buffer_create( MAX_PKT_SIZE );
    user_buffer_append( value, &c->gen_seq, sizeof(c->gen_seq) );
    value->len += get_timestamp( value->data + value->len, value->size - value->len );
    *pdata = value;
    c->gen_seq++;

    if (1) {
        uint32_t *pseq;
        uint8_t *pts0;
        pseq = (uint32_t*)value->data;
        pts0 = value->data + sizeof(uint32_t);
        SHOWDATA( 'P', "produce", ++c->counter_p, info, value, "seq=%u, ts0=" TS_FMT() "\n", *pseq, TS_ARG(pts0) );
    }

    //usleep( 997 ); // to simulate I/O blocking
    return 0;
}
static int xmit_data( void *ctx, void **pdata )
{
    const pvc_info_t * const info = pvc_get_info();
    context_t * const c = ctx;
    user_buffer_t *value = *pdata;
    size_t recved_len;
    int fd, ret;

    fd = socket( AF_INET, SOCK_STREAM, 0 );
    if ( fd < 0 ) {
        printf( "Err: socket(IPv4,TCP): E%d: %s\n", errno, strerror( errno ) );
        exit( -1 );
    } else if ( connect( fd, (struct sockaddr *)&c->toaddr, sizeof(c->toaddr) ) ) {
        printf( "Err: connect(): E%d: %s\n", errno, strerror( errno ) );
        exit( -1 );
    }

    if (1) {
        uint32_t *pseq;
        uint8_t *pts0;
        pseq = (uint32_t*)value->data;
        pts0 = value->data + sizeof(uint32_t);
        SHOWDATA( 'X', "send", ++c->counter_x, info, value, "seq=%u, ts0=" TS_FMT() "\n", *pseq, TS_ARG(pts0) );
    }

    //usleep( 997 ); // to simulate I/O blocking
    ret = send( fd, &value->len, sizeof(value->len), 0 );
    assert( ret == sizeof(value->len) );
    ret = send( fd, value->data, value->len, 0 );
    assert( ret == value->len );
    ret = recv( fd, &value->len, sizeof(value->len), 0 );
    assert( ret == sizeof(value->len) );
    assert( value->len <= value->size );
    recved_len = 0;
    do {
        ret = recv( fd, value->data + recved_len, value->len - recved_len, 0 );
        if ( ret < 0 ) {
            printf( "Err: recv(): E%d: %s\n", errno, strerror( errno ) );
            exit( -1 );
        } else if ( ret == 0 ) {
            printf( "Err: recv(): %s\n", "connection broken" );
            exit( -1 );
        }
        recved_len += ret;
    } while ( recved_len < value->len );

    if (1) {
        uint32_t *pseq;
        uint8_t *pts0, *pts1;
        pseq = (uint32_t*)value->data;
        pts0 = value->data + sizeof(uint32_t);
        pts1 = pts0 + sizeof(uint32_t);
        SHOWDATA( 'X', "send", ++c->counter_x, info, value, "seq=%u, ts0=" TS_FMT() ", ts1=" TS_FMT() "\n", *pseq, TS_ARG(pts0), TS_ARG(pts1) );
    }

    return 0;
}
static int consume_data( void *ctx, void *data )
{
    const pvc_info_t * const info = pvc_get_info();
    context_t * const c = ctx;
    user_buffer_t *value = data;

    //usleep( 1313 ); // to simulate I/O blocking

    if (1) {
        uint32_t *pseq;
        uint8_t *pts0, *pts1;
        pseq = (uint32_t*)value->data;
        pts0 = value->data + sizeof(uint32_t);
        pts1 = pts0 + sizeof(uint32_t);
        SHOWDATA( 'X', "send", ++c->counter_x, info, value, "seq=%u, ts0=" TS_FMT() ", ts1=" TS_FMT() "\n", *pseq, TS_ARG(pts0), TS_ARG(pts1) );
    }

    free( value );
    return 0;
}
static int cleanup_data( void *ctx, void *data )
{
    const pvc_info_t * const info = pvc_get_info();
    context_t * const c = ctx;
    user_buffer_t *value = data;
    //usleep( 1313 ); // to simulate I/O blocking
    printf( "-#%d:\tthread #%d(-%d): tid=%p, consume: l=%zd, data={%u,%u,%u}\n", ++c->counter_c, info->index, info->sub_index, pthread_self(), value->len, value->data[0], value->data[1], value->data[2] );
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
    int port;
    char * ipaddr;

    setbuf( stdout, NULL );

    if ( argc > 1 && !strcmp( argv[ argc - 1 ], "-h" ) ) {
        printf( "Usage: %s [NPROD] [NXMIT] [NCONS] [ELEMS]\n", argv[0] );
        exit( 0 );
    }

    ipaddr = argc > 1 ? argv[1] : "127.0.0.1";
    port = argc > 2 ? atoi( argv[2] ) : 12345;

    n_producer  = argc > 3 ? atoi( argv[3] ) : 6;
    n_xmitter   = argc > 4 ? atoi( argv[4] ) : 4;
    n_consumer  = argc > 5 ? atoi( argv[5] ) : 5;
    n_max_elems = argc > 6 ? atoi( argv[6] ) : 30;

    pvc_send = pvc_open( n_max_elems );
    pvc_recv = pvc_open( n_max_elems );

    pvc_add_producer( pvc_send, produce_data, n_producer );
    pvc_chain( pvc_send, pvc_recv, xmit_data, n_xmitter );
    pvc_add_consumer( pvc_recv, consume_data, n_consumer );

    ctx.toaddr.sin_family = AF_INET;
    ctx.toaddr.sin_port = htons( port );
    inet_aton( ipaddr, &ctx.toaddr.sin_addr );

    gettimeofday( &ts_ref, NULL );

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
