# TinyPvC

TinyPvC is a small class like API to create a set of pthreads as data
producer and another set as consumers, named from Producer Vs. Consumer.

I call it PVC for short.

# Remarks

1.  All datagrams from one producer or one consumer is treated
    sequencially.

1.  All datagrams is treated identical between multiple producers,
    the sequence of datagram between producers is no garanteed.

    The same to consumer.

# API

## Basic Operation

Use `pvc_open()` function to create a PVC object, and `pvc_close()`
to destroy it.

    pvc_t pvc_open( size_t max_elems );
    void pvc_close( pvc_t pvc );

Once a PVC is configured, you can start the whole engine, and stop it,
then.

    int pvc_start( pvc_t pvc, void *arg );
    int pvc_stop( pvc_t pvc, pvc_cb_consume_func_t func, void *arg );

Configure the PVC means that tell the engine what to be done in one
single iteration. Do this by setting the producer callback functions
and consumer ones.

    int pvc_add_producer( pvc_t pvc, pvc_cb_produce_func_t func, int count );
    int pvc_add_consumer( pvc_t pvc, pvc_cb_consume_func_t func, int count );

In callback functions, you can also get the current PVC iteration info.

    const pvc_info_t * pvc_get_info( void );

## Producer and Consumer callbacks

A producer callback is a function which generates one data block on
succeed one time, which has signature `pvc_cb_produce_func_t`.

    typedef int (*pvc_cb_produce_func_t)( void *arg, void **pdata );

A consumer callback is a function which consumes one data block on
succeed one time, which has signature `pvc_cb_consume_func_t`.

    typedef int (*pvc_cb_consume_func_t)( void *arg, void *data );

## Chain-ed PVC

A chain callback works like a consumer for one PVC, and a producer for
another. Which should has signature `pvc_cb_chain_func_t`.

    typedef int (*pvc_cb_chain_func_t)( void *arg, void **pdata );

To chain-up two PVC, you can use `pvc_chain()` function.

    int pvc_chain( pvc_t src, pvc_t dst, pvc_cb_chain_func_t func, int count );

