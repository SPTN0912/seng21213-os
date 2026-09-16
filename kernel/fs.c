#include "fs.h"
#include "ramdisk.h"

/*
 * Stage 4 - RAM Disk File System
 * Lecture 12
 *
 * Filesystem layout:
 *
 *   Block 0  -> Superblock
 *   Block 1  -> Flat directory
 *   Block 2  -> Block bitmap
 *   Block 3  -> Inode bitmap
 *   Block 4  -> Inode table
 *   Block 5+ -> File data
 */

/* ---------------------------------------------------------------------------
 * Internal filesystem helpers
 * -------------------------------------------------------------------------*/

/*
 * Get pointers to the filesystem metadata stored in the RAM disk.
 */
static fs_superblock_t *fs_superblock(void)
{
    return (fs_superblock_t *)ramdisk_get_block(FS_SUPERBLOCK_BLOCK);
}

static fs_dir_entry_t *fs_directory(void)
{
    return (fs_dir_entry_t *)ramdisk_get_block(FS_DIRECTORY_BLOCK);
}

static uint8_t *fs_block_bitmap(void)
{
    return ramdisk_get_block(FS_BLOCK_BITMAP_BLOCK);
}

static uint8_t *fs_inode_bitmap(void)
{
    return ramdisk_get_block(FS_INODE_BITMAP_BLOCK);
}

static fs_inode_t *fs_inode_table(void)
{
    return (fs_inode_t *)ramdisk_get_block(FS_INODE_TABLE_BLOCK);
}

/*
 * Open file table.
 */
static fs_file_t open_files[FS_MAX_OPEN_FILES];

/*
 * Simple string length function.
 */
static uint32_t fs_strlen(const char *str)
{
    uint32_t length = 0;

    if (str == NULL)
    {
        return 0;
    }

    while (str[length] != '\0')
    {
        length++;
    }

    return length;
}

/*
 * Compare two strings.
 *
 * Returns 0 when equal.
 */
static int fs_strcmp(const char *a, const char *b)
{
    uint32_t i = 0;

    if (a == NULL || b == NULL)
    {
        return -1;
    }

    while (a[i] != '\0' && b[i] != '\0')
    {
        if (a[i] != b[i])
        {
            return (int)((uint8_t)a[i] - (uint8_t)b[i]);
        }

        i++;
    }

    return (int)((uint8_t)a[i] - (uint8_t)b[i]);
}

/*
 * Copy a string into a fixed-size filesystem name.
 */
static void fs_copy_name(char *destination, const char *source)
{
    uint32_t i;

    for (i = 0; i < FS_NAME_MAX; i++)
    {
        destination[i] = '\0';
    }

    for (i = 0; i < FS_NAME_MAX - 1 && source[i] != '\0'; i++)
    {
        destination[i] = source[i];
    }
}

/*
 * Check whether a filename is valid.
 */
static int fs_valid_name(const char *name)
{
    uint32_t length;

    if (name == NULL)
    {
        return 0;
    }

    length = fs_strlen(name);

    if (length == 0 || length >= FS_NAME_MAX)
    {
        return 0;
    }

    return 1;
}

/* ---------------------------------------------------------------------------
 * Bitmap helpers
 * -------------------------------------------------------------------------*/

/*
 * Check whether a bit is set.
 */
static int bitmap_test(uint8_t *bitmap, uint32_t index)
{
    return (bitmap[index / 8] & (uint8_t)(1u << (index % 8))) != 0;
}

/*
 * Set a bitmap bit.
 */
static void bitmap_set(uint8_t *bitmap, uint32_t index)
{
    bitmap[index / 8] |= (uint8_t)(1u << (index % 8));
}

/*
 * Clear a bitmap bit.
 */
static void bitmap_clear(uint8_t *bitmap, uint32_t index)
{
    bitmap[index / 8] &= (uint8_t)~(1u << (index % 8));
}

/*
 * Find a free block.
 *
 * Only data blocks are allocated to files.
 */
static int fs_allocate_block(void)
{
    uint8_t *bitmap = fs_block_bitmap();
    uint32_t block;

    for (block = FS_DATA_START_BLOCK;
         block < FS_BLOCK_COUNT;
         block++)
    {
        if (!bitmap_test(bitmap, block))
        {
            bitmap_set(bitmap, block);
            return (int)block;
        }
    }

    return -1;
}

/*
 * Free a data block.
 */
static void fs_free_block(uint32_t block)
{
    if (block < FS_BLOCK_COUNT &&
        block >= FS_DATA_START_BLOCK)
    {
        bitmap_clear(fs_block_bitmap(), block);
    }
}

/*
 * Find a free inode.
 */
