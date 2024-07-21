#include <hash.h>
#include "threads/palloc.h"

struct frame
	{
		struct hash_elem hash_elem;
		uint8_t *kpage;
	};

void init_frame_table(void);
struct frame *init_frame(enum palloc_flags);
void free_frame(struct frame *);