#include "vm/mapped_file.h"
#include "vm/page.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"


static unsigned mapped_file_hash (const struct hash_elem *p_, 
																	void *aux UNUSED);
static bool mapped_file_less (const struct hash_elem *a_, 
															const struct hash_elem *b_,
                      				void *aux UNUSED);

/* Initialize mapping table */
void
init_mappings (struct thread *t)
	{
		hash_init (&t->mappings, mapped_file_hash, mapped_file_less, NULL);
	}

/* Return mapped_file for UADDR in current threads mappings */
struct mapped_file *
get_mapped_file(mapid_t mapid)
	{
		struct mapped_file mapped_file_entry;

  	mapped_file_entry.id = mapid;
		struct hash_elem *hash_elem = hash_find (&thread_current()->mappings, 
																						&mapped_file_entry.hash_elem);

		if(hash_elem != NULL)
			return hash_entry (hash_elem, struct mapped_file, hash_elem);
	
		return NULL;
	}


void 
mapped_file_destory(struct hash_elem *e, void *aux UNUSED)
	{
		struct mapped_file *mapped_file = hash_entry (e, struct mapped_file, 
																									hash_elem);
		
		int size = mapped_file->size;
		int page_num = 0;

		while (size > 0)
		{
			void *cur_addr = mapped_file->addr + (PGSIZE * page_num);
			struct spt_entry *spt_entry = get_spt_entry (cur_addr);

			if (pagedir_is_dirty (thread_current ()->pagedir, cur_addr))
				file_write_at (spt_entry->file, spt_entry->uaddr, 
											spt_entry->file_page_size,
											spt_entry->file_offset);

			spt_entry_destory (&spt_entry->hash_elem, NULL);

			page_num++;
			size -= PGSIZE;
		}

		hash_delete (&thread_current()->mappings, &mapped_file->hash_elem);
		free (mapped_file);
	}

static unsigned
mapped_file_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct mapped_file *mapped_file_entry = hash_entry (p_, 
																														struct mapped_file, 
																														hash_elem);
  return hash_bytes (&mapped_file_entry->id, sizeof mapped_file_entry->id);
}


static bool
mapped_file_less (const struct hash_elem *a_, const struct hash_elem *b_,
            			void *aux UNUSED)
{
  const struct mapped_file *a = hash_entry (a_, struct mapped_file, hash_elem);
  const struct mapped_file *b = hash_entry (b_, struct mapped_file, hash_elem);
  return a->id < b->id;
}