static int fs_allocate_inode(void)
{
    uint8_t *bitmap = fs_inode_bitmap();
    uint32_t inode;

    for (inode = 0; inode < FS_MAX_INODES; inode++)
    {
        if (!bitmap_test(bitmap, inode))
        {
            bitmap_set(bitmap, inode);
            return (int)inode;
        }
    }

    return -1;
}

/*
 * Free an inode.
 */
static void fs_free_inode(uint32_t inode)
{
    if (inode < FS_MAX_INODES)
    {
        bitmap_clear(fs_inode_bitmap(), inode);
    }
}

/* ---------------------------------------------------------------------------
 * Directory helpers
 * -------------------------------------------------------------------------*/

/*
 * Find a directory entry by filename.
 */
static int fs_find_directory_entry(const char *name)
{
    fs_dir_entry_t *directory = fs_directory();
    uint32_t entries_per_block;
    uint32_t i;

    entries_per_block = FS_BLOCK_SIZE / sizeof(fs_dir_entry_t);

    for (i = 0; i < entries_per_block; i++)
    {
        if (directory[i].inode != FS_INVALID_INODE &&
            fs_strcmp(directory[i].name, name) == 0)
        {
            return (int)i;
        }
    }

    return -1;
}

/*
 * Find an empty directory entry.
 */
static int fs_find_free_directory_entry(void)
{
    fs_dir_entry_t *directory = fs_directory();
    uint32_t entries_per_block;
    uint32_t i;

    entries_per_block = FS_BLOCK_SIZE / sizeof(fs_dir_entry_t);

    for (i = 0; i < entries_per_block; i++)
    {
        if (directory[i].inode == FS_INVALID_INODE)
        {
            return (int)i;
        }
    }

    return -1;
}

/* ---------------------------------------------------------------------------
 * Filesystem initialization
 * -------------------------------------------------------------------------*/

void fs_init(void)
{
    fs_superblock_t *superblock;
    fs_dir_entry_t *directory;
    fs_inode_t *inode_table;
    uint8_t *block_bitmap;
    uint8_t *inode_bitmap;

    uint32_t directory_entries;
    uint32_t i;

    /*
     * Initialize the underlying 1 MB RAM disk.
     */
    ramdisk_init();

    superblock = fs_superblock();
    directory = fs_directory();
    inode_table = fs_inode_table();
    block_bitmap = fs_block_bitmap();
    inode_bitmap = fs_inode_bitmap();

    /*
     * Initialize superblock.
     */
    superblock->magic = FS_MAGIC;
    superblock->block_size = FS_BLOCK_SIZE;
    superblock->block_count = FS_BLOCK_COUNT;
    superblock->inode_count = FS_MAX_INODES;
    superblock->directory_block = FS_DIRECTORY_BLOCK;
    superblock->block_bitmap_block = FS_BLOCK_BITMAP_BLOCK;
    superblock->inode_bitmap_block = FS_INODE_BITMAP_BLOCK;
    superblock->inode_table_block = FS_INODE_TABLE_BLOCK;
    superblock->data_start_block = FS_DATA_START_BLOCK;

    /*
     * Clear bitmaps.
     */
    for (i = 0; i < FS_BLOCK_SIZE; i++)
    {
        block_bitmap[i] = 0;
        inode_bitmap[i] = 0;
    }

    /*
     * Reserve metadata blocks 0 through 4.
     */
    for (i = 0; i < FS_DATA_START_BLOCK; i++)
    {
        bitmap_set(block_bitmap, i);
    }

    /*
     * Mark every directory entry as unused.
     */
    directory_entries = FS_BLOCK_SIZE / sizeof(fs_dir_entry_t);

    for (i = 0; i < directory_entries; i++)
    {
        directory[i].inode = FS_INVALID_INODE;
        directory[i].name[0] = '\0';
    }

    /*
     * Clear inode table.
     */
    for (i = 0; i < FS_MAX_INODES; i++)
    {
        inode_table[i].size = 0;

        {
            uint32_t j;

            for (j = 0; j < FS_DIRECT_BLOCKS; j++)
            {
                inode_table[i].direct[j] = FS_INVALID_BLOCK;
            }
        }
    }

    /*
     * Clear the open file table.
     */
    for (i = 0; i < FS_MAX_OPEN_FILES; i++)
    {
        open_files[i].used = 0;
        open_files[i].flags = 0;
        open_files[i].reserved = 0;
        open_files[i].inode = FS_INVALID_INODE;
        open_files[i].offset = 0;
    }
}

/* ---------------------------------------------------------------------------
 * Open
 * -------------------------------------------------------------------------*/

