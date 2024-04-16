#include <stdio.h>
#include "kv_store.h"
#include "common.h"
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    char *filename = argv[1];

    init(10000);

    // read from file line by line
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        printf("Error: Unable to open file\n");
        return 1;
    }

    char *line = NULL;
    size_t size = 0;
    while (getline(&line, &size, file) != -1)
    {
        char *command = strtok(line, " ");
        char *key = strtok(NULL, " ");
        if (strcmp(command, "put") == 0)
        {
            char *value = strtok(NULL, " ");
            put(atoi(key), atoi(value));
            printf("PUT %d %d\n", atoi(key), atoi(value));
        }
        else if (strcmp(command, "get") == 0)
        {
            get(atoi(key));
            printf("GET %d\n", atoi(key));
        }
        else
        {
            printf("Error: Invalid command\n");
        }
    }

    free(line);
    fclose(file);

    return 0;
}