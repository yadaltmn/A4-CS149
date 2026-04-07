/*
 * countnames.c
 * Reads one file of names, writes PID.out / PID.err logs, and optionally
 * stores the computed name counts into a shared-memory region for shell.c.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_NAMES 1000
#define MAX_NAME_LENGTH 31
#define LINE_BUFFER_SIZE 256

typedef struct
{
    char name[MAX_NAME_LENGTH];
    int count;
} NameCountData;

static void redirect_output(void)
{
    char out_name[64];
    char err_name[64];

    snprintf(out_name, sizeof(out_name), "%d.out", getpid());
    snprintf(err_name, sizeof(err_name), "%d.err", getpid());

    close(STDOUT_FILENO);
    open(out_name, O_WRONLY | O_CREAT | O_TRUNC, 0777);

    close(STDERR_FILENO);
    open(err_name, O_WRONLY | O_CREAT | O_TRUNC, 0777);
}

static void initialize_region(NameCountData *region, int capacity)
{
    for (int i = 0; i < capacity; i++)
    {
        region[i].name[0] = '\0';
        region[i].count = 0;
    }
}

static void count_file_into_region(const char *filename, NameCountData *region, int capacity)
{
    FILE *fp = fopen(filename, "r");
    char buffer[LINE_BUFFER_SIZE];
    int length_count = 0;
    int line_num = 0;

    if (fp == NULL)
    {
        fprintf(stderr, "error: cannot open file %s\n", filename);
        return;
    }

    initialize_region(region, capacity);

    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        int found = 0;

        line_num++;
        buffer[strcspn(buffer, "\n")] = '\0';

        if (strlen(buffer) == 0)
        {
            fprintf(stderr, "Warning - file %s line %d is empty.\n", filename, line_num);
            continue;
        }

        for (int i = 0; i < length_count; i++)
        {
            if (strcmp(region[i].name, buffer) == 0)
            {
                region[i].count++;
                found = 1;
                break;
            }
        }

        if (!found && length_count < capacity - 1)
        {
            strncpy(region[length_count].name, buffer, MAX_NAME_LENGTH - 1);
            region[length_count].name[MAX_NAME_LENGTH - 1] = '\0';
            region[length_count].count = 1;
            length_count++;
            region[length_count].name[0] = '\0';
            region[length_count].count = 0;
        }
    }

    fclose(fp);
}

static void print_region(const NameCountData *region, int capacity)
{
    for (int i = 0; i < capacity && region[i].name[0] != '\0'; i++)
    {
        printf("%s: %d\n", region[i].name, region[i].count);
    }
}

static int write_results_to_shared_memory(const char *filename, const char *shm_path, long offset_bytes)
{
    int fd;
    struct stat sb;
    void *global_region;
    NameCountData *child_region;

    fd = open(shm_path, O_RDWR);
    if (fd == -1)
    {
        fprintf(stderr, "error: cannot open shared memory file %s\n", shm_path);
        return 1;
    }

    if (fstat(fd, &sb) == -1)
    {
        fprintf(stderr, "error: cannot stat shared memory file %s\n", shm_path);
        close(fd);
        return 1;
    }

    global_region = mmap(NULL, (size_t)sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (global_region == MAP_FAILED)
    {
        fprintf(stderr, "error: mmap failed for shared memory file %s\n", shm_path);
        close(fd);
        return 1;
    }

    child_region = (NameCountData *)((char *)global_region + offset_bytes);
    count_file_into_region(filename, child_region, MAX_NAMES + 1);
    print_region(child_region, MAX_NAMES + 1);

    munmap(global_region, (size_t)sb.st_size);
    close(fd);
    return 0;
}

int main(int argc, char *argv[])
{
    NameCountData local_region[MAX_NAMES + 1];

    redirect_output();

    if (argc == 2)
    {
        count_file_into_region(argv[1], local_region, MAX_NAMES + 1);
        print_region(local_region, MAX_NAMES + 1);
        return 0;
    }

    if (argc == 5 && strcmp(argv[2], "--shm") == 0)
    {
        char *endptr = NULL;
        long offset_bytes = strtol(argv[4], &endptr, 10);

        if (endptr == NULL || *endptr != '\0' || offset_bytes < 0)
        {
            fprintf(stderr, "error: invalid shared memory offset %s\n", argv[4]);
            return 1;
        }

        return write_results_to_shared_memory(argv[1], argv[3], offset_bytes);
    }

    return 0;
}
