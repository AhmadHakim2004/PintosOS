#include "vm/page.h"
#include <hash.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include <ctype.h>
#include <stdio.h>

static unsigned spt_hash (const struct hash_elem *p_, void *aux UNUSED);
static bool spt_less (const struct hash_elem *a_, const struct hash_elem *b_,
                      void *aux UNUSED);

void
init_spt (struct thread *t)
	{
		hash_init (&t->spt, spt_hash, spt_less, NULL);
	}

struct spt_entry *
init_spt_entry (uint8_t *uaddr, enum palloc_flags flags, struct file *file, off_t file_offset, 
				size_t file_page_size, bool writable, bool in_memory, enum vpt vpt)
	{
		struct spt_entry *spt_entry = malloc(sizeof (struct spt_entry));
        spt_entry->uaddr = uaddr;
		if (flags != 0)
        	init_frame (flags, spt_entry);
      	spt_entry->file = file;
        spt_entry->file_offset = file_offset;
        spt_entry->file_page_size = file_page_size;
        spt_entry->writable = writable;
		spt_entry->in_memory = in_memory;
		spt_entry->vpt = vpt;
		hash_insert(&thread_current()->spt, &spt_entry->hash_elem);
		return spt_entry;
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
		// lock_acquire (&file_lock);
		if (file_read_at (spt_entry->file, kpage, spt_entry->file_page_size, 
											spt_entry->file_offset) != (int) spt_entry->file_page_size)
			{
				return false;
			}
		// print_ascii (kpage, spt_entry->file_page_size);
			
		memset(kpage + spt_entry->file_page_size, 0,
		PGSIZE - spt_entry->file_page_size);
		// lock_release (&file_lock);
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