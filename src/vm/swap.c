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

void init_swap_table (void)
{
  lock_init (&swap_table_lock);
  swap_block = block_get_role (BLOCK_SWAP);

  swap_table_size = (block_size (swap_block) * BLOCK_SECTOR_SIZE) / PGSIZE;
  swap_table = malloc(swap_table_size * sizeof(struct swap_entry));

    for (int i = 0; i < swap_table_size; i++)
        {
        lock_acquire (&swap_table_lock);
        swap_table[i].free = true;
        lock_release (&swap_table_lock);
        }

}

void write_swap (void *uaddr, int index)
{
  int sectors = PGSIZE / BLOCK_SECTOR_SIZE;
  for (int i = 0; i < sectors; i++)
  {
    lock_acquire (&swap_table_lock);
    block_write (swap_block, index * sectors + i, uaddr + i * BLOCK_SECTOR_SIZE);
    lock_release (&swap_table_lock);
  }
}

void read_swap (void *uaddr, int index)
{
  int sectors = PGSIZE / BLOCK_SECTOR_SIZE;
  for (int i = 0; i < sectors; i++)
  {
    lock_acquire (&swap_table_lock);
    block_read (swap_block, index * sectors + i, uaddr + i * BLOCK_SECTOR_SIZE);
    lock_release (&swap_table_lock);
  }
}

int swap_out (void *uaddr)
{
  for (int i = 0; i < swap_table_size; i++)
    {
      if (swap_table[i].free)
        {
          lock_acquire (&swap_table_lock);
          swap_table[i].free = false;
          lock_release (&swap_table_lock);
          write_swap (uaddr, i);
          return i;
        }
    }
  return -1;
}

void swap_in (void *uaddr, int index)
{
  if (index < 0 || index >= swap_table_size)
    return;
  if (swap_table[index].free)
    return;

  read_swap (uaddr, index);
  lock_acquire (&swap_table_lock);
  swap_table[index].free = true;
  lock_release (&swap_table_lock);
}

