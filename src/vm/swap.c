#include "vm/swap.h"
#include "threads/malloc.h"
#include <hash.h>
#include "devices/block.h"
#include "vm/page.h"
#include "threads/vaddr.h"

static struct swap_entry *swap_table;
static struct lock swap_table_lock;
static struct block *swap_block;
static int swap_table_size;

/* Initializes the swap table and the swap table lock */
void 
init_swap_table (void)
{
  lock_init (&swap_table_lock);
  swap_block = block_get_role (BLOCK_SWAP);

  swap_table_size = (block_size (swap_block) * BLOCK_SECTOR_SIZE) / PGSIZE;
  swap_table = malloc (swap_table_size * sizeof (struct swap_entry));

    for (int i = 0; i < swap_table_size; i++)
      {
        swap_table[i].free = true;
      }

}

/* Writes bytes from UADDR to a swap slot with index INDEX */
void 
write_swap (void *uaddr, int index)
{
  int sectors = PGSIZE / BLOCK_SECTOR_SIZE;
  for (int i = 0; i < sectors; i++)
    {
      block_write (swap_block, index * sectors + i, 
                   uaddr + i * BLOCK_SECTOR_SIZE);
    }
}


/* Reads bytes from swap slot at index INDEX to UADDR */
void 
read_swap (void *uaddr, int index)
{
  int sectors = PGSIZE / BLOCK_SECTOR_SIZE;
  for (int i = 0; i < sectors; i++)
    {
      block_read (swap_block, index * sectors + i, 
                  uaddr + i * BLOCK_SECTOR_SIZE);
    }
}

/* Finds a free swap and swaps out the page at UADDR */
int 
swap_out (void *uaddr)
{
  lock_acquire (&swap_table_lock);
  for (int i = 0; i < swap_table_size; i++)
    {
      if (swap_table[i].free)
        {
          swap_table[i].free = false;
          write_swap (uaddr, i);
          lock_release (&swap_table_lock);
          return i;
        }
    }

  lock_release (&swap_table_lock);
  return -1;
}

/* Swaps in the data from swap entry at INDEX in the swap table 
   to page at UADDR */
void 
swap_in (void *uaddr, int index)
{
  if (index < 0 || index >= swap_table_size)
    return;
  if (swap_table[index].free)
    return;

  lock_acquire (&swap_table_lock);
  swap_table[index].free = true;
  read_swap (uaddr, index);
  lock_release (&swap_table_lock);
}

