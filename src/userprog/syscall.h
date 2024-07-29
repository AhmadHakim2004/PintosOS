#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
struct lock file_lock;		// Must aquire to execute file-related functions
#endif /* userprog/syscall.h */
