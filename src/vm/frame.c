#include "vm/frame.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "userprog/process.h"



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
		void *page = palloc_get_page (flags);

  	if (page != NULL)
    	{
      	struct frame *f = malloc (sizeof (struct frame));

				if (f == NULL)
					{
						PANIC("MALLOC FAILED");
					}

      	f->kpage = page;
				hash_insert (&frame_table, &f->hash_elem);
				return f;
    	}

		//Need to change later
  	PANIC ("NO MORE FRAMES");
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
void
free_frame(struct frame *f)
	{
		free(f);
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