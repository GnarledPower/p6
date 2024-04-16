#include <stdio.h>
#include <pthread.h>
#include "ring_buffer.h"
#include <sched.h>

// Descriptions of all methods in ring_buffer.h :D

// 素晴らしいコーディング！！
// For fun, I asked copilot to rewrite all my comments like how a cowboy would talk

// Mutex for thread safety
pthread_mutex_t get_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t put_mutex = PTHREAD_MUTEX_INITIALIZER;

int init_ring()
{

    return 0;
}

void ring_submit(struct ring *r, struct buffer_descriptor *bd)
{
    // Yeehaw! Time to wrangle that there mutex before we go pokin' around in the ring buffer
    pthread_mutex_lock(&put_mutex);

    // Let's take a gander and see if the buffer's chock-full
    while (r->p_head >= r->c_tail + RING_SIZE)
    {
    }

    // Rootin' tootin'! We got some space. Time to add our request to the buffer
    r->buffer[r->p_head % RING_SIZE] = *bd;

    // Now we gotta update the producer head, keep the line movin'
    ++r->p_head;
    ++r->p_tail;

    // All done! Time to unlock the mutex and let the next cowboy have a turn
    pthread_mutex_unlock(&put_mutex);
}

void ring_get(struct ring *r, struct buffer_descriptor *bd)
{
    // Now, we're gonna lock this here mutex 'fore we go messin' with the ring buffer
    pthread_mutex_lock(&get_mutex);
    //   printf("Lock acquired\n");

    // Let's take a gander and see if the buffer's all empty-like
    while (r->c_head >= r->p_tail)
    {
    }

    //   printf("c_head: %d, c_tail: %d\n", r->c_head, r->c_tail);
    // Time to rustle up the request from the buffer
    *bd = r->buffer[r->c_head % RING_SIZE];
    //   printf("bd: %p, k: %d, v: %d\n", bd, bd->k, bd->v);

    // Now we gotta update the consumer head, keep the line movin'

    r->c_head++;
    r->c_tail++;

    // Done did our business with the ring buffer, best unlock the mutex now
    pthread_mutex_unlock(&get_mutex);
}