int fs_open(const char *name, uint8_t flags)
{
    int directory_index;
    int inode_index;
    uint32_t i;

    if (!fs_valid_name(name))
    {
        return -1;
    }

    /*
     * Look for an existing file.
     */
    directory_index = fs_find_directory_entry(name);

    /*
     * Create the file when requested.
     */
    if (directory_index < 0 && (flags & FS_O_CREATE))
    {
        directory_index = fs_find_free_directory_entry();

        if (directory_index < 0)
        {
            return -1;
        }

        inode_index = fs_allocate_inode();

        if (inode_index < 0)
        {
            return -1;
        }

        fs_directory()[directory_index].inode = (uint32_t)inode_index;
        fs_copy_name(fs_directory()[directory_index].name, name);

        fs_inode_table()[inode_index].size = 0;

        for (i = 0; i < FS_DIRECT_BLOCKS; i++)
        {
            fs_inode_table()[inode_index].direct[i] = FS_INVALID_BLOCK;
        }
    }

    /*
     * File doesn't exist and CREATE wasn't requested.
     */
    if (directory_index < 0)
    {
        return -1;
    }

    inode_index = (int)fs_directory()[directory_index].inode;

    /*
     * Find an unused file descriptor.
     */
    for (i = 0; i < FS_MAX_OPEN_FILES; i++)
    {
        if (!open_files[i].used)
        {
            open_files[i].used = 1;
            open_files[i].flags = flags;
            open_files[i].reserved = 0;
            open_files[i].inode = (uint32_t)inode_index;

            if (flags & FS_O_APPEND)
            {
                open_files[i].offset =
                    fs_inode_table()[inode_index].size;
            }
            else
            {
                open_files[i].offset = 0;
            }

            return (int)i;
        }
    }

    return -1;
}

/* ---------------------------------------------------------------------------
 * Read
 * -------------------------------------------------------------------------*/

int fs_read(int fd, void *buffer, uint32_t size)
{
    fs_file_t *file;
    fs_inode_t *inode;
    uint8_t *destination;
    uint32_t remaining;
    uint32_t total_read;

    if (fd < 0 || fd >= FS_MAX_OPEN_FILES ||
        buffer == NULL)
    {
        return -1;
    }

    file = &open_files[fd];

    if (!file->used)
    {
        return -1;
    }

    /*
     * A write-only descriptor cannot be read.
     */
    if ((file->flags & 0x03) == FS_O_WRONLY)
    {
        return -1;
    }

    inode = &fs_inode_table()[file->inode];

    if (file->offset >= inode->size)
    {
        return 0;
    }

    remaining = inode->size - file->offset;

    if (size > remaining)
    {
        size = remaining;
    }

    destination = (uint8_t *)buffer;
    total_read = 0;

    while (total_read < size)
    {
        uint32_t absolute_offset;
        uint32_t block_index;
        uint32_t block_offset;
        uint32_t bytes_to_copy;
        uint8_t *source;

        absolute_offset = file->offset + total_read;
        block_index = absolute_offset / FS_BLOCK_SIZE;
        block_offset = absolute_offset % FS_BLOCK_SIZE;

        if (block_index >= FS_DIRECT_BLOCKS)
        {
            break;
        }

        if (inode->direct[block_index] == FS_INVALID_BLOCK)
        {
            break;
        }

        source = ramdisk_get_block(inode->direct[block_index]);

        bytes_to_copy = FS_BLOCK_SIZE - block_offset;

        if (bytes_to_copy > size - total_read)
        {
            bytes_to_copy = size - total_read;
        }

        {
            uint32_t i;

            for (i = 0; i < bytes_to_copy; i++)
            {
                destination[total_read + i] =
                    source[block_offset + i];
            }
        }

        total_read += bytes_to_copy;
    }

    file->offset += total_read;

    return (int)total_read;
}

/* ---------------------------------------------------------------------------
 * Write
 * -------------------------------------------------------------------------*/

