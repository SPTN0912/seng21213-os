#ifndef FS_H
#define FS_H

#include "../include/types.h"

/*
 * Stage 4 - RAM Disk File System
 * Lecture 12
 *
 * Simple flat filesystem with:
 *   - Superblock
 *   - Block bitmap
 *   - Inode bitmap
 *   - Inode table
 *   - Single-block directory
 */

/* Filesystem layout */
#define FS_MAGIC               0x53454E47u
#define FS_BLOCK_SIZE          4096
#define FS_BLOCK_COUNT         256

#define FS_SUPERBLOCK_BLOCK    0
#define FS_DIRECTORY_BLOCK     1
#define FS_BLOCK_BITMAP_BLOCK  2
#define FS_INODE_BITMAP_BLOCK  3
#define FS_INODE_TABLE_BLOCK   4
#define FS_DATA_START_BLOCK    5

#define FS_MAX_INODES          64
#define FS_MAX_OPEN_FILES      16

#define FS_NAME_MAX            28
#define FS_DIRECT_BLOCKS       8
#define FS_MAX_FILE_SIZE       (FS_DIRECT_BLOCKS * FS_BLOCK_SIZE)

#define FS_INVALID_INODE       0xFFFFFFFFu
#define FS_INVALID_BLOCK       0xFFFFFFFFu

/* Open flags */
#define FS_O_RDONLY            0x01
#define FS_O_WRONLY            0x02
#define FS_O_RDWR              0x03
#define FS_O_CREATE            0x04
#define FS_O_APPEND            0x08

/*
 * Filesystem superblock.
 */
typedef struct
{
    uint32_t magic;
    uint32_t block_size;
    uint32_t block_count;
    uint32_t inode_count;
    uint32_t directory_block;
    uint32_t block_bitmap_block;
    uint32_t inode_bitmap_block;
    uint32_t inode_table_block;
    uint32_t data_start_block;
} fs_superblock_t;

/*
 * Inode.
 *
 * A file can use up to 8 direct blocks.
 * 8 × 4096 = 32768 bytes maximum.
 */
typedef struct
{
    uint32_t size;
    uint32_t direct[FS_DIRECT_BLOCKS];
} fs_inode_t;

/*
 * Flat directory entry.
 *
 * Each entry occupies 32 bytes:
 *   name  = 28 bytes
 *   inode = 4 bytes
 */
typedef struct
{
    char name[FS_NAME_MAX];
    uint32_t inode;
} fs_dir_entry_t;

/*
 * Open file descriptor.
 */
typedef struct
{
    uint8_t used;
    uint8_t flags;
    uint16_t reserved;
    uint32_t inode;
    uint32_t offset;
} fs_file_t;

/*
 * Initialize the filesystem.
 */
void fs_init(void);

/*
 * POSIX-style file operations.
 */
int fs_open(const char *name, uint8_t flags);
int fs_read(int fd, void *buffer, uint32_t size);
int fs_write(int fd, const void *buffer, uint32_t size);
int fs_close(int fd);
int fs_unlink(const char *name);

/*
 * Directory listing support.
 *
 * Returns the number of files found and writes their names into
 * the supplied buffer.
 */
int fs_list(char *buffer, uint32_t buffer_size);

#endif /* FS_H */
