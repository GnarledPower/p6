#include <stdio.h>
#include <pthread.h>
#include "ring_buffer.h"

// Descriptions of all methods in ring_buffer.h :D

// 素晴らしいコーディング！！
// For fun, I asked copilot to rewrite all my comments like how a cowboy would talk

// Mutex for thread safety
pthread_mutex_t ring_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t full = PTHREAD_COND_INITIALIZER;

int init_ring(struct ring *r)
{

    return 0;
}

void ring_submit(struct ring *r, struct buffer_descriptor *bd)
{
    // Yeehaw! Time to wrangle that there mutex before we go pokin' around in the ring buffer
    pthread_mutex_lock(&ring_mutex);

    // Let's take a gander and see if the buffer's chock-full
    if (r->p_head >= r->p_tail + RING_SIZE)
        pthread_cond_wait(&full, &ring_mutex);

    // Rootin' tootin'! We got some space. Time to add our request to the buffer
    r->buffer[r->p_head] = *bd;

    // Now we gotta update the producer head, keep the line movin'
    r->p_head = (r->p_head + 1) % RING_SIZE;

    // All done! Time to unlock the mutex and let the next cowboy have a turn
    pthread_cond_broadcast(&empty);
}

void ring_get(struct ring *r, struct buffer_descriptor *bd)
{
    // Now, we're gonna lock this here mutex 'fore we go messin' with the ring buffer
    pthread_mutex_lock(&ring_mutex);

    // Let's take a gander and see if the buffer's all empty-like
    if (r->c_head >= r->c_tail)
        pthread_cond_wait(&empty, &ring_mutex);

    // Time to rustle up the request from the buffer
    *bd = r->buffer[r->c_head];

    // Now we gotta update the consumer head, keep the line movin'
    r->c_head = (r->c_head + 1) % RING_SIZE;

    // Done did our business with the ring buffer, best unlock the mutex now
    pthread_cond_broadcast(&full);
}
