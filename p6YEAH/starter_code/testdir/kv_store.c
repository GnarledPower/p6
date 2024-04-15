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
	printf("Initializing...\n");

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
	struct stat sb;
	if (stat(filename, &sb) == -1)
	{
		perror("stat error\n");
		exit(1);
	}
	int shmemsize = sb.st_size;
	printf("Shared memory size: %d\n", shmemsize);
	void *mem = mmap(NULL, shmemsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (mem == MAP_FAILED)
	{
		perror("mmap\n");
		exit(1);
	}
	memset(mem, 0, shmemsize);
	close(fd);
	shmem_area = mem;
	ring = (struct ring *)shmem_area;

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
	node *newNode = malloc(sizeof(node));
	newNode->key = k;
	newNode->value = v;
	newNode->next = NULL;
	int index = hash_function(k, table_size);

	pthread_mutex_lock(&table->heads[index]->lock);
	printf("putting\n");
	node *next = table->heads[index];

	table->heads[index]->next = newNode;
	newNode->next = next;

	pthread_mutex_unlock(&table->heads[index]->lock);
}

int get(key_type k)
{
	int index = hash_function(k, table_size);

	pthread_mutex_lock(&table->heads[index]->lock);
	printf("getting\n");
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
void *thread_func(void *arg)
{
	struct buffer_descriptor bd;
	struct ring *r = (struct ring *)arg;

	while (1)
	{

		printf("Thread function running...\n");
		ring_get(r, &bd);
		printf("Received buffer descriptor from ring\n");
		// print contents of bd
		if (bd.req_type == PUT)
		{
			printf("Request type: PUT\n");
			put(bd.k, bd.v);
		}
		else if (bd.req_type == GET)
		{
			printf("Request type: GET\n");
			int value = get(bd.k);
			printf("Got value: %d\n", value);
		}
		struct buffer_descriptor *result = (struct buffer_descriptor *)(shmem_area + bd.res_off);
		memcpy(result, &bd, sizeof(struct buffer_descriptor));
		result->ready = 1;
		printf("Buffer descriptor copied to result\n");
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
			printf("Number of threads: %d\n", num_threads);
			break;
		case 's':
			table_size = atoi(optarg);
			printf("Table size: %d\n", table_size);
			break;
		default:
			printf("Usage: %s -n <num_threads> -s <table_size>\n", argv[0]);
			exit(1);
		}
	}
	init();

	printf("r: %p\n", ring);
	pthread_t threads[num_threads];
	for (int i = 0; i < num_threads; i++)
	{
		printf("Creating thread %d\n", i);
		pthread_create(&threads[i], NULL, &thread_func, ring);
	}

	for (int i = 0; i < num_threads; i++)
	{
		printf("Joining thread %d\n", i);
		pthread_join(threads[i], NULL);
	}
	printf("All threads joined. Exiting...\n");
	return 0;
}