#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "pagedir.h"
#include "process.h"
#include "vm/mapped_file.h"
#include "vm/page.h"

#define MAX_FILE_NAME_LENGTH 14       /* Maximum allowed file name length. */

static void syscall_handler (struct intr_frame *);
static void exit_handler (int);
static void halt_handler (void);
static int exec_handler (char *);
static int wait_handler (int);
static bool create_handler (char *, unsigned);
static bool remove_handler (char *);
static int open_handler (char *);
static int filesize_handler (int);
static int read_handler (int, char *, unsigned);
static int write_handler (int, char *, unsigned);
static void seek_handler (int, unsigned);
static unsigned tell_handler (int);
static void close_handler (int);
static mapid_t mmaphandler (int, void *);
static void munmap_handler (mapid_t);
static bool is_valid_pointer_with_size (void *, int, bool);
static bool is_valid_pointer (void *);
static bool is_valid_char_pointer (char *);

static uint32_t *esp;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&file_lock);
}

/* Gets the interrupt code and arguements from the interrupt frame and calls 
the appropriate handler based on the interrupt code. */
static void
syscall_handler (struct intr_frame *f) 
{
  esp = (uint32_t *)f->esp;
  int *intr_code = (int *)f->esp;
  void *arg1 = f->esp+4;
  void *arg2 = f->esp+8;
  void *arg3 = f->esp+12;

  thread_current()->saved_esp = f->esp;

  /* Validates the interrupt code pointer. */
  if (!is_valid_pointer (intr_code))
    thread_exit ();

  /* Validates first arguement if its an int pointer. */
  if ((*intr_code == SYS_WAIT || *intr_code == SYS_EXIT 
      || *intr_code >= SYS_FILESIZE) && !is_valid_pointer (arg1))
      thread_exit ();

  /* Validates first arguement if its a pointer to a char pointer. */
  if ((*intr_code == SYS_EXEC || *intr_code == SYS_CREATE 
      || *intr_code == SYS_REMOVE || *intr_code == SYS_OPEN) 
      && !is_valid_pointer (arg1))
      thread_exit ();

  switch (*intr_code)
    {
      case SYS_HALT:
        halt_handler ();
        break;
      case SYS_EXIT:
        exit_handler (*(int *)arg1);
        break;
      case SYS_EXEC:
        f->eax = exec_handler (*(char **)arg1);
        break;
      case SYS_WAIT:
        f->eax = wait_handler (*(int *)arg1);
        break;
      case SYS_CREATE:
        if (!is_valid_pointer (arg2))
          thread_exit ();
        f->eax = create_handler (*(char **)arg1, *(unsigned *)arg2);
        break;
      case SYS_REMOVE:
        f->eax = remove_handler (*(char **)arg1);
        break;
      case SYS_OPEN:
        f->eax = open_handler (*(char **)arg1);
        break;
      case SYS_FILESIZE:
        f->eax = filesize_handler (*(int *)arg1);
        break;
      case SYS_READ:
        if (!is_valid_pointer (arg2) || !is_valid_pointer (arg3))
          thread_exit ();
        f->eax = read_handler (*(int *)arg1, *(char **)arg2, 
                               *(unsigned *)arg3);
        break;
      case SYS_WRITE:
        if (!is_valid_pointer (arg2) || !is_valid_pointer (arg3))
          thread_exit ();
        f->eax = write_handler (*(int *)arg1, *(char **)arg2, 
                                *(unsigned *)arg3);
        break;
      case SYS_SEEK:
        if (!is_valid_pointer (arg2))
          thread_exit ();
        seek_handler (*(int *)arg1, *(unsigned *)arg2);
        break;
      case SYS_TELL:
        f->eax = tell_handler (*(int *)arg1);
        break;
      case SYS_CLOSE:
        close_handler (*(int *)arg1);
        break;
      case SYS_MMAP:
        if (!is_valid_pointer (arg2))
          thread_exit ();
        f->eax = mmaphandler (*(int *)arg1, *(void **)arg2);
        break;
      case SYS_MUNMAP:
        munmap_handler (*(mapid_t *)arg1);
        break;
      default:
        thread_exit ();
    }
}

/* Terminates Pintos. */
static void 
halt_handler ()
{
  shutdown_power_off ();
}

