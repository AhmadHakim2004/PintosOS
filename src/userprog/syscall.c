#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "pagedir.h"

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
  printf("halt_handler not implemented yet");
}

static void 
exit_handler (int status)
{  
  thread_exit ();
}

static int 
exec_handler (char *file)
{
  printf("exec_handler not implemented yet");
  return -1;
}

static int 
wait_handler (int pid)
{
  printf("wait_handler not implemented yet");
  return -1;
}

static bool 
create_handler (char *file, unsigned initial_size)
{
  printf("create_handler not implemented yet");
  return false;
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
  printf("open_handler not implemented yet");
  return -1;
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
  printf("write_handler not implemented yet");
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