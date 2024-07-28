#include <hash.h>
#include <stdbool.h>
#include "filesys/file.h"
#include "threads/thread.h"
#include "vm/frame.h"


/* Virtual Page Type */
enum vpt
	{
		ELF_FILE,																/* READ ONLY file */
		GEN_FILE,
		SWAP
	};

struct spt_entry
	{
		struct hash_elem hash_elem;
		uint8_t *uaddr;
		struct frame *frame;
		struct file *file;
		off_t file_offset;
		size_t file_page_size;
		bool writable;
		bool in_memory;
		enum vpt vpt;
		int swap_index;
	};	

void init_spt (struct thread *);
struct spt_entry * init_spt_entry (uint8_t *, struct frame *, struct file *, 
								    off_t, size_t, bool, bool, enum vpt);
struct spt_entry *get_spt_entry(void *uaddr);
bool load_file_page_to_mem (void *kpage, struct spt_entry *);

// TODO: Need to implement 
void spt_entry_destory (struct hash_elem *, void *);