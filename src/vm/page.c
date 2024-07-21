#include "vm/page.h"
#include <hash.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/vaddr.h"


static unsigned spt_hash (const struct hash_elem *p_, void *aux UNUSED);
static bool spt_less (const struct hash_elem *a_, const struct hash_elem *b_,
                      void *aux UNUSED);


void
init_spt (struct thread *t)
	{
		hash_init (&t->spt, spt_hash, spt_less, NULL);
	}

/* Return spt_entry for UADDR in current threads spt */
struct spt_entry *
get_spt_entry(void *uaddr)
	{
		struct spt_entry spt_entry;

  	spt_entry.uaddr = (uint8_t *)pg_round_down (uaddr);
  	struct hash_elem *hash_elem_f = hash_find(&thread_current()->spt, 
                                            &spt_entry.hash_elem);

  	if(hash_elem_f != NULL)
    	{
      	return hash_entry(hash_elem_f, struct spt_entry, hash_elem);
    	}
  
  	return NULL;
	}


bool 
load_file_page_to_mem (void *kpage, struct spt_entry *spt_entry)
	{
		// off_t saved_ofs = file_tell (spt_entry->file);
		// file_seek(spt_entry->file, spt_entry->file_offset);

    // if (file_read (file_reopen(spt_entry->file), kpage,
		// 		spt_entry->file_page_size) != (int) spt_entry->file_page_size)
    //   {
    //     return false; 
 		// 	}	
			
		// file_seek(spt_entry->file, saved_ofs);

		if (file_read_at (spt_entry->file, kpage, spt_entry->file_page_size, 
											spt_entry->file_offset) != (int) spt_entry->file_page_size)
			{
				return false;
			}

			
		memset(kpage + spt_entry->file_page_size, 0,
		PGSIZE - spt_entry->file_page_size);
	}


static unsigned
spt_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct spt_entry *spt_entry = hash_entry (p_, struct spt_entry, 
																									hash_elem);
  return hash_bytes (&spt_entry->uaddr, sizeof spt_entry->uaddr);
}


static bool
spt_less (const struct hash_elem *a_, const struct hash_elem *b_,
            void *aux UNUSED)
{
  const struct spt_entry *a = hash_entry (a_, struct spt_entry, hash_elem);
  const struct spt_entry *b = hash_entry (b_, struct spt_entry, hash_elem);
  return a->uaddr < b->uaddr;
}