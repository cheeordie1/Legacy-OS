#ifndef CACHE_H
#define CACHE_H
#include <debug.h>
#include <hash.h>
#include "devices/block.h"
#include "threads/synch.h"
#include "filesys/filesys.h"

enum sector_type
  {
    FILE_DATA = 1,       /* Cached fileblock represents file data. */
    INODE_DATA = 2,      /* Cached fileblock represents inode data. */
    INODE_METADATA = 3   /* Cached fileblock represents inode metadata. */
  };

struct cache_block
  {
    struct hash_elem elem;          /* Element in a hash by sector. */
    block_sector_t sector;          /* Sector number to hash with. */
    char accessed;                  /* For reads or writes. */
    bool removed;                   /* Allocated or not. */
    bool dirty;                     /* Recent write access. */
    enum sector_type type;          /* Type of fileblock for eviction. */
    struct shared_lock spot_lock;   /* Shared lock for eviction/fetching. */
    char data[BLOCK_SECTOR_SIZE];   /* Fileblock data. */
  };

struct lock GENGAR;

void cache_init (void);
struct cache_block *cache_find_sector (block_sector_t);
struct cache_block *cache_fetch (size_t pos, block_sector_t, enum sector_type);
void cache_write (struct cache_block *, const void *, off_t, size_t);
void cache_delete (struct cache_block *);
void cache_flush (void *aux UNUSED);

struct cache_block *cache_pin (block_sector_t, enum sector_type);
void cache_unpin (struct cache_block *);

#endif /* filesys/cache.h */
