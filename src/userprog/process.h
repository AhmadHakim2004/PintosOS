#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "filesys/file.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);
void init_process_control_block (struct thread *t);
struct file* get_file_from_fd (int fd);
struct list_elem* get_file_elem_from_fd (int fd);
void close_files (void);
bool install_page (void *upage, void *kpage, bool writable);

/* File Information for an entry in the Process Control Block FD Table */
struct fds
  {
    int fd;                           /* File descriptor */
    char *file_name;                  /* File name associated with fd */
    struct file *fp;                  /* Pointer to the file */
    struct list_elem elem;            /* List element */ 
  };

/* Process Control Block Struct */
struct pcb
	{
		struct thread *parent_thread;   /* Thread that spawns the process thread */
		struct thread *process_thread;  /* Process thread */
		struct list_elem elem;          /* List element */
		struct file *file;              /* Process executable file */
    struct list fd_table;           /* Process's list of fds */ 
    int highest_fd;                 /* Process's highest fd */
	};
	

#endif /* userprog/process.h */
