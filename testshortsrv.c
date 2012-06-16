/*
 * =====================================================================================
 *
 *       Filename:  testshortsrv.c
 *
 *    Description:  Short Connection Server
 *
 *        Version:  1.0
 *        Created:  2012/06/16 16时16分23秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Levi G (yxguo), yxguo@wisvideo.com.cn
 *   Organization:  WisVideo
 *
 * =====================================================================================
 */
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <pthread.h>

typedef struct {
    int *running;
    int *threads;
    int fd;
} context_t;

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

int get_timestamp( void *buf, size_t size )
{
    struct timeval *data = buf;
    if ( size < sizeof(*data) )
        return -1;
    gettimeofday( data, NULL );
    return sizeof(*data);
}

void eliminate_connection( int *fd )
{
    if ( !fd || *fd <= 0 ) return;
    shutdown( *fd, SHUT_RDWR );
    close( *fd );
    *fd = 0;
}

void *server_thread( void *arg )
{
    context_t * const ctx = arg;

    if ( ctx->threads )
        ++*ctx->threads; // assert atomic

    if ( ctx->running && *ctx->running ) do {
        char buf[0x1000];
        size_t bufsz = sizeof(buf), buflen = 0;
        int ret;

        ret = recv( ctx->fd, buf, bufsz, 0 );
        if ( ret < 0 ) {
            printf( "Err: recv(%d): E%d: %s\n", ctx->fd, errno, strerror( errno ) );
            break;
        } else if ( ret == 0 )
            break;

        buflen = ret;
        ret = get_timestamp( buf+buflen, bufsz-buflen );
        if ( ret < 0 ) {
            printf( "Err: get_timestamp(%p+%zd): E%d: %s\n", buf+buflen, bufsz-buflen, errno, strerror( errno ) );
            break;
        }
        buflen += ret;

        ret = send( ctx->fd, buf, buflen, 0 );
        if ( ret < 0 ) {
            printf( "Err: send(%d): E%d: %s\n", ctx->fd, errno, strerror( errno ) );
            break;
        } else if ( ret == 0 )
            break;
    } while (0);

    eliminate_connection( &ctx->fd );

    free( ctx );

    if ( ctx->threads )
        --*ctx->threads; // assert atomic

    return NULL;
}
void *listen_thread( void *arg )
{
    context_t * const ctx = arg;
    context_t * sub_ctx = NULL;
    int ret;

    if ( ctx->threads )
        ++*ctx->threads; // assert atomic

    while ( ctx->running && *ctx->running ) {
        pthread_t tid;
        struct sockaddr addr;
        socklen_t addrlen = sizeof(addr);
        int fd = accept( ctx->fd, &addr, &addrlen );
        if ( fd < 0 ) {
            printf( "Err: accept(%d): E%d: %s\n", ctx->fd, errno, strerror( errno ) );
            if ( errno == EAGAIN )
                continue;
            break;
        }

        sub_ctx = calloc( 1, sizeof(*sub_ctx) );
        sub_ctx->running = ctx->running;
        sub_ctx->threads = ctx->threads;
        sub_ctx->fd = fd;
        ret = pthread_create( &tid, NULL, server_thread, sub_ctx );
        if ( ret ) {
            printf( "Err: pthread_create(fd=%d): E%d: %s\n", fd, ret, strerror( ret ) );
            free( sub_ctx );
        }
        sub_ctx = NULL;
    }

    if ( ctx->threads ) {
        while ( *ctx->threads > 1 )
            usleep( 100 );
        --*ctx->threads; // assert atomic
    }

    return NULL;
}

int main( int argc, char *argv[] )
{
    int running = 1, threads = 0;
    context_t ctx = { &running, &threads };
    pthread_t tid;
    int fd_listen = 0;
    int port;
    int ret;

    if ( argc > 1 && !strcmp( argv[ argc - 1 ], "-h" ) ) {
        printf( "Usage: %s [PORT]\n", argv[0] );
        exit( 0 );
    }

    port = argc > 1 ? atoi( argv[1] ) : 12345;

    fd_listen = socket( AF_INET, SOCK_STREAM, 0 );
    if ( fd_listen < 0 ) {
        printf( "Err: socket(IPv4,TCP): E%d: %s\n", errno, strerror( errno ) );
        exit( -1 );
    } else {
        int on = 1;
        struct sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htons(INADDR_ANY);
        if ( setsockopt( fd_listen, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) ) ) {
            printf( "Err: setsockopt(REUSEADDR): E%d: %s\n", errno, strerror( errno ) );
            exit( -1 );
        }
        if ( bind( fd_listen, (struct sockaddr *)&addr, sizeof(addr) ) < 0 ) {
            printf( "Err: bind(localhost.%d): E%d: %s\n", port, errno, strerror( errno ) );
            exit( -1 );
        }
        if ( listen( fd_listen, 64 ) ) {
            printf( "Err: listen(%d): E%d: %s\n", fd_listen, errno, strerror( errno ) );
            exit( -1 );
        }
        printf( "listening port: %d\n", port );
    }

    global_running = &running;
    handle_signals( sig_notify, SIGINT, SIGTERM, SIGQUIT, 0 );

    ctx.fd = fd_listen;

    ret = pthread_create( &tid, NULL, listen_thread, &ctx );
    if ( ret ) {
        printf( "Err: pthread_create(fd=%d): E%d: %s\n", fd_listen, ret, strerror( ret ) );
        exit( -1 );
    }

    while ( running )
        usleep( 100000 );

    eliminate_connection( &fd_listen );

    pthread_join( tid, NULL );

    return 0;
}
