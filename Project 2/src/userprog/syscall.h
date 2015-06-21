#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h"

#define CLOSE_ALL -1
#define ERROR -1
#define LOADED_DEFAULT 0
#define SUCCESSFULLY_LOAD 1
#define LOADING_FAILED 2

struct process {
    struct lock lock_for_wait;
    struct list_elem elem;
    uint32_t pid;
    uint32_t load;
    bool wait;
    bool exit;
    uint8_t status;

};

struct file_struct_for_process {
    int fd;
    struct file *file; 
    struct list_elem elem;
};

struct process* add_process_current_thread (int pid);
struct process* child_process_check (int pid);
void syscall_init (void);
void remove_process_by (struct process *cp);
void remove_child_processes_all (void);
void close_file_by_fd (int fd);

#endif /* userprog/syscall.h */