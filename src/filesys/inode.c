#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define DIRECT_BLOCKS 124

#define INDIRECT_PTRS 128

#define MAX_DIRECT_BYTE \
    (DIRECT_BLOCKS * BLOCK_SECTOR_SIZE)

#define MAX_INDIRECT_BYTE \
    ((INDIRECT_PTRS * BLOCK_SECTOR_SIZE) \
    + MAX_DIRECT_BYTE)

#define MAX_DINDIRECT_BYTE \
    ((INDIRECT_PTRS * INDIRECT_PTRS * BLOCK_SECTOR_SIZE) \
    + MAX_INDIRECT_BYTE)

static int inode_create_indirect (block_sector_t *indirect_bock, size_t num_sectors);
static int inode_create_db_indirect (block_sector_t *db_indirect, size_t num_sectors);

static char zeroed_sector[BLOCK_SECTOR_SIZE];

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    off_t length;                         /* File size in bytes. */
    unsigned magic;                       /* Magic number. */
    block_sector_t direct[DIRECT_BLOCKS]; /* Direct blocks. */
    block_sector_t indirect;              /* Indirect blocks. */
    block_sector_t doubly_indirect;       /* Doubly indirect blocks. */
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
    struct lock inode_lock;             /* Lock for inode. */
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  
  uint32_t sector_idx;
  block_sector_t db_buffer[128];
  block_sector_t db_buffer2[128];

  if (pos < MAX_DIRECT_BYTE)
    {
      return inode->data.direct[pos / BLOCK_SECTOR_SIZE];
    }
  
  else if (pos < MAX_INDIRECT_BYTE)
    {
      sector_idx = (pos - MAX_DIRECT_BYTE) / BLOCK_SECTOR_SIZE;
      block_read (fs_device, inode->data.indirect, &db_buffer);
      
      block_sector_t sector = db_buffer[sector_idx];
      if (!sector)
      {
        return -1;
      }
      return sector;
    }
  
  else if (pos < MAX_DINDIRECT_BYTE)
    {
      uint32_t indirect_idx = (pos - MAX_INDIRECT_BYTE) / 
                              (INDIRECT_PTRS * BLOCK_SECTOR_SIZE);
      sector_idx = (pos - MAX_INDIRECT_BYTE) % 
                   (INDIRECT_PTRS * BLOCK_SECTOR_SIZE) / BLOCK_SECTOR_SIZE;

      block_read (fs_device, inode->data.doubly_indirect, db_buffer);
        
      block_read (fs_device, db_buffer[indirect_idx], db_buffer2);
        
      block_sector_t sector = db_buffer2[sector_idx];
       
      if (!sector)
        {
          return -1;
        }
      
      return sector;
    }
  else 
    {
      return -1;
    } 
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}


/* Allocates direct, indirect and doubly indirect per requirement NUM_SECTOR 
   sectors for inode DISK_INODE */
static bool 
allocate_blocks (struct inode_disk *disk_inode, size_t num_sectors)
{
  bool success = false;
  bool all_written = false;
  int i = 0;

  while (i < DIRECT_BLOCKS)
    {
      if(!disk_inode->direct[i])
        {
          if(free_map_allocate (1, &disk_inode->direct[i]))
            {
              block_write (fs_device, disk_inode->direct[i], zeroed_sector);
            }
          else
            {
              success = false;
              free (disk_inode);
              return success;
            }
        }

      num_sectors -= 1;
      i++;

      if (num_sectors == 0)
        {
          all_written = true;
          break;
        } 
    }

  //allocate indirect and/or doubly indirect blocks if needed 
  if (!all_written)
    {
      num_sectors = inode_create_indirect (&disk_inode->indirect, num_sectors);
      if ((int) num_sectors == -1)
        {
          success = false;
          return false;
        }

      if (num_sectors == 0)
        {
          all_written = true;
        }
      
      if (!all_written)
        {
          //need to allocate double indirect as well
          num_sectors = inode_create_db_indirect (&disk_inode->doubly_indirect, num_sectors);
          if ((int )num_sectors == -1)
            {
              success = false;
            }
          else if (num_sectors == 0)
            {
              all_written = true;
            }
          else
            {
              //should never reach here for pintos built in test cases
              all_written = false;
              PANIC ("File is too larege");
            }
        }
    }

  if (all_written)
    {
      success = true;
    }
  return success;
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      
      if (sectors != 0)
        {
          if (allocate_blocks (disk_inode, sectors))
            {
              block_write (fs_device, sector, disk_inode);
              success = true;
            }
       }
      else
        {
          success = true;
        }
    }
  free (disk_inode);
  return success;
}


/* Allocates NUM_SECTORS sectors or INDIRECT_PTRS sectors, whichever comes 
  smaller, in the indirect block. */
static int
inode_create_indirect (block_sector_t *indirect_block, size_t num_sectors)
{
  //this checks if the indirect block has been iniatilized 
  if (! (*indirect_block)) {
    if (!free_map_allocate (1, indirect_block)) {
      return -1;
    }
    block_write (fs_device, *indirect_block, zeroed_sector);
  }

  //read the indirect block to the indirect buffer
  block_sector_t indirect_buff[INDIRECT_PTRS];
  block_read (fs_device, *indirect_block, indirect_buff);

  int i = 0;

  while (i < INDIRECT_PTRS)
    { 
      if (!indirect_buff[i])
        {
          bool suc = free_map_allocate (1, &indirect_buff[i]);
          if (!suc)
          {
            return -1;
          }
        }
      block_write (fs_device, indirect_buff[i], zeroed_sector);

      i++;
      num_sectors--;
      if (num_sectors == 0)
        {
          break;
        }
    }

  block_write (fs_device, *indirect_block, indirect_buff);
  return num_sectors;
}

