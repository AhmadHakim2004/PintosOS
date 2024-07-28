#include <hash.h>
#include "threads/palloc.h"

struct frame
	{
		struct hash_elem hash_elem;
		struct thread *owner;
		bool pinned;
		struct spt_entry *spe;
		void *kpage;
	};

void init_frame_table(void);
struct frame *init_frame(enum palloc_flags);
void free_frame(struct frame *);
bool link_frame_to_uaddr(void *, void *, bool);
struct frame *frame_evict_get(void);
void frame_evict(struct frame *);