#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define INODE_ERROR -1

#define DIRECT_SECTORS 8
#define DOUBIND_SECTOR 7
#define BLOCKNUMS_PER_IND 256
#define INODES_PER_SECTOR 16
#define INODE_SIZE 32
#define PERCENT_INODES 1 / 100

/* On-disk inode.
   Must be INODE_SIZE bytes long. */
struct inode
  {
    struct list_elem elem;               /* Element in inode list. */
    off_t length;                        /* File size in bytes. */
    block_sector_t i_sectors[8];         /* Sector numbers of disk locations. */
    int open_cnt;                        /* Number of openers. */
    bool removed;                        /* True if deleted, false otherwise. */
    bool dir;                            /* True if directory, false otherwise. */
    bool large;                          /* True if large block addressing.  */
    int deny_write_cnt;                  /* 0: writes ok, >0: deny writes. */
    unsigned magic;                      /* Magic number. */
    char unused[8];                      /* Unused bytes to align to 32 byte inode. */
  };

static block_sector_t small_lookup (struct inode *inode, int block_idx);
static block_sector_t large_lookup (struct inode *inode, int block_idx);
static block_sector_t lookup_in_sector (block_sector_t file_data_sector,
                                        int sector_ofs);

/* Returns the inode-relative block device sector that contains byte
   offset POS within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
block_sector_t
inode_byte_to_block_idx (struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->length)
    return pos / BLOCK_SECTOR_SIZE;
  else
    return INODE_ERROR;
}

/* Searches the inode for the index of the fileblock referred to by
   the inode's blockNum'th file block number in its file block number
   array. The function runs a direct index lookup on small files that 
   use 7 or less file blocks and indirect index lookups on large files
   that have more than 7 file blocks. Returns INODE_ERROR on error. */
block_sector_t 
inode_lookup (struct inode *inode, int block_idx) 
{
  if (inode->large) 
    return large_lookup (inode, sector);
  else 
    return small_lookup (inode, sector); 
}

/* Runs inode fileblock lookup algorithm for small files. */
static block_sector_t 
small_lookup (struct inode *inode, int block_idx)
{
  ASSERT (block_idx >= 0 && block_idx < DIRECT_SECTORS);
  block_sector_t file_data_sector = inode->i_sectors[block_idx];
  if (file_data_sector < 0) 
    return INODE_ERROR;
  return file_data_sector; 
}

/* Runs inode fileblock lookup algorithm for large files. */
static block_sector_t
large_lookup (struct inode *inode, int block_idx)
{
  int indirect_idx = block_idx / BLOCKNUMS_PER_IND;
  int indirect_ofs;
  block_sector_t file_data_sector; 
  if (indirect_idx < DOUBIND_SECTOR)
    {
      sector_ofs = block_idx % BLOCKNUMS_PER_IND;
      file_data_sector = lookup_in_sector (fs, inode->i_addr[indirect_idx], indirect_ofs);
    }
  else
    {
      indirect_ofs = (indirect_idx - DOUBIND_SECTOR) / BLOCKNUMS_PER_IND;
      int dindirect_ofs = (indirect_idx - DOUBIND_SECTOR) % BLOCKNUMS_PER_IND;
      block_sector_t dindirect_idx = lookup_in_sector (fs, inode->i_addr[7], indirect_ofs);
      file_data_sector = lookup_in_sector (fs, dindirect_idx, dindirect_ofs);
    }
  return file_data_sector;
}

/* Searches for a sector in an indirect sector. */
static block_sector_t
lookup_in_sector (block_sector_t file_data_sector, int sector_ofs)
{
  uint16_t *cached_sector;
  if ((cached_sector = cache_find_sector (file_data_sector)) != NULL)
    return cached_sector[sector_ofs];
  struct cache_block *cache_block = cache_insert (file_data_sector, INODE_METADATA);
  if (cache_clock == NULL)
    return INODE_ERROR;
  return cached_block->data[sector_ofs];
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

static struct free_map inode_map; 
static struct lock inode_lock;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
  lock_init (&inode_lock);
  file_block_start = block_size (fs_device) * PERCENT_INODES;
  free_map_init (&inode_map, file_block_start, INODE_MAP_INODE);
  free_map_open (&inode_map);
  ASSERT (free_map_set_multiple (&inode_map, 0, RESERVED_INODES));
}

/* Allocate the next available inode. 
   Return INODE_ERROR if no inodes available. */
block_sector_t
inode_next_free ()
{
  block_sector_t sector;
  if (free_map_allocate (&inode_map, RESERVED_INODES, 1, &sector))
    return sector;
  else
    return INODE_ERROR;
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, bool is_dir)
{
  struct inode new_inode;
  bool success = false;

  ASSERT (length >= 0);
  
  ASSERT (sizeof new_inode == INODE_SIZE);
  new_inode->removed = false;
  new_inode->dir = is_dir;
  new_inode->large = false;
  new_inode->magic = INODE_MAGIC;
  if (free_map_allocate (inode_sectors, &disk_inode->start)) 
    {
      block_write (fs_device, sector, disk_inode);
      if (sectors > 0) 
        {
          static char zeros[BLOCK_SECTOR_SIZE];
          size_t i;
              
          for (i = 0; i < sectors; i++) 
            block_write (fs_device, disk_inode->start + i, zeros);
        }
      success = true; 
    } 
  return success;
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
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
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

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          free_map_release (inode->data.start,
                            bytes_to_sectors (inode->data.length)); 
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
  inode->removed = true;
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
  inode->deny_write_cnt++;
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
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->length;
}
