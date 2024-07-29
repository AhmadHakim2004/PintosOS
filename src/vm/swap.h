#include <hash.h>

/* Structure representing an entry in the swap table. */

struct swap_entry
{
  bool free;   /* Indicates whether the swap slot is free */
};

void init_swap_table (void);
struct swap_entry *get_swap_entry (int);
void swap_in (void *, int);
int swap_out (void *);
void write_swap (void *, int);
void read_swap (void *, int);