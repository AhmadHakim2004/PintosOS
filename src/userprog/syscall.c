#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "pagedir.h"
#include "devices/shutdown.h"
#include "threads/synch.h"
#include "process.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"

static void syscall_handler (struct intr_frame *);
static void halt_handler (void);
static void exit_handler (int);
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
static bool is_valid_pointer (void *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int *interrupt_code = (int *)f->esp;
  void *arg1 = f->esp+4;
  void *arg2 = f->esp+8;
  void *arg3 = f->esp+12;

  if (!is_valid_pointer (interrupt_code) 
      || (*interrupt_code != 0 && !is_valid_pointer (arg1)))
    {
      exit_handler (-1);
    }
  switch (*interrupt_code)
    {
      case 0:
        halt_handler ();
        break;
      case 1:
        exit_handler (*(int *)arg1);
        break;
      case 2:
        f->eax = exec_handler (*(char **)arg1);
        break;
      case 3:
        f->eax = wait_handler (*(int *)arg1);
        break;
      case 4:
        if (!is_valid_pointer (arg2))
          {
            exit_handler (-1);
          }
        f->eax = create_handler (*(char **)arg1, *(unsigned *)arg2);
        break;
      case 5:
        f->eax = remove_handler (*(char **)arg1);
        break;
      case 6:
        f->eax = open_handler (*(char **)arg1);
        break;
      case 7:
        f->eax = filesize_handler (*(int *)arg1);
        break;
      case 8:
        if (!is_valid_pointer (arg2) || !is_valid_pointer (arg3))
          {
            exit_handler (-1);
          }
        f->eax = read_handler (*(int *)arg1, *(char **)arg2, *(unsigned *)arg3);
        break;
      case 9:
        if (!is_valid_pointer (arg2) || !is_valid_pointer (arg3))
          {
            exit_handler (-1);
          }
        f->eax = write_handler (*(int *)arg1, *(char **)arg2, *(unsigned *)arg3);
        break;
      case 10:
        if (!is_valid_pointer (arg2))
          {
            exit_handler (-1);
          }
        seek_handler (*(int *)arg1, *(unsigned *)arg2);
        break;
      case 11:
        f->eax = tell_handler (*(int *)arg1);
        break;
      case 12:
        close_handler (*(int *)arg1);
        break;
      default:
        exit_handler (-1);
    }
}

static void 
halt_handler ()
{
  shutdown_power_off ();
}

static void 
exit_handler (int status)
{  
  struct child_thread_info *cti = find_cti (thread_current()->parent, 
                                            thread_current ()->tid);
  cti->exit_status = status;
  cti->exited = true;
  sema_up (&cti->exit_sema);

  printf ("%s: exit(%d)\n", thread_current ()->name, status);
  thread_exit ();
}

static int 
exec_handler (char *file)
{
  if (is_valid_pointer (file))
  {
    int tid = process_execute (file);
    struct child_thread_info *cti = find_cti (thread_current(), tid);
    sema_down (&cti->load_sema);
    return (cti->loaded) ? tid : -1;
  }
  else 
    exit_handler (-1); 
}

static int 
wait_handler (int pid)
{
  return process_wait (pid);
}

static bool 
create_handler (char *file, unsigned initial_size)
{
  if(!is_valid_pointer(file))
    {
      exit_handler (-1);
    }

  return filesys_create(file, initial_size);
}

static bool 
remove_handler (char *file)
{
  printf("remove_handler not implemented yet");
  return false;
}

static int 
open_handler (char *file)
{

  if(!is_valid_pointer (file))
    {
      exit_handler (-1);
    }

  struct file *fp = filesys_open (file);

  if(fp == NULL)
    {
      return -1;
    }

  struct pcb *pcb = thread_current () -> pcb;
  
  struct fds *fd_table_entry = malloc (sizeof(struct fds));

  fd_table_entry->fd = pcb->highest_fd + 1;
  fd_table_entry->file_name = file;
  fd_table_entry->fp = fp;
  
  //increase highest fd of process
  pcb->highest_fd += 1;

  list_push_back(&pcb->fd_table, &fd_table_entry->elem);
  return fd_table_entry->fd;
}

static int 
filesize_handler (int fd)
{
  printf("filesize_handler not implemented yet");
  return -1;
}

static int 
read_handler (int fd, char *buffer, unsigned length)
{
  printf("read_handler not implemented yet");
  return -1;
}

static int 
write_handler (int fd, char *buffer, unsigned length)
{
  if (fd == 1)
    {
      putbuf (buffer, length);
      return length;
    }
  return -1;
}

static void 
seek_handler (int fd, unsigned postition)
{
  printf("seek_handler not implemented yet");
}

static unsigned 
tell_handler (int fd)
{
  printf("tell_handler not implemented yet");
  return -1;
}

static void 
close_handler (int fd)
{
  printf("close_handler not implemented yet");
}

static bool 
is_valid_pointer (void *p)
{
  return p != NULL 
         && is_user_vaddr (p) 
         && pagedir_get_page (thread_current ()->pagedir, p) != NULL 
         && is_user_vaddr (p+3) 
         && pagedir_get_page (thread_current ()->pagedir, p+3) != NULL;
}