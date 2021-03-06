/*
 * Nitro
 *
 * socket.h - Common socket functions.
 *
 *  -- LICENSE --
 *
 * Copyright 2013 Bump Technologies, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY BUMP TECHNOLOGIES, INC. ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BUMP TECHNOLOGIES, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of Bump Technologies, Inc.
 *
 */
#ifndef NITRO_SOCKET_H
#define NITRO_SOCKET_H

#include "common.h"

#include "buffer.h"
#include "cbuffer.h"
#include "frame.h"
#include "opt.h"
#include "queue.h"
#include "trie.h"

typedef struct nitro_pipe_t *nitro_pipe_t_p;

typedef struct nitro_pipe_t {

    /* Direct send queue */
    nitro_queue_t *q_send;

    /* for TCP sockets */
    ev_io ior;
    ev_io iow;
    int fd;
    uint64_t sub_state_sent;
    uint64_t sub_state_recv;

    /* When we have partial output */
    nitro_frame_t *partial;
    uint8_t *remote_ident;
    nitro_counted_buffer_t *remote_ident_buf;
    char us_handshake;
    char them_handshake;
    char registered;
    uint8_t crypto_cache[crypto_box_BEFORENMBYTES];
    uint8_t nonce_gen[crypto_box_NONCEBYTES];
    uint64_t *nonce_incr;

    nitro_buffer_t *in_buffer;

    void *the_socket;

    struct nitro_pipe_t *prev;
    struct nitro_pipe_t *next;

    uint64_t stat_sent;
    uint64_t stat_recv;
    uint64_t stat_direct;
    uint64_t bytes_sent;
    uint64_t bytes_recv;
    double born;

    char remote_location[50];

    nitro_key_t *sub_keys;

    UT_hash_handle hh;
} nitro_pipe_t;

#define INPROC_PREFIX "inproc://"
#define TCP_PREFIX "tcp://"

#define SOCKET_CALL(s, name, args...) \
    (s->trans == NITRO_SOCKET_TCP ? \
     Stcp_socket_##name(&(s->stype.tcp), ## args) : \
     Sinproc_socket_##name(&(s->stype.inproc), ## args));

#define SOCKET_SET_PARENT(s) {\
        if(s->trans == NITRO_SOCKET_TCP)\
            s->stype.tcp.parent = s;\
        else\
            s->stype.inproc.parent = s;\
    }

#define SOCKET_PARENT(s) ((nitro_socket_t *)s->parent)
#define SOCKET_UNIVERSAL(s) (&(SOCKET_PARENT(s)->stype.univ))

typedef enum {
    NITRO_SOCKET_NO_TRANSPORT,
    NITRO_SOCKET_TCP,
    NITRO_SOCKET_INPROC,
} NITRO_SOCKET_TRANSPORT;

#define SOCKET_COMMON_FIELDS\
    /* Given Location */\
    char *given_location;\
    /* Socket Options */\
    nitro_sockopt_t *opt;\
    /* Incoming messages */\
    nitro_queue_t *q_recv;\
    /* Event fd (for integration with other event loops) */\
    int event_fd;\
    int write_pipe;\
    /* Parent socket */\
    void *parent;\
    /* Subscription trie */\
    nitro_prefix_trie_node *subs;\
    /* Stats lock (for 32 bit systems) */\
    pthread_mutex_t l_stats;\
    /* Local "want subscription" list */\
    nitro_key_t *sub_keys;\
     
typedef struct nitro_universal_socket_t {
    SOCKET_COMMON_FIELDS
} nitro_universal_socket_t;

typedef struct nitro_tcp_socket_t *nitro_tcp_socket_t_p;

typedef struct nitro_tcp_socket_t {
    SOCKET_COMMON_FIELDS

    /* Outgoing _general_ messages */
    nitro_queue_t *q_send;
    nitro_queue_t *q_empty;
    ev_timer close_timer;

    /* Pipes need to be locked during map
       lookup, mutation by libev thread, etc */
    pthread_mutex_t l_pipes;
    /* for reply-style session mapping
       UT Hash.  Pipes that have not yet registered are not in here */
    nitro_pipe_t *pipes_by_session;

    /* Circular List of all connected pipes (can use for round robining, or broadcast with pub)*/
    nitro_pipe_t *pipes;
    nitro_pipe_t *next_pipe;
    int num_pipes;

    uint64_t sub_keys_state;

    ev_io bound_io;
    int bound_fd;

    ev_io connect_io;
    int connect_fd;
    ev_timer connect_timer;
    ev_timer sub_send_timer;
    int outbound;

    struct sockaddr_in location;

    nitro_counted_buffer_t *sub_data;
    uint32_t sub_data_length;

    uint64_t stat_sent;
    uint64_t stat_recv;
    uint64_t stat_direct;
} nitro_tcp_socket_t;

typedef struct nitro_inproc_socket_t {
    SOCKET_COMMON_FIELDS

    /* hash table for bound sockets on the runtime */
    UT_hash_handle hh;

    pthread_rwlock_t link_lock;
    int num_links;
    struct nitro_inproc_socket_t *links;
    struct nitro_inproc_socket_t *current;
    nitro_counted_buffer_t *bind_counter;

    struct nitro_inproc_socket_t *prev;
    struct nitro_inproc_socket_t *next;

    int bound;
    int dead;

    uint64_t stat_recv;

    struct nitro_inproc_socket_t *registry;
    UT_hash_handle bound_hh;

} nitro_inproc_socket_t;

typedef struct nitro_socket_t {
    NITRO_SOCKET_TRANSPORT trans;

    union {
        nitro_universal_socket_t univ;
        nitro_tcp_socket_t tcp;
        nitro_inproc_socket_t inproc;
    } stype;

    struct nitro_socket_t *prev;
    struct nitro_socket_t *next;

} nitro_socket_t;

nitro_socket_t *nitro_socket_new();
void nitro_socket_destroy();
nitro_socket_t *nitro_socket_bind(char *location, nitro_sockopt_t *opt);
nitro_socket_t *nitro_socket_connect(char *location, nitro_sockopt_t *opt);
void nitro_socket_close(nitro_socket_t *s);
NITRO_SOCKET_TRANSPORT socket_parse_location(char *location, char **next);

#endif /* SOCKET_H */
