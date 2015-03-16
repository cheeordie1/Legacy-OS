#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/cache.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include "filesys/directory.h"

#define PERCENT_INODES 1 / 100

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  file_block_start = block_size (fs_device) * PERCENT_INODES;
  inode_init ();
  free_map_init (&inode_map, file_block_start, INODE_MAP_INODE);
  free_map_init (&fs_map, block_size (fs_device), FREE_MAP_INODE);
  
  if (format) 
    do_format ();

  free_map_open (&inode_map);
  free_map_open (&fs_map);
  free_map_set_multiple (&inode_map, 0, RESERVED_INODES);
  free_map_set_multiple (&fs_map, 0, file_block_start);
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  cache_flush (NULL);	
  free_map_close (&fs_map);
  free_map_close (&inode_map);
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  block_sector_t inode_sector = INODE_ERROR;
  struct dir *dir = dir_open_root ();
  bool success = (dir != NULL
                  && free_map_allocate (&inode_map, RESERVED_INODES,
                                        1, &inode_sector)
                  && inode_create (inode_sector, initial_size, false)
                  && dir_add (dir, name, inode_sector));
  if (!success && inode_sector != INODE_ERROR) 
    free_map_release (&inode_map, inode_sector, 1);
  dir_close (dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise. Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  struct dir *dir = dir_open_root ();
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, name, &inode);
  dir_close (dir);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir = dir_open_root ();
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create (&inode_map);
  free_map_create (&fs_map);
  if (!dir_create (ROOT_DIR_INODE, 16))
    PANIC ("root directory creation failed");
  free_map_close (&inode_map);
  free_map_close (&fs_map);
  printf ("done.\n");
}
