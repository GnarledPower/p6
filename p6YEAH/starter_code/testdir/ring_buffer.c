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
    r->buffer[r->p_head % RING_SIZE] = *bd;
    // Now we gotta update the producer head, keep the line movin'
    r->p_head++;
    r->p_tail++;

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

    *bd = r->buffer[r->c_head % RING_SIZE];

    r->c_head++;
    r->c_tail++;

    // Done did our business with the ring buffer, best unlock the spinlock now
    pthread_mutex_unlock(&get_lock);
}