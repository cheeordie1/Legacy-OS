#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"

#define INODE_ERROR -1

/* First sector of non-inode data. */
static block_sector_t file_block_start;
 
static struct lock GENGAR; //:')
static struct free_map inode_map;
 
void inode_init (void);
bool inode_create (block_sector_t, off_t, bool);
struct inode *inode_open (block_sector_t);
struct inode *inode_reopen (struct inode *);
block_sector_t inode_byte_to_block_idx (struct inode *inode, off_t pos);
block_sector_t inode_sector_from_inumber (block_sector_t inumber);
block_sector_t inode_lookup (struct inode *, size_t block_idx);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);

#endif /* filesys/inode.h */