int fs_write(int fd, const void *buffer, uint32_t size)
{
    fs_file_t *file;
    fs_inode_t *inode;
    const uint8_t *source;
    uint32_t total_written;

    if (fd < 0 || fd >= FS_MAX_OPEN_FILES ||
        buffer == NULL)
    {
        return -1;
    }

    file = &open_files[fd];

    if (!file->used)
    {
        return -1;
    }

    /*
     * A read-only descriptor cannot be written.
     */
    if ((file->flags & 0x03) == FS_O_RDONLY)
    {
        return -1;
    }

    inode = &fs_inode_table()[file->inode];

    /*
     * Append mode always starts at the current end of file.
     */
    if (file->flags & FS_O_APPEND)
    {
        file->offset = inode->size;
    }

    /*
     * Do not exceed the 32 KB maximum file size.
     */
    if (file->offset >= FS_MAX_FILE_SIZE)
    {
        return 0;
    }

    if (size > FS_MAX_FILE_SIZE - file->offset)
    {
        size = FS_MAX_FILE_SIZE - file->offset;
    }

    source = (const uint8_t *)buffer;
    total_written = 0;

    while (total_written < size)
    {
        uint32_t absolute_offset;
        uint32_t block_index;
        uint32_t block_offset;
        uint32_t bytes_to_copy;
        uint32_t block_number;
        uint8_t *destination;

        absolute_offset = file->offset + total_written;
        block_index = absolute_offset / FS_BLOCK_SIZE;
        block_offset = absolute_offset % FS_BLOCK_SIZE;

        if (block_index >= FS_DIRECT_BLOCKS)
        {
            break;
        }

        /*
         * Allocate a data block when this part of the file
         * does not have one yet.
         */
        if (inode->direct[block_index] == FS_INVALID_BLOCK)
        {
            int new_block = fs_allocate_block();

            if (new_block < 0)
            {
                break;
            }

            inode->direct[block_index] = (uint32_t)new_block;

            /*
             * Clear the newly allocated block.
             */
            destination = ramdisk_get_block((uint32_t)new_block);

            {
                uint32_t i;

                for (i = 0; i < FS_BLOCK_SIZE; i++)
                {
                    destination[i] = 0;
                }
            }
        }

        block_number = inode->direct[block_index];

        destination = ramdisk_get_block(block_number);

        bytes_to_copy = FS_BLOCK_SIZE - block_offset;

        if (bytes_to_copy > size - total_written)
        {
            bytes_to_copy = size - total_written;
        }

        {
            uint32_t i;

            for (i = 0; i < bytes_to_copy; i++)
            {
                destination[block_offset + i] =
                    source[total_written + i];
            }
        }

        total_written += bytes_to_copy;
    }

    file->offset += total_written;

    if (file->offset > inode->size)
    {
        inode->size = file->offset;
    }

    return (int)total_written;
}

/* ---------------------------------------------------------------------------
 * Close
 * -------------------------------------------------------------------------*/

int fs_close(int fd)
{
    if (fd < 0 || fd >= FS_MAX_OPEN_FILES)
    {
        return -1;
    }

    if (!open_files[fd].used)
    {
        return -1;
    }

    open_files[fd].used = 0;
    open_files[fd].flags = 0;
    open_files[fd].reserved = 0;
    open_files[fd].inode = FS_INVALID_INODE;
    open_files[fd].offset = 0;

    return 0;
}

/* ---------------------------------------------------------------------------
 * Unlink
 * -------------------------------------------------------------------------*/

int fs_unlink(const char *name)
{
    int directory_index;
    uint32_t inode_index;
    fs_inode_t *inode;
    uint32_t i;

    if (!fs_valid_name(name))
    {
        return -1;
    }

    directory_index = fs_find_directory_entry(name);

    if (directory_index < 0)
    {
        return -1;
    }

    inode_index = fs_directory()[directory_index].inode;

    /*
     * Do not remove a file while it is open.
     */
    for (i = 0; i < FS_MAX_OPEN_FILES; i++)
    {
        if (open_files[i].used &&
            open_files[i].inode == inode_index)
        {
            return -1;
        }
    }

    inode = &fs_inode_table()[inode_index];

    /*
     * Release all data blocks belonging to the file.
     */
    for (i = 0; i < FS_DIRECT_BLOCKS; i++)
    {
        if (inode->direct[i] != FS_INVALID_BLOCK)
        {
            fs_free_block(inode->direct[i]);
            inode->direct[i] = FS_INVALID_BLOCK;
        }
    }

    inode->size = 0;

    /*
     * Release the inode.
     */
    fs_free_inode(inode_index);

    /*
     * Remove the directory entry.
     */
    fs_directory()[directory_index].inode = FS_INVALID_INODE;
    fs_directory()[directory_index].name[0] = '\0';

    return 0;
}

/* ---------------------------------------------------------------------------
 * Directory listing
 * -------------------------------------------------------------------------*/

int fs_list(char *buffer, uint32_t buffer_size)
{
    fs_dir_entry_t *directory = fs_directory();
    uint32_t entries_per_block;
    uint32_t i;
    uint32_t position = 0;
    int count = 0;

    if (buffer == NULL && buffer_size > 0)
    {
        return -1;
    }

    entries_per_block = FS_BLOCK_SIZE / sizeof(fs_dir_entry_t);

    for (i = 0; i < entries_per_block; i++)
    {
        if (directory[i].inode != FS_INVALID_INODE)
        {
            uint32_t name_length = fs_strlen(directory[i].name);
            uint32_t j;

            count++;

            if (buffer != NULL &&
                position + name_length + 1 < buffer_size)
            {
                for (j = 0; j < name_length; j++)
                {
                    buffer[position++] = directory[i].name[j];
                }

                buffer[position++] = '\n';
                buffer[position] = '\0';
            }
        }
    }

    if (buffer != NULL && buffer_size > 0)
    {
        buffer[position < buffer_size ? position : buffer_size - 1] =
            '\0';
    }

    return count;
}
