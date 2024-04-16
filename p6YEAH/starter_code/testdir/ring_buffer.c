#include <stdio.h>
#include <sched.h>
#include <stdatomic.h>
#include "ring_buffer.h"

// Atomic flags for spinlocks
atomic_flag put_lock = ATOMIC_FLAG_INIT;
atomic_flag get_lock = ATOMIC_FLAG_INIT;

int init_ring()
{
    return 0;
}

void ring_submit(struct ring *r, struct buffer_descriptor *bd)
{
    // Yeehaw! Time to wrangle that there spinlock before we go pokin' around in the ring buffer
    while (atomic_flag_test_and_set(&put_lock))
        sched_yield(); // yield the processor instead of spinning

    // Let's take a gander and see if the buffer's chock-full
    while (r->p_head >= r->c_tail + RING_SIZE)
    {
        sched_yield(); // yield the processor instead of spinning
    }

    // Rootin' tootin'! We got some space. Time to add our request to the buffer
    r->buffer[r->p_head % RING_SIZE] = *bd;

    // Now we gotta update the producer head, keep the line movin'
    ++r->p_head;
    ++r->p_tail;

    // All done! Time to unlock the spinlock and let the next cowboy have a turn
    atomic_flag_clear(&put_lock);
}

void ring_get(struct ring *r, struct buffer_descriptor *bd)
{
    // Now, we're gonna lock this here spinlock 'fore we go messin' with the ring buffer
    while (atomic_flag_test_and_set(&get_lock))
        sched_yield(); // yield the processor instead of spinning

    // Let's take a gander and see if the buffer's all empty-like
    while (r->c_head >= r->p_tail)
    {
        sched_yield(); // yield the processor instead of spinning
    }

    // Time to rustle up the request from the buffer
    *bd = r->buffer[r->c_head % RING_SIZE];

    // Now we gotta update the consumer head, keep the line movin'
    r->c_head++;
    r->c_tail++;

    // Done did our business with the ring buffer, best unlock the spinlock now
    atomic_flag_clear(&get_lock);
}