/*
 * shell.c
 * A simple shell that supports running countnames in parallel using
 * mmap()-backed shared memory regions.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAXLINE 1024
#define MAX_ARGS 128
#define MAX_NAMES 1000
#define MAX_NAME_LENGTH 31

typedef struct
{
    char name[MAX_NAME_LENGTH];
    int count;
} NameCountData;

// Make all entries empty
static void initialize_region(NameCountData *region, int capacity)
{
    for (int i = 0; i < capacity; i++)
    {
        region[i].name[0] = '\0';
        region[i].count = 0;
    }
}

// Merge this entry into the summary region
static void merge_name_count(NameCountData *summary_region, int capacity, const NameCountData *entry)
{
    for (int i = 0; i < capacity - 1; i++)
    {
        if (summary_region[i].name[0] == '\0')
        {
            strncpy(summary_region[i].name, entry->name, MAX_NAME_LENGTH - 1);
            summary_region[i].name[MAX_NAME_LENGTH - 1] = '\0';
            summary_region[i].count = entry->count;
            summary_region[i + 1].name[0] = '\0';
            summary_region[i + 1].count = 0;
            return;
        }

        if (strcmp(summary_region[i].name, entry->name) == 0)
        {
            summary_region[i].count += entry->count;
            return;
        }
    }
}

// Add this child's counts to the summary region
static void aggregate_child_region(NameCountData *summary_region, int capacity, const NameCountData *child_region)
{
    // Empty name means stop here.
    for (int i = 0; i < capacity && child_region[i].name[0] != '\0'; i++)
    {
        merge_name_count(summary_region, capacity, &child_region[i]);
    }
}

// Print the name counts in this region
static void print_region(const NameCountData *region, int capacity)
{
    for (int i = 0; i < capacity && region[i].name[0] != '\0'; i++)
    {
        printf("%s: %d\n", region[i].name, region[i].count);
    }
}

// Run countnames in parallel using shared memory
static int run_countnames_parallel(char *args[], int argc)
{
    const int region_capacity = MAX_NAMES + 1;
    const size_t region_bytes = (size_t)region_capacity * sizeof(NameCountData);
    const int child_count = argc - 1;
    const size_t total_bytes = (size_t)(child_count + 1) * region_bytes;
    char shm_template[] = "/tmp/countnames-global-XXXXXX";
    int shm_fd;
    void *global;
    NameCountData *summary_region;
    int status = 0;

    shm_fd = mkstemp(shm_template);
    if (shm_fd == -1)
    {
        perror("mkstemp");
        return 1;
    }

    if (ftruncate(shm_fd, (off_t)total_bytes) == -1)
    {
        perror("ftruncate");
        close(shm_fd);
        unlink(shm_template);
        return 1;
    }

    global = mmap(NULL, total_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (global == MAP_FAILED)
    {
        perror("mmap");
        close(shm_fd);
        unlink(shm_template);
        return 1;
    }

    // Make one part for each child and one part for the final result
    memset(global, 0, total_bytes);
    summary_region = (NameCountData *)((char *)global + ((size_t)child_count * region_bytes));
    initialize_region(summary_region, region_capacity);

    // Start all children so they run at the same time.
    for (int i = 0; i < child_count; i++)
    {
        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork");
            status = 1;
            continue;
        }

        if (pid == 0)
        {
            char offset_buffer[32];
            size_t offset = (size_t)i * region_bytes;

            // Tell each child where its part starts
            snprintf(offset_buffer, sizeof(offset_buffer), "%zu", offset);
            execl("./countnames", "./countnames", args[i + 1], "--shm", shm_template, offset_buffer, (char *)NULL);
            perror("exec");
            _exit(127);
        }
    }

    for (int i = 0; i < child_count; i++)
    {
        int child_status = 0;

        if (wait(&child_status) == -1)
        {
            perror("wait");
            status = 1;
        }
        else if (!WIFEXITED(child_status) || WEXITSTATUS(child_status) != 0) // if the child failed, we should fail too.
        {
            status = 1;
        }
    }

    for (int i = 0; i < child_count; i++)
    {
        NameCountData *child_region = (NameCountData *)((char *)global + ((size_t)i * region_bytes));
        // Add each child's results to the final result
        aggregate_child_region(summary_region, region_capacity, child_region);
    }

    print_region(summary_region, region_capacity);

    munmap(global, total_bytes);
    close(shm_fd);
    unlink(shm_template);
    return status;
}

int main(void)
{
    char buf[MAXLINE];

    printf("%% ");
    while (fgets(buf, sizeof(buf), stdin) != NULL) // Read a line of input and parse it into arguments
    {
        char *args[MAX_ARGS];
        int argc = 0;
        char *token;

        // Remove the trailing newline and split on whitespace
        buf[strcspn(buf, "\n")] = '\0';
        token = strtok(buf, " \t");

        while (token != NULL && argc < MAX_ARGS - 1)
        {
            args[argc++] = token;
            token = strtok(NULL, " \t");
        }
        args[argc] = NULL;

        if (argc == 0)
        {
            printf("%% ");
            continue;
        }

        // Handle built-in commands here.
        if (strcmp(args[0], "quit") == 0 || strcmp(args[0], "exit") == 0)
        {
            break;
        }

        // If the command is countnames, we want to run our parallel version that uses shared memory. Otherwise, just run it normally
        if (strcmp(args[0], "./countnames") == 0)
        {
            if (argc == 1)
            {
                pid_t pid = fork();
                int status;

                if (pid < 0)
                {
                    perror("fork");
                }
                else if (pid == 0)
                {
                    execl("./countnames", "./countnames", (char *)NULL);
                    perror("exec");
                    _exit(127);
                }
                else
                {
                    waitpid(pid, &status, 0);
                }
            }
            else
            {
                run_countnames_parallel(args, argc);
            }

            printf("%% ");
            continue;
        }

        {
            pid_t pid = fork();
            int status;

            if (pid < 0)
            {
                perror("fork");
            }
            else if (pid == 0)
            {
                execvp(args[0], args);
                perror("exec");
                _exit(127);
            }
            else
            {
                waitpid(pid, &status, 0);
            }
        }

        printf("%% ");
    }

    return 0;
}
