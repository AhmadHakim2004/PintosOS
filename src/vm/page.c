#include "vm/page.h"
#include <hash.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

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
		if (file_read_at (spt_entry->file, kpage, spt_entry->file_page_size, 
											spt_entry->file_offset) != (int) spt_entry->file_page_size)
			{
				return false;
			}
			
		memset(kpage + spt_entry->file_page_size, 0,
		PGSIZE - spt_entry->file_page_size);
		return true;
	}

void spt_entry_destory(struct hash_elem *e, void *aux UNUSED)
	{
		struct spt_entry *spt_entry = hash_entry(e, struct spt_entry, hash_elem);
		pagedir_clear_page(thread_current()->pagedir, spt_entry->uaddr);
		if (spt_entry->frame != NULL)
			free_frame(spt_entry->frame);
		hash_delete(&thread_current()->spt, &spt_entry->hash_elem);
		free(spt_entry);
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