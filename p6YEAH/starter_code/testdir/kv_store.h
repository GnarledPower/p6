#include <pthread.h>
#include "common.h"

typedef struct node
{
	key_type key;
	value_type value;
	pthread_mutex_t lock;
	struct node *next;
} node;

typedef struct hashtable
{
	node **heads;
	int size;
} hashtable;

void put(key_type k, value_type v);
int get(key_type k);
void init(int size);