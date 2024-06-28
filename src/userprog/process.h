#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void init_process_control_block(struct thread *t);
struct file* get_file_from_fd(int fd);
struct list_elem* get_file_elem_from_fd(int fd);
void close_files(void);

/* Process Control Block Struct */
struct fds
  {
    int fd;
    char *file_name;
    struct file *fp;
    struct list_elem elem;
  };

struct pcb
	{
		struct thread *parent_thread;
		struct thread *process_thread;
		struct list_elem elem;
		struct file *file; //needed?
    struct list fd_table;
    int highest_fd;
	};
	

#endif /* userprog/process.h */
