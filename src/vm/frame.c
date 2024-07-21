#include "vm/frame.h"
#include "threads/malloc.h"
#include "threads/palloc.h"


static struct hash frame_table;


static unsigned frame_hash (const struct hash_elem *p_, void *aux UNUSED);
static bool frame_less (const struct hash_elem *a_, const struct hash_elem *b_,
                        void *aux UNUSED);



/* Initialize OS wide frame table */
void init_frame_table()
	{
		hash_init (&frame_table, frame_hash, frame_less, NULL);
	}


struct frame *
init_frame(enum palloc_flags flags)
	{
		void *page = palloc_get_page (flags);

  	if (page != NULL)
    	{
      	struct frame *f = malloc (sizeof (struct frame));
      	f->kpage = page;
				hash_insert (&frame_table, &f->hash_elem);
				return f;
    	}

		//Need to change later
  	PANIC ("NO MORE FRAMES");
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