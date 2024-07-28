#include "threads/malloc.h"
#include "threads/synch.h"
#include "userprog/process.h"
#include "vm/page.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"



static struct hash frame_table;
struct lock frame_table_lock;


static unsigned frame_hash (const struct hash_elem *p_, void *aux UNUSED);
static bool frame_less (const struct hash_elem *a_, const struct hash_elem *b_,
                        void *aux UNUSED);




/* Initialize OS wide frame table */
void init_frame_table()
	{
		lock_init(&frame_table_lock);
		hash_init (&frame_table, frame_hash, frame_less, NULL);
	}


struct frame *
init_frame (enum palloc_flags flags)
	{
		// lock_acquire(&frame_table_lock);

		void *page = palloc_get_page (flags);

  	if (page == NULL)
    	{
      	struct frame *f = frame_evict_get ();
		frame_evict (f);
		page = palloc_get_page (flags);
		// printf("NEW PAGE: %p\n", page);
    	}

	struct frame *f = malloc (sizeof (struct frame));

	if (f == NULL)
{
						PANIC("MALLOC FAILED");
					}

      	f->kpage = page;
		f->pinned = false;
		f->owner = thread_current();
		
				hash_insert (&frame_table, &f->hash_elem);
				// lock_release(&frame_table_lock);		
				return f;
	}

bool 
link_frame_to_uaddr(void *uaddr, void *kpage, bool writable)
	{
		lock_acquire(&frame_table_lock);
		bool install_success = install_page (uaddr, kpage, writable);
		lock_release(&frame_table_lock);		
		return install_success;																			
	}

/*Free frame*/
void free_frame(struct frame *f)
	{
		lock_acquire(&frame_table_lock);
		hash_delete(&frame_table, &f->hash_elem);

		palloc_free_page(f->kpage);
		free(f);
		lock_release(&frame_table_lock);
	}

struct frame *
frame_evict_get ()
	{
		lock_acquire (&frame_table_lock);
		for (int j = 0; j < 2; j++)
			{
				struct hash_iterator i;
				hash_first (&i, &frame_table);
				while (hash_next (&i))
					{
					struct frame *f = hash_entry (hash_cur (&i), struct frame, hash_elem);
					struct spt_entry *spe = f->spe;
					if (!f->pinned)
						{
							if (f->owner->pagedir != NULL && pagedir_is_accessed (f->owner->pagedir, spe->uaddr))
								{
									pagedir_set_accessed (f->owner->pagedir, spe->uaddr, false);
								}
							else
								{
									// hash_delete (&frame_table, &f->hash_elem);
									lock_release (&frame_table_lock);
									return f;
								}
						}
					}
			}
		lock_release (&frame_table_lock);
		return NULL;
	}

void
frame_evict (struct frame *f)
	{
		struct spt_entry *spe = f->spe;
		bool is_dirty = pagedir_is_dirty (f->owner->pagedir, spe->uaddr);
		enum vpt type = spe->vpt;

		if (type == SWAP)
			{
				spe->swap_index = swap_out (f->kpage);
			}
		if (type == ELF_FILE && is_dirty)
			{
				spe->swap_index = swap_out (f->kpage);
				spe->vpt = SWAP;
			}
		if (type == GEN_FILE && is_dirty)
			{
				file_write_at (spe->file, f->kpage, spe->file_page_size, spe->file_offset);
			}

		hash_delete (&frame_table, &f->hash_elem);
		pagedir_clear_page(f->owner->pagedir, spe->uaddr);
		free_frame (f);
		spe->in_memory = false;
	}

/* Returns a hash value for frame p. */
static unsigned
frame_hash (const struct hash_elem *p_, void *aux UNUSED)
	{
  	const struct frame *p = hash_entry (p_, struct frame, hash_elem);
  	return hash_bytes (&p->kpage, sizeof p->kpage);
	}

/* Returns true if frame a precedes frame b. */
static bool
frame_less (const struct hash_elem *a_, const struct hash_elem *b_,
            void *aux UNUSED)
	{
  	const struct frame *a = hash_entry (a_, struct frame, hash_elem);
  	const struct frame *b = hash_entry (b_, struct frame, hash_elem);
  	return a->kpage < b->kpage;
	}
