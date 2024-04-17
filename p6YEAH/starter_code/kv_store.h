#include <pthread.h>
#include <stdatomic.h>
#include "common.h"

typedef struct node
{
	key_type key;
	value_type value;
	struct node *next;
} node;
typedef struct head
{
	pthread_spinlock_t lock;
	node *next;
} head;

typedef struct hashtable
{
	head **heads;
	int size;
} hashtable;

void put(key_type k, value_type v);
int get(key_type k);
void init(int size);