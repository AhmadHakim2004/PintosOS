#include <hash.h>

struct swap_entry
{
  bool free;
};

void init_swap_table (void);
int pick_swap (void *uaddr);
void free_swap (void *uaddr);
struct swap_entry *get_swap_entry (int index);
void swap_in (void *uaddr, int index);
int swap_out (void *uaddr);
void write_swap (void *uaddr, int index);
void read_swap (void *uaddr, int index);