/* Allocates NUM_SECTORS sectors in the doubly indirect block. */
static int
inode_create_db_indirect (block_sector_t *db_indirect, size_t num_sectors)
{
  //this checks if the indirect block has been iniatilized 
  if (!(*db_indirect)) {
    if (!free_map_allocate (1, db_indirect)) {
      return -1;
    }
    block_write (fs_device, *db_indirect, zeroed_sector);
  }

  block_sector_t db_indirect_buff[INDIRECT_PTRS];
  block_read (fs_device, *db_indirect, db_indirect_buff);

  int i  = 0;
  
  while (i < INDIRECT_PTRS)
    {
      size_t num_of_indirect_sectors = 0;

      if ((int) num_sectors - INDIRECT_PTRS < 0)
        {
          num_of_indirect_sectors = num_sectors % INDIRECT_PTRS;
        }
      else
        {
          num_of_indirect_sectors = INDIRECT_PTRS;
        }

      int num_indirect_alloc = inode_create_indirect (&db_indirect_buff[i],
                                                       num_of_indirect_sectors);

      if (num_indirect_alloc != 0)
        {
          //there is problem so we end
          break;
        }
      
      num_sectors = num_sectors - num_of_indirect_sectors;
      i++;

      if (num_sectors == 0)
          break;
    }
  
    //allocated all sectors
    if (num_sectors == 0)
      {
        block_write (fs_device, *db_indirect, db_indirect_buff);
      }

    return num_sectors;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  lock_init (&inode->inode_lock);
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    lock_acquire (&inode->inode_lock);
    inode->open_cnt++;
    lock_release (&inode->inode_lock);
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  lock_acquire (&inode->inode_lock);
  --inode->open_cnt;
  lock_release (&inode->inode_lock);
  /* Release resources if this was the last opener. */
  if (inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          struct inode_disk on_disk_inode = inode->data;
          int num_of_blocks_left = on_disk_inode.length / BLOCK_SECTOR_SIZE;
          if (on_disk_inode.length % BLOCK_SECTOR_SIZE != 0)
            num_of_blocks_left++;

          int num_of_direct_blocks = (num_of_blocks_left > DIRECT_BLOCKS) 
                                      ? DIRECT_BLOCKS : num_of_blocks_left;

          for (int i = 0; i < num_of_direct_blocks; i++)
            {
              free_map_release (on_disk_inode.direct[i], 1);
            }

          if (num_of_blocks_left == num_of_direct_blocks)
            {
              free (inode); 
              return;
            }

          num_of_blocks_left -= DIRECT_BLOCKS;
          int num_of_indirect_blocks = (num_of_blocks_left > INDIRECT_PTRS) 
                                        ? INDIRECT_PTRS : num_of_blocks_left;
          block_sector_t indirect_blocks[INDIRECT_PTRS];
          block_read (fs_device, on_disk_inode.indirect, indirect_blocks);

          for (int i = 0; i < num_of_indirect_blocks; i++)
            {
              free_map_release (indirect_blocks[i], 1);
            }

          free_map_release (on_disk_inode.indirect, 1);

          if (num_of_blocks_left == num_of_indirect_blocks)
            {
              free (inode); 
              return;
            }

          num_of_blocks_left -= INDIRECT_PTRS;
          int num_of_double_indirect_blocks = num_of_blocks_left/INDIRECT_PTRS;

          if (num_of_blocks_left% INDIRECT_PTRS != 0)
            num_of_double_indirect_blocks++;

          block_sector_t double_indirect_blocks[INDIRECT_PTRS];
          block_read (fs_device, on_disk_inode.doubly_indirect, 
                     double_indirect_blocks);

          for (int i = 0; i < num_of_double_indirect_blocks; i++)
            {
              block_sector_t indirect_blocks2[INDIRECT_PTRS];
              block_read(fs_device, double_indirect_blocks[i], indirect_blocks2);
              int num_of_indirect_blocks2 = (num_of_blocks_left / INDIRECT_PTRS > 0) 
                                            ? INDIRECT_PTRS 
                                            : num_of_blocks_left % INDIRECT_PTRS;
                                            
              for (int j = 0; j < num_of_indirect_blocks2; j++)
                {
                  free_map_release (indirect_blocks2[i], 1);
                }
              free_map_release (double_indirect_blocks[i], 1);
              num_of_blocks_left -= INDIRECT_PTRS;
            }
          free_map_release (inode->sector, 1);
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  lock_acquire (&inode->inode_lock);
  inode->removed = true;
  lock_release (&inode->inode_lock);
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  if (offset + size > inode->data.length)
    {
      lock_acquire (&inode->inode_lock);

      // off_t pos = ROUND_DOWN (inode->data.length, BLOCK_SECTOR_SIZE);
      off_t length = offset + size;
      off_t sectors = bytes_to_sectors (length);

      if (!allocate_blocks (&inode->data, sectors))
      {
        lock_release (&inode->inode_lock);
        return 0;
      }

      inode->data.length = length;
      block_write (fs_device, inode->sector, &inode->data);
      lock_release (&inode->inode_lock);
    }
  
  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  lock_acquire (&inode->inode_lock);
  inode->deny_write_cnt++;
  lock_release (&inode->inode_lock);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  lock_acquire (&inode->inode_lock);
  inode->deny_write_cnt--;
  lock_release (&inode->inode_lock);
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
