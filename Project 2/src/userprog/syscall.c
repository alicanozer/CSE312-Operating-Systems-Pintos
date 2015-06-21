#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <user/syscall.h>
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

#define SYSTEM_CALL_MAX_ARG 4
#define VIRTUAL_ADRESSS_USER ((void *) 0x08048000)

struct lock file_system_lock;
static void syscall_handler (struct intr_frame *);
int process_add_file (struct file *f);
struct file* process_get_file (int fd);
int user_virtualaddress_to_kernel_ptr(const void *vaddr);
void fetch_argumans_from_frame (struct intr_frame *f, int *arg, int n);
void isvalid_frame_ptr (const void *vaddr);
void boundry_check_buffer (void* buffer, unsigned size);

void syscall_init (void){
  lock_init(&file_system_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler (struct intr_frame *f UNUSED){
  int arg[SYSTEM_CALL_MAX_ARG];
  isvalid_frame_ptr((const void*) f->esp);
  switch (* (int *) f->esp){

    case SYS_WRITE: fetch_argumans_from_frame(f, &arg[0], 3);
                    boundry_check_buffer((void *) arg[1], (unsigned) arg[2]);
                    arg[1] = user_virtualaddress_to_kernel_ptr((const void *) arg[1]);
                    f->eax = write(arg[0], (const void *) arg[1],
                    (unsigned) arg[2]);
                    break;

    case SYS_SEEK:  fetch_argumans_from_frame(f, &arg[0], 2);
                    seek(arg[0], (unsigned) arg[1]);
                    break;

    case SYS_CREATE:fetch_argumans_from_frame(f, &arg[0], 2);
                    arg[0] = user_virtualaddress_to_kernel_ptr((const void *) arg[0]);
                    f->eax = create((const char *)arg[0], (unsigned) arg[1]);
                    break;
      
    case SYS_REMOVE:fetch_argumans_from_frame(f, &arg[0], 1);
                    arg[0] = user_virtualaddress_to_kernel_ptr((const void *) arg[0]);
                    f->eax = remove((const char *) arg[0]);
                    break;

    case SYS_OPEN:  fetch_argumans_from_frame(f, &arg[0], 1);
                    arg[0] = user_virtualaddress_to_kernel_ptr((const void *) arg[0]);
                    f->eax = open((const char *) arg[0]);
                    break;
      
    case SYS_FILESIZE:  fetch_argumans_from_frame(f, &arg[0], 1);
                        f->eax = filesize(arg[0]);
                        break;
      
    case SYS_TELL:  fetch_argumans_from_frame(f, &arg[0], 1);
                    f->eax = tell(arg[0]);
                    break;
      
    case SYS_CLOSE: fetch_argumans_from_frame(f, &arg[0], 1);
                    close(arg[0]);
                    break;

    case SYS_HALT:  halt();
                    break;
    case SYS_EXIT:  fetch_argumans_from_frame(f, &arg[0], 1);
                    exit(arg[0]);
                    break;

    case SYS_EXEC:  fetch_argumans_from_frame(f, &arg[0], 1);
                    arg[0] = user_virtualaddress_to_kernel_ptr((const void *) arg[0]);
                    f->eax = exec((const char *) arg[0]);
                    break;
    case SYS_WAIT:  fetch_argumans_from_frame(f, &arg[0], 1);
                    f->eax = wait(arg[0]);
                    break;
      
    
      
    case SYS_READ:  fetch_argumans_from_frame(f, &arg[0], 3);
                    boundry_check_buffer((void *) arg[1], (unsigned) arg[2]);
                    arg[1] = user_virtualaddress_to_kernel_ptr((const void *) arg[1]);
                    f->eax = read(arg[0], (void *) arg[1], (unsigned) arg[2]);
                    break;
      
    
      
    }
}

int read (int fd, void *buffer, unsigned size){
  if (fd == STDIN_FILENO)
    {
      unsigned i;
      uint8_t* local_buffer = (uint8_t *) buffer;
      for (i = 0; i < size; i++)
        {
        local_buffer[i] = input_getc();
        }
      return size;
    }
  lock_acquire(&file_system_lock);
  struct file *f = process_get_file(fd);
  if (!f){
      lock_release(&file_system_lock);
      return ERROR;
    }
  int bytes = file_read(f, buffer, size);
  lock_release(&file_system_lock);
  return bytes;
}

int write (int fd, const void *buffer, unsigned size){
  if (fd == STDOUT_FILENO){
      putbuf(buffer, size);
      return size;
    }
  
  lock_acquire(&file_system_lock);

  struct file *f = process_get_file(fd);

   if (!f){
      lock_release(&file_system_lock);
      return ERROR;
    }

  int bytes = file_write(f, buffer, size); 

  lock_release(&file_system_lock);

  return bytes;
}

void fetch_argumans_from_frame (struct intr_frame *f, int *arg, int n){
  int i = 0;
  int *ptr;
  
  while(i < n){
   ptr = (int *) f->esp + i + 1;
    isvalid_frame_ptr((void *) ptr);
      arg[i] = *ptr;
      i++;
  }
  
}
struct process* add_process_current_thread (int pid){
  struct process* cp = malloc(sizeof(struct process));
  cp->pid = pid;
  cp->load = LOADED_DEFAULT;
  cp->wait = false;
  cp->exit = false;
  lock_init(&cp->lock_for_wait);
  list_push_back(&thread_current()->child_list,
&cp->elem);
  return cp;
}

struct process* child_process_check (int pid){
  struct thread *t = thread_current();
  struct list_elem *e;

  for (e = list_begin (&t->child_list); e != list_end (&t->child_list);e = list_next (e)){
          struct process *cp = list_entry (e, struct process, elem);
          if (pid == cp->pid)        
            return cp;

    }
  return NULL;
}

void close_file_by_fd (int fd){
}

void boundry_check_buffer (void* buffer, unsigned size){
  unsigned i;
  char* local_buffer = (char *) buffer;
  for (i = 0; i < size; i++){
      isvalid_frame_ptr((const void*) local_buffer);
      local_buffer++;
    }
}

int user_virtualaddress_to_kernel_ptr(const void *vaddr){
  isvalid_frame_ptr(vaddr);
  void *ptr = pagedir_get_page(thread_current()->pagedir, vaddr);
  if (!ptr)
      exit(ERROR);
  return (int) ptr;
}
void seek (int fd, unsigned position){
  lock_acquire(&file_system_lock);
  struct file *f = process_get_file(fd);
  if (!f){
      lock_release(&file_system_lock);
      return;
    }
  file_seek(f, position);
  lock_release(&file_system_lock);
}

unsigned tell (int fd){
  lock_acquire(&file_system_lock);
  struct file *f = process_get_file(fd);
  if (!f){
      lock_release(&file_system_lock);
      return ERROR;
    }
  off_t offset = file_tell(f);
  lock_release(&file_system_lock);
  return offset;
}

void close (int fd){
  lock_acquire(&file_system_lock);
  close_file_by_fd(fd);
  lock_release(&file_system_lock);
}

void exit (int status){
  struct thread *cur = thread_current();
  if (is_alive(cur->parent)){
      cur->cp->status = status;
    }
  printf ("%s: exit(%d)\n", cur->name, status);
  thread_exit();
}

 
pid_t exec (const char *cmd_line){
  pid_t pid = process_execute(cmd_line);
                                            
  struct process* cp = child_process_check(pid);
  ASSERT(cp);
  
  while (cp->load == LOADED_DEFAULT){
      barrier();
    }
  if (cp->load == LOADING_FAILED){
      return ERROR;
    }
  return pid;
}

int wait (pid_t pid){
  return process_wait(pid);
}

bool create (const char *file, unsigned initial_size){

    bool success = false;
    lock_acquire(&file_system_lock);
    success = filesys_create(file, initial_size);
    lock_release(&file_system_lock);
  return success;
}

bool remove (const char *file){
    bool success = false;

  lock_acquire(&file_system_lock);
    success = filesys_remove(file);
   lock_release(&file_system_lock);
  return success;
}

int open (const char *file){
  return 0;
}

int filesize (int fd){
  lock_acquire(&file_system_lock);
  struct file *f = process_get_file(fd);
  if (!f)
    {
      lock_release(&file_system_lock);
      return ERROR;
    }
  int size = file_length(f);
  lock_release(&file_system_lock);
  return size;
}


void isvalid_frame_ptr (const void *vaddr){
  if (!is_user_vaddr(vaddr) || vaddr < VIRTUAL_ADRESSS_USER){
      exit(ERROR);
    }
}

int process_add_file (struct file *f){
  struct file_struct_for_process *pf = malloc(sizeof(struct file_struct_for_process));
  pf->file = f;
  pf->fd = thread_current()->fd;
  thread_current()->fd++;
  list_push_back(&thread_current()->file_list, &pf->elem);
  return pf->fd;
}

struct file* process_get_file (int fd){
  struct thread *t = thread_current();
  struct list_elem *e;

  for (e = list_begin (&t->file_list); e != list_end (&t->file_list);e = list_next (e)){
          struct file_struct_for_process *process_file = list_entry (e, struct file_struct_for_process, elem);
          if (fd == process_file->fd){
                    return process_file->file;
            }
        }
  return NULL;
}
void remove_process_by (struct process *cp){
  list_remove(&cp->elem);
  free(cp);
}


void remove_child_processes_all (void){
  struct thread *t = thread_current();
  struct list_elem *next, *e = list_begin(&t->child_list);

  while (e != list_end (&t->child_list)){
      next = list_next(e);
      struct process *cp = list_entry (e, struct process,elem);
      list_remove(&cp->elem);
      free(cp);
      e = next;
    }
}
void halt (void){
  shutdown_power_off();
}