/* Terminates the current user program, returning status to the kernel. */
static void 
exit_handler (int status)
{  
  struct thread *c = thread_current ();
  struct child_thread_info *cur_child = find_child_thread (c->parent, c->tid);
  cur_child->exit_status = status;

  thread_exit ();
  NOT_REACHED ();
}

/* Runs the executable file and returns the new process’s program id (tid). 
Returns tid -1, if the program cannot load or run for any reason. */
static int 
exec_handler (char *file)
{
  if (!is_valid_char_pointer (file))
    thread_exit (); 

  int tid = process_execute (file);
  struct child_thread_info *child = find_child_thread (thread_current (), tid);
  sema_down (&child->load_sema);
  return (child->loaded) ? tid : -1;   
}

/* Waits for a child process pid and retrieves the child’s exit status. */
static int 
wait_handler (int pid)
{
  return process_wait (pid);
}

/* Creates a new file named file initially initial size bytes in size. 
Returns true if successful, false otherwise. */
static bool 
create_handler (char *file, unsigned initial_size)
{
  if (!is_valid_char_pointer (file))
    thread_exit ();

  lock_acquire (&file_lock);
  bool success = filesys_create (file, initial_size);
  lock_release (&file_lock);
  return success;
}

/* Deletes the file named file. Returns true if successful, false otherwise. */
static bool 
remove_handler (char *file)
{
  if (!is_valid_char_pointer (file))
    thread_exit ();

  lock_acquire (&file_lock);
  bool success = filesys_remove (file);
  lock_release (&file_lock);
  return success;
}

/* Opens the file called file. Returns its fd if successful, -1 otherwise. */
static int 
open_handler (char *file)
{
  if (!is_valid_char_pointer (file))
    thread_exit ();

  lock_acquire (&file_lock);
  struct file *fp = filesys_open (file);
  lock_release (&file_lock);

  if (fp == NULL)
    return -1;

  struct pcb *pcb = thread_current ()->pcb;
  struct fds *fd_table_entry = malloc (sizeof (struct fds));

  fd_table_entry->fd = pcb->highest_fd + 1;
  fd_table_entry->file_name = file;
  fd_table_entry->fp = fp;
  
  pcb->highest_fd += 1;

  list_push_back (&pcb->fd_table, &fd_table_entry->elem);
  return fd_table_entry->fd;
}

/* Returns the size, in bytes, of the file open as fd. */
static int 
filesize_handler (int fd)
{
  struct file *fp = get_file_from_fd (fd);
  if (fp == NULL)
    thread_exit ();
  
  lock_acquire (&file_lock);
  int size = file_length (fp);
  lock_release (&file_lock);
  return size;
}

/* Reads size bytes from the file open as fd into buffer. Returns the number of 
bytes actually read or -1 if it couldn't be read. */
static int 
read_handler (int fd, char *buffer, unsigned size)
{
  if (!is_valid_pointer_with_size (buffer, sizeof (char) * size, true))
    thread_exit ();

  if (fd == 0)
    {
      //read from keyboard
      int i = 0;
      char c;
      while (i < size && (c = input_getc ()) != '\n')
        {
          buffer[i++] = c;
        }
      buffer[i] = '\0';
      return i;
    }

  struct file *fp = get_file_from_fd (fd);
  if (fp == NULL)
    return -1;
  
  // lock_acquire (&file_lock);
  int read_length = file_read (fp, buffer, size);
  // lock_release (&file_lock);
  return read_length;
}

/* Writes size bytes from buffer to the open file fd. Returns the number of
bytes actually written. */
static int 
write_handler (int fd, char *buffer, unsigned length)
{
  if (!is_valid_pointer_with_size (buffer, sizeof (char) * length, true))
    thread_exit ();

  if (fd == 1)
    {
      putbuf (buffer, length);
      return length;
    }
  
  struct file *fp = get_file_from_fd (fd);
  if (fp == NULL)
    return -1;
  
  // lock_acquire (&file_lock);
  int write_length = file_write (fp, buffer, length);
  // lock_release (&file_lock);
  return write_length;
}

/* Changes the next byte to be read or written in open file fd to position, 
expressed in bytes from the beginning of the file. */
static void 
seek_handler (int fd, unsigned position)
{
  struct file *fp = get_file_from_fd (fd);
  if (fp == NULL)
    thread_exit ();

  lock_acquire (&file_lock);
  file_seek (fp, position);
  lock_release (&file_lock);
}

