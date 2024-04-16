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
#include "kv_store.h"

int num_threads;
int table_size;

hashtable *table;
char *shmem_area = NULL;
struct ring *ring;
int fd;
int ring_size;
char *mem;
void init(int size)
{
	printf("Initializing...\n");
	table_size = size;
	table = malloc(sizeof(hashtable));
	if (table == NULL)
	{
		printf("Error: Unable to allocate memory for table\n");
		exit(1);
	}

	table->size = table_size;
	printf("Table size: %d\n", table->size);

	table->heads = malloc(sizeof(node *) * table_size);
	if (table->heads == NULL)
	{
		printf("Error: Unable to allocate memory for table heads\n");
		free(table);
		return;
	}

	for (int i = 0; i < table_size; i++)
	{
		table->heads[i] = malloc(sizeof(node));
		if (table->heads[i] == NULL)
		{
			printf("Error: Unable to allocate memory for table head %d\n", i);
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

	printf("Table heads initialized\n");

	const char *filename = "shmem_file";
	int fd = open(filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (fd < 0)
	{
		perror("open\n");
		exit(1);
	}
	int shmemsize = sizeof(struct ring) + sizeof(struct buffer_descriptor) * num_threads;

	if (ftruncate(fd, shmemsize) == -1)
	{
		perror("ftruncate");
		close(fd);
		exit(1);
	}

	printf("Shared memory size: %d\n", shmemsize);
	void *mem = mmap(NULL, shmemsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (mem == MAP_FAILED)
	{
		perror("mmap\n");
		exit(1);
	}

	close(fd);

	ring = (struct ring *)mem;

	printf("Shared memory area initialized\n");

	if (init_ring() < 0)
	{
		printf("Error: Unable to initialize ring\n");
		exit(1);
	}

	printf("Initialization complete\n");
}

void put(key_type k, value_type v)
{
	int index = hash_function(k, table_size);

	pthread_mutex_lock(&table->heads[index]->lock);

	printf("putting: k: %d, v: %d\n", k, v);

	node *current = table->heads[index]->next;
	printf("current: %d\n", v);
	while (current != NULL)
	{
		if (current->key == k)
		{
			// Update the value of the existing node
			current->value = v;
			pthread_mutex_unlock(&table->heads[index]->lock);
			return;
		}
		current = current->next;
	}

	node *newNode = malloc(sizeof(node));
	newNode->key = k;
	newNode->value = v;
	newNode->next = table->heads[index]->next;
	table->heads[index]->next = newNode;

	pthread_mutex_unlock(&table->heads[index]->lock);
}

int get(key_type k)
{
	int index = hash_function(k, table_size);

	printf("getting k: %d\n", k);
	node *current = table->heads[index]->next;
	printf("current: %p\n", current);
	if (current == NULL)
	{
		return 0;
	}

	while (current != NULL)
	{
		if (current->key == k)
		{
			value_type value = current->value;
			printf("value: %d\n", value);
			return value;
		}
		current = current->next;
	}
	return 0;
}
void *thread_func(void *arg)
{
	struct buffer_descriptor bd;
	struct ring *r = (struct ring *)arg;

	while (1)
	{

		ring_get(r, &bd);

		// print contents of bd
		if (bd.req_type == PUT)
		{
			put(bd.k, bd.v);
		}
		else if (bd.req_type == GET)
		{
			int value = get(bd.k);
			//	printf("Got value: %d\n", value);
		}
		struct buffer_descriptor *result = (struct buffer_descriptor *)(arg + bd.res_off);
		memcpy(result, &bd, sizeof(struct buffer_descriptor *));
		result->ready = 1;
	}
}

int main(int argc, char *argv[])
{
	int o;

	while ((o = getopt(argc, argv, "n:s:")) != -1)
	{
		switch (o)
		{
		case 'n':
			num_threads = atoi(optarg);
			//	printf("Number of threads: %d\n", num_threads);
			break;
		case 's':
			table_size = atoi(optarg);
			//		printf("Table size: %d\n", table_size);
			break;
		default:
			//		printf("Usage: %s -n <num_threads> -s <table_size>\n", argv[0]);
			exit(1);
		}
	}
	init(table_size);

	// printf("r: %p\n", ring);
	pthread_t threads[num_threads];
	for (int i = 0; i < num_threads; i++)
	{
		//		printf("Creating thread %d\n", i);
		pthread_create(&threads[i], NULL, &thread_func, ring);
	}

	for (int i = 0; i < num_threads; i++)
	{
		//		printf("Joining thread %d\n", i);
		pthread_join(threads[i], NULL);
	}
	//	printf("All threads joined. Exiting...\n");
	return 0;
}