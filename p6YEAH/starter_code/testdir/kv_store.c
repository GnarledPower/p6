#include <stdio.h>
#include <sys/mman.h>
#include <pthread.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <signal.h>
#include <string.h>

#include "common.h"
#include "ring_buffer.h"

int num_threads;
int table_size;

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

hashtable *table;
char *shmem_area = NULL;
struct ring *ring;
int fd;
int ring_size;
char *mem;

void init()
{
	table = malloc(sizeof(hashtable));
	if (table == NULL)
	{
		printf("Error: Unable to allocate memory for table\n");
		exit(1);
	}

	table->size = table_size;
	table->heads = malloc(sizeof(node *) * table_size);
	if (table->heads == NULL)
	{
		free(table);
		return;
	}

	for (int i = 0; i < table_size; i++)
	{
		table->heads[i] = malloc(sizeof(node));
		if (table->heads[i] == NULL)
		{
			for (int j = 0; j < i; j++)
			{
				free(table->heads[j]);
			}
			free(table->heads);
			free(table);
			return;
		}
		table->heads[i]->next = NULL;
		pthread_mutex_init(&table->heads[i]->lock, NULL);
	}

	const char *filename = "shmem_file";
	int fd = open(filename, O_RDONLY);
	if (fd < 0)
	{
		perror("open\n");
		exit(1);
	}
	struct stat sb;
	if (stat(filename, &sb) == -1)
	{
		perror("stat error\n");
		exit(1);
	}

	void *mem = mmap(NULL, sizeof(struct ring), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	shmem_area = mem;
	ring = (struct ring *)shmem_area;

	if (init_ring(ring) < 0)
	{
		exit(1);
	}
}

void put(key_type k, value_type v)
{
	node *newNode = malloc(sizeof(node));
	newNode->key = k;
	newNode->value = v;
	newNode->next = NULL;
	int index = hash_function(k, table_size);

	pthread_mutex_lock(&table->heads[index]->lock);

	node *next = table->heads[index];

	table->heads[index]->next = newNode;
	newNode->next = next;

	pthread_mutex_unlock(&table->heads[index]->lock);
}

int get(key_type k)
{
	int index = hash_function(k, table_size);

	pthread_mutex_lock(&table->heads[index]->lock);

	node *current = table->heads[index];

	while (current->next != NULL)
	{
		if (current->key == k)
		{
			pthread_mutex_unlock(&table->heads[index]->lock);
			return current->value;
		}
		current = current->next;
	}
	pthread_mutex_unlock(&table->heads[index]->lock);
	return 0;
}

void *thread_func()
{
	while (1)
	{
		struct buffer_descriptor *bd;
		ring_get(ring, bd);
		// print contents of bd
		if (bd->req_type == PUT)
		{
			put(bd->k, bd->v);
		}
		else if (bd->req_type == GET)
		{
			int value = get(bd->k);
		}
		struct buffer_descriptor *result = (struct buffer_descriptor *)(shmem_area + bd->res_off);
		memcpy(result, bd, sizeof(struct buffer_descriptor));
		result->ready = 1;
	}
}

int main(int argc, char *argv[])
{
	strtok(argv[1], " ");

	table_size = atoi(strtok(NULL, " "));

	strtok(argv[2], " ");

	num_threads = atoi(strtok(NULL, " "));
	init();

	pthread_t threads[num_threads];
	for (int i = 0; i < num_threads; i++)
	{
		pthread_create(&threads[i], NULL, &thread_func, NULL);
	}

	return 0;
}