/* Returns the position of the next byte to be read or written in open file fd, 
expressed in bytes from the beginning of the file. */
static unsigned 
tell_handler (int fd)
{
  struct file *fp = get_file_from_fd (fd);

  if (fp == NULL)
    {
      thread_exit ();
    }
  
  lock_acquire (&file_lock);
  unsigned position = file_tell (fp);
  lock_release (&file_lock);
  return position;
}

/* Closes file with descriptor fd. */
static void 
close_handler (int fd)
{
  struct list_elem *file_elem = get_file_elem_from_fd (fd);
  if (file_elem == NULL)
    return;

  struct fds *fds = list_entry (file_elem, struct fds, elem);
  lock_acquire (&file_lock);
  file_close (fds->fp);
  lock_release (&file_lock);
  list_remove (file_elem);
  free (fds);
}

/* Maps the file open as fd into the process’s virtual address space. 
The entire file ismapped into consecutive virtual pages starting at addr. */
static mapid_t 
mmaphandler (int fd, void *addr)
{
  if (fd < 2 || (uintptr_t)addr == 0 || (uintptr_t)addr % PGSIZE != 0)
    return -1;

  struct list_elem *file_elem = get_file_elem_from_fd (fd);
  if (file_elem == NULL)
    return -1;

  struct fds *fds = list_entry (file_elem, struct fds, elem);
  struct file *file = fds->fp;
  if (file == NULL || file_length (file) == 0)
    return -1;

  file = file_reopen (file);
  uint32_t read_bytes = file_length (file);
  uint32_t zero_bytes = PGSIZE - (read_bytes % PGSIZE);

  int page_num = read_bytes / PGSIZE + 1;
  for (int i = 0; i < page_num; i++)
  {
    void * page_addr = addr + (PGSIZE * i);
    if (get_spt_entry (page_addr) != NULL) 
      return -1;
  }
  
  if (!load_segment (file_reopen (fds->fp), 0, addr, read_bytes,zero_bytes, true))
    return -1;

  thread_current ()->last_mapid++;
  struct mapped_file *mapped_file_entry = malloc (sizeof (struct mapped_file));
  mapped_file_entry->addr = addr;
  mapped_file_entry->file = file;
  mapped_file_entry->size = read_bytes;
  mapped_file_entry->id = thread_current ()->last_mapid;
  hash_insert (&thread_current ()->mappings, &mapped_file_entry->hash_elem);
  
  return mapped_file_entry->id;
}

/* Unmaps the mapping designated by mapping, which must be a mapping ID returned
by a previous call to mmap by the same process that has not yet been unmapped.*/
static void 
munmap_handler (mapid_t id)
{
  struct mapped_file *mapped_file = get_mapped_file (id);
  if (mapped_file == NULL)
    return;

  mapped_file_destory (&mapped_file->hash_elem, NULL);
}

/* Checks if the provided pointer is valid. */
static bool 
is_valid_pointer_with_size (void *p, int size, bool can_grow_stack)
{
  uintptr_t addr = (uintptr_t)p;
  bool is_mapped_virtual_memory = get_spt_entry (p) != NULL
                                  && is_user_vaddr (p+size-1) 
                                  && (addr / PGSIZE == (addr+size-1) / PGSIZE
                                      || get_spt_entry (p+size-1) != NULL);
  bool growing_stack = (uint32_t *)p >= (esp - 32);

  return p != NULL && is_user_vaddr (p) 
         && (is_mapped_virtual_memory || (can_grow_stack && growing_stack));        
}

/* Checks if the provided pointer is valid if its an int, unsigned or char * 
pointer. */
static bool 
is_valid_pointer (void *p)
{
  return is_valid_pointer_with_size (p, 4, false);
}

/* Checks if char pointer is valid. */
static bool
is_valid_char_pointer (char *p)
{
  uintptr_t addr = (uintptr_t)p;
  if (p == NULL || is_kernel_vaddr (p) || get_spt_entry (p) == NULL)
    return false;

  int length = 0;
  bool checked_page_boundary = false;
  while (length < MAX_FILE_NAME_LENGTH && *(p+length) != '\0')
    {
      length++;

      if (is_kernel_vaddr (p+length))
        return false;

      if (!checked_page_boundary && addr / PGSIZE < (addr+length) / PGSIZE)
        {
          checked_page_boundary = true;
          if (get_spt_entry (p+length) == NULL)
            return false;
        }
    }
    
  return true;
}