#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include "ring_buffer.h"

// Descriptions of all methods in ring_buffer.h :D

// 素晴らしいコーディング！！
// For fun, I asked copilot to rewrite all my comments like how a cowboy would talk

// Mutex for thread safety
pthread_mutex_t submit_lock;
pthread_mutex_t get_lock;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t full = PTHREAD_COND_INITIALIZER;

int init_ring()
{
    if (pthread_mutex_init(&get_lock, NULL) != 0)
    {
        return -1;
    }
    if (pthread_mutex_init(&submit_lock, NULL) != 0)
    {
        return -1;
    }
    return 0;
}

void ring_submit(struct ring *r, struct buffer_descriptor *bd)
{
    pthread_mutex_lock(&submit_lock);
    printf("ring_submit: obtained lock\n");
    // Wait until there is space in the ring buffer
    while ((r->p_head - r->c_tail) >= RING_SIZE)
    {
        printf("ring_submit: waiting for space in the ring buffer\n");
    }
    printf("ring_submit: copying data to the ring buffer\n");
    memcpy(&r->buffer[r->p_head % RING_SIZE], bd, sizeof(struct buffer_descriptor));
    r->p_head++;
    r->p_tail++;
    printf("ring_submit: data copied, releasing lock\n");
    pthread_mutex_unlock(&submit_lock);
}

void ring_get(struct ring *r, struct buffer_descriptor *bd)
{
    pthread_mutex_lock(&get_lock);
    printf("r: %p\n", r);
    while (r->c_head >= r->p_tail)
    {
    }
    printf("ring_get: obtained lock\n");
    printf("ring_get: copying data from the ring buffer\n");
    printf("r->c_head: %d\n", r->c_head);
    printf("r->p_tail: %d\n", r->p_tail);
    memcpy(bd, &r->buffer[r->c_head % RING_SIZE], sizeof(struct buffer_descriptor));

    r->c_head++;
    r->c_tail++;
    printf("ring_get: data copied, releasing lock\n");
    pthread_mutex_unlock(&get_lock);
}
