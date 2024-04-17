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

void cleanup()
{
	for (int i = 0; i < table_size; i++)
	{
		node *current = table->heads[i]->next;
		while (current != NULL)
		{
			node *temp = current;
			current = current->next;
			free(temp);
		}
		free(table->heads[i]);
	}
	free(table->heads);
	free(table);
}

void init(int size)
{
	table_size = size;
	table = malloc(sizeof(hashtable));
	if (table == NULL)
	{
		printf("Error: Unable to allocate memory for table\n");
		exit(1);
	}

	table->size = table_size;

	table->heads = malloc(sizeof(head *) * table_size);
	if (table->heads == NULL)
	{
		free(table);
		return;
	}

	for (int i = 0; i < table_size; i++)
	{
		table->heads[i] = malloc(sizeof(head));
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

		pthread_spin_init(&table->heads[i]->lock, PTHREAD_PROCESS_PRIVATE);
	}

	const char *filename = "shmem_file";
	int fd = open(filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (fd < 0)
	{
		perror("open\n");
		cleanup();
		exit(1);
	}
	int shmemsize = sizeof(struct ring) + sizeof(struct buffer_descriptor) * num_threads;

	if (ftruncate(fd, shmemsize) == -1)
	{
		perror("ftruncate");
		close(fd);
		cleanup();
		exit(1);
	}

	void *mem = mmap(NULL, shmemsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (mem == MAP_FAILED)
	{
		perror("mmap\n");
		cleanup();
		exit(1);
	}

	close(fd);

	ring = (struct ring *)mem;

	if (init_ring() < 0)
	{
		cleanup();
		exit(1);
	}
}

void put(key_type k, value_type v)
{
	int index = hash_function(k, table_size);

	pthread_spin_lock(&table->heads[index]->lock);

	node *current = table->heads[index]->next;
	while (current != NULL)
	{
		if (current->key == k)
		{
			// Update the value of the existing node
			current->value = v;
			pthread_spin_unlock(&table->heads[index]->lock);
			return;
		}
		current = current->next;
	}

	node *newNode = malloc(sizeof(node));
	newNode->key = k;
	newNode->value = v;
	newNode->next = table->heads[index]->next;
	table->heads[index]->next = newNode;

	pthread_spin_unlock(&table->heads[index]->lock);
}

int get(key_type k)
{
	int index = hash_function(k, table_size);

	node *current = table->heads[index]->next;

	while (current != NULL)
	{
		if (current->key == k)
		{
			value_type value = current->value;
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
		int value;
		if (bd.req_type == PUT)
		{
			put(bd.k, bd.v);
		}
		else if (bd.req_type == GET)
		{
			value = get(bd.k);
			//	printf("Got value: %d\n", value);
		}
		struct buffer_descriptor *result = (struct buffer_descriptor *)(arg + bd.res_off);
		memcpy(result, &bd, sizeof(struct buffer_descriptor *));
		result->v = value;
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

	pthread_t threads[num_threads];
	for (int i = 0; i < num_threads; i++)
	{
		pthread_create(&threads[i], NULL, &thread_func, ring);
	}

	for (int i = 0; i < num_threads; i++)
	{
		pthread_join(threads[i], NULL);
	}
	cleanup();
	return 0;
}