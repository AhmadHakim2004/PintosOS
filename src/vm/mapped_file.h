#include <stddef.h>
#include <hash.h>
#include "lib/user/syscall.h"
#include "filesys/file.h"
#include "threads/thread.h"


struct mapped_file
	{
		mapid_t id;												 /* Map ID */
		void *addr;                        /* Mapped file's user address */ 
		struct file *file;                 /* Mapped file */
		size_t size;       								 /* Mapped file's size */

		struct hash_elem hash_elem;				 /* Hash elem for hash table */
	};

void init_mappings (struct thread *);
struct mapped_file *get_mapped_file (mapid_t mapid);
void mapped_file_destory (struct hash_elem *, void *);