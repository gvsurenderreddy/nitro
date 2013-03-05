#ifndef QUEUE_H
#define QUEUE_H

#include "common.h"
#include "frame.h"

#define INITIAL_QUEUE_SZ 1024

typedef enum {
    NITRO_QUEUE_STATE_EMPTY,
    NITRO_QUEUE_STATE_CONTENTS,
    NITRO_QUEUE_STATE_FULL
} NITRO_QUEUE_STATE;

typedef void (*nitro_queue_state_changed)(NITRO_QUEUE_STATE st, void *baton);

/* Queue of Frames */
typedef struct nitro_queue_t {
    nitro_frame_t **q;
    nitro_frame_t **head;
    nitro_frame_t **tail;
    nitro_frame_t **end;
    int size;
    int count;
    int capacity;
    pthread_mutex_t lock;
    pthread_cond_t trigger;

    nitro_queue_state_changed state_callback;
    void *baton;

} nitro_queue_t;

nitro_queue_t *nitro_queue_new(int capacity,
    nitro_queue_state_changed queue_cb, void *baton);

nitro_frame_t *nitro_queue_pull(nitro_queue_t *q);
void nitro_queue_push(nitro_queue_t *q, nitro_frame_t *f);
int nitro_queue_socket_write(nitro_queue_t *q, int fd);
void nitro_queue_destroy(nitro_queue_t *q);
inline int nitro_queue_count(nitro_queue_t *q);
typedef nitro_frame_t *(*nitro_queue_frame_generator)(void *baton);
void nitro_queue_move(nitro_queue_t *src, nitro_queue_t *dst, int max);

void nitro_queue_consume(nitro_queue_t *q, 
    nitro_queue_frame_generator gen,
    void *baton);
#endif /* QUEUE_H */