#include <hash.h>
#include "threads/palloc.h"

struct frame
	{
		struct hash_elem hash_elem;				/* Hash element hash table. */
		struct thread *owner;				  		/* The owner thread of the frame */
		bool pinned;											/* Indicates if the frame is pinned */
		struct spt_entry *spe;						/* SPT entry associated with the frame */
		void *kpage;											/* Physical Page address */
	};

void init_frame_table (void);
struct frame *init_frame (enum palloc_flags);
void free_frame (struct frame *);
bool link_frame_to_uaddr (void *, void *, bool);
struct frame *frame_evict_get (void);
void frame_evict (struct frame *);