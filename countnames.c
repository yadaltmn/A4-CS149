// filepath: /countnames/src/countnames.c

/*
 * countnames.c
 * Reads names from a file and prints how many times each name appears.
 * For Assignment 4, children can also write their results into an mmap()
 * shared-memory region assigned by shell.c.
 * GitHub : https://github.com/yadaltmn/A4-CS149
 * Author :
 * Jesse Mendoza
 * Jada-Lien Nguyen
 * Date modified : 04/07/2026
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define MAX_NAMES 1000
#define MAX_NAME_LENGTH 31
#define LINE_BUFFER_SIZE 256

typedef struct
{
    char name[MAX_NAME_LENGTH];
    int count;
} NameCountData;

// Redirect stdout/stderr so each child leaves behind its own .out and .err logs
static void redirect(void)
{
    char out[64];
    char errName[64];

    snprintf(out, sizeof(out), "%d.out", getpid());
    snprintf(errName, sizeof(errName), "%d.err", getpid());

    close(STDOUT_FILENO);
    open(out, O_WRONLY | O_CREAT | O_TRUNC, 0777);

    close(STDERR_FILENO);
    open(errName, O_WRONLY | O_CREAT | O_TRUNC, 0777);
}

static void initialize_region(NameCountData *region, int capacity)
{
    // Make all entries empty
    for (int i = 0; i < capacity; i++)
    {
        region[i].name[0] = '\0';
        region[i].count = 0;
    }
}

static int count_file_into_region(const char *filename, NameCountData *region, int capacity)
{
    // Count the names in this file.
    FILE *fp = fopen(filename, "r");
    char buffer[LINE_BUFFER_SIZE];
    int lengthCount = 0;
    int lineNum = 0;

    if (fp == NULL)
    {
        fprintf(stderr, "error: cannot open file %s\n", filename);
        return 1;
    }

    initialize_region(region, capacity);

    while (fgets(buffer, LINE_BUFFER_SIZE, fp) != NULL)
    {
        int found = 0;

        lineNum++;
        buffer[strcspn(buffer, "\n")] = '\0';

        if (strlen(buffer) == 0)
        {
            fprintf(stderr, "Warning - file %s line %d is empty.\n", filename, lineNum);
            continue;
        }

        for (int i = 0; i < lengthCount; i++)
        {
            if (strcmp(region[i].name, buffer) == 0)
            {
                region[i].count++;
                found = 1;
                break;
            }
        }

        if (!found && lengthCount < capacity - 1)
        {
            strncpy(region[lengthCount].name, buffer, MAX_NAME_LENGTH - 1);
            region[lengthCount].name[MAX_NAME_LENGTH - 1] = '\0';
            region[lengthCount].count = 1;
            lengthCount++;
            region[lengthCount].name[0] = '\0';
            region[lengthCount].count = 0;
        }
    }

    fclose(fp);
    return 0;
}

// Prints the name counts in this region until we hit an empty entry or the capacity limit.
static void print_region(const NameCountData *region, int capacity)
{
    for (int i = 0; i < capacity && region[i].name[0] != '\0'; i++)
    {
        printf("%s: %d\n", region[i].name, region[i].count);
    }
}

// Returns 0 on success, 1 on failure
static int write_results_to_shared_memory(const char *filename, const char *shmPath, long offsetBytes)
{
    // Open the shared memory and go to this child's part.
    int fd;
    struct stat sb;
    void *globalRegion;
    NameCountData *childRegion;
    int status;

    // Open the shared memory file
    fd = open(shmPath, O_RDWR);
    if (fd == -1)
    {
        fprintf(stderr, "error: cannot open shared memory file %s\n", shmPath);
        return 1;
    }
    
    // Get the size of the shared memory file
    if (fstat(fd, &sb) == -1)
    {
        fprintf(stderr, "error: cannot stat shared memory file %s\n", shmPath);
        close(fd);
        return 1;
    }

    // Map the shared memory file into this process's address space
    globalRegion = mmap(NULL, (size_t)sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (globalRegion == MAP_FAILED)
    {
        fprintf(stderr, "error: mmap failed for shared memory file %s\n", shmPath);
        close(fd);
        return 1;
    }

    childRegion = (NameCountData *)((char *)globalRegion + offsetBytes);
    status = count_file_into_region(filename, childRegion, MAX_NAMES + 1);
    if (status == 0)
    {
        print_region(childRegion, MAX_NAMES + 1);
    }

    munmap(globalRegion, (size_t)sb.st_size); // Unmap the shared memory
    close(fd);
    return status;
}

int main(int argc, char *argv[])
{
    NameCountData localRegion[MAX_NAMES + 1];

    redirect();

    if (argc == 2)
    {
        int status = count_file_into_region(argv[1], localRegion, MAX_NAMES + 1);

        if (status == 0)
        {
            print_region(localRegion, MAX_NAMES + 1);
        }

        return status;
    }

    if (argc == 5 && strcmp(argv[2], "--shm") == 0)
    {
        // Use shared memory
        char *endptr = NULL;
        long offsetBytes = strtol(argv[4], &endptr, 10);

        if (endptr == NULL || *endptr != '\0' || offsetBytes < 0)
        {
            fprintf(stderr, "error: invalid shared memory offset %s\n", argv[4]);
            return 1;
        }

        return write_results_to_shared_memory(argv[1], argv[3], offsetBytes);
    }

    return 0;
}
