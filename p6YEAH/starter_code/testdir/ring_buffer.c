#include <stdio.h>
#include <sched.h>
#include <stdatomic.h>
#include <pthread.h>
#include <string.h>
#include "ring_buffer.h"

// Atomic flags for spinlocks
pthread_mutex_t put_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t get_lock = PTHREAD_MUTEX_INITIALIZER;

int init_ring()
{
    return 0;
}

void ring_submit(struct ring *r, struct buffer_descriptor *bd)
{
    // Yeehaw! Time to wrangle that there spinlock before we go pokin' around in the ring buffer
    pthread_mutex_lock(&put_lock);

    // Let's take a gander and see if the buffer's chock-full
    while (atomic_load(&r->p_head) >= atomic_load(&r->c_tail) + RING_SIZE)
    {
        sched_yield(); // yield the processor instead of spinning
    }

    // Rootin' tootin'! We got some space. Time to add our request to the buffer
    memcpy(&r->buffer[atomic_load(&r->p_head) % RING_SIZE], bd, sizeof(struct buffer_descriptor));
    // Now we gotta update the producer head, keep the line movin'
    atomic_fetch_add(&r->p_head, 1);
    atomic_fetch_add(&r->p_tail, 1);

    // All done! Time to unlock the spinlock and let the next cowboy have a turn
    pthread_mutex_unlock(&put_lock);
}

void ring_get(struct ring *r, struct buffer_descriptor *bd)
{
    // Now, we're gonna lock this here spinlock 'fore we go messin' with the ring buffer
    pthread_mutex_lock(&get_lock);

    // Let's take a gander and see if the buffer's all empty-like
    while (atomic_load(&r->c_head) >= atomic_load(&r->p_tail))
    {
        sched_yield(); // yield the processor instead of spinning
    }

    // Time to rustle up the request from the buffer

    memcpy(bd, &r->buffer[atomic_load(&r->c_head) % RING_SIZE], sizeof(struct buffer_descriptor));
    // Now we gotta update the consumer head, keep the line movin'
    atomic_fetch_add(&r->c_head, 1);
    atomic_fetch_add(&r->c_tail, 1);

    // Done did our business with the ring buffer, best unlock the spinlock now
    pthread_mutex_unlock(&get_lock);
}