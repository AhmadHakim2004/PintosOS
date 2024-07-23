#include <stddef.h>
#include <hash.h>
#include "lib/user/syscall.h"
#include "filesys/file.h"
#include "threads/thread.h"


struct mapped_file
	{
		mapid_t id;
		void *addr;
		struct file *file;
		size_t size;

		struct hash_elem hash_elem;
	};

void init_mappings (struct thread *);
struct mapped_file *get_mapped_file(mapid_t mapid);
void mapped_file_destory(struct hash_elem *, void *);