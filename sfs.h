#ifndef SIMPLEFS_H
#define SIMPLEFS_H

/* source: https://en.wikipedia.org/wiki/Hexspeak */
#define SIMPLEFS_MAGIC 0xDEADCELL

#define SIMPLEFS_SB_BLOCK_NR 0

#define SIMPLEFS_BLOCK_SIZE (1 << 12)   /* 4 KiB */
#define SIMPLEFS_MAX_FILESIZE (1 << 22) /* 4 MiB */
#define SIMPLEFS_FILENAME_LEN 28
#define SIMPLEFS_MAX_SUBFILES 128

/*
 * sfs partition layout
 * +---------------+
 * |  superblock   |  1 block
 * +---------------+
 * |  inode store  |  sb->nr_istore_blocks blocks
 * +---------------+
 * | ifree bitmap  |  sb->nr_ifree_blocks blocks
 * +---------------+
 * | bfree bitmap  |  sb->nr_bfree_blocks blocks
 * +---------------+
 * |    data       |
 * |      blocks   |  rest of the blocks
 * +---------------+
 */

struct sfs_inode {
    uint32_t i_mode;      /* File mode */
    uint32_t i_uid;       /* Owner id */
    uint32_t i_gid;       /* Group id */
    uint32_t i_size;      /* Size in bytes */
    uint32_t i_ctime;     /* Inode change time */
    uint32_t i_atime;     /* Access time */
    uint32_t i_mtime;     /* Modification time */
    uint32_t i_blocks;    /* Block count */
    uint32_t i_nlink;     /* Hard links count */
    uint32_t index_block; /* Block with list of blocks for this file */
};

#define SIMPLEFS_INODES_PER_BLOCK \
    (SIMPLEFS_BLOCK_SIZE / sizeof(struct sfs_inode))

struct sfs_sb_info {
    uint32_t magic; /* Magic number */

    uint32_t nr_blocks; /* Total number of blocks (incl sb & inodes) */
    uint32_t nr_inodes; /* Total number of inodes */

    uint32_t nr_istore_blocks; /* Number of inode store blocks */
    uint32_t nr_ifree_blocks;  /* Number of inode free bitmap blocks */
    uint32_t nr_bfree_blocks;  /* Number of block free bitmap blocks */

    uint32_t nr_free_inodes; /* Number of free inodes */
    uint32_t nr_free_blocks; /* Number of free blocks */

#ifdef __KERNEL__
    unsigned long *ifree_bitmap; /* In-memory free inodes bitmap */
    unsigned long *bfree_bitmap; /* In-memory free blocks bitmap */
#endif
};

#ifdef __KERNEL__

struct sfs_inode_info {
    uint32_t index_block;
    struct inode vfs_inode;
};

struct sfs_file_index_block {
    uint32_t blocks[SIMPLEFS_BLOCK_SIZE >> 2];
};

struct sfs_dir_block {
    struct sfs_file {
        uint32_t inode;
        char filename[SIMPLEFS_FILENAME_LEN];
    } files[SIMPLEFS_MAX_SUBFILES];
};

/* superblock functions */
int sfs_fill_super(struct super_block *sb, void *data, int silent);

/* inode functions */
int sfs_init_inode_cache(void);
void sfs_destroy_inode_cache(void);
struct inode *sfs_iget(struct super_block *sb, unsigned long ino);

/* file functions */
extern const struct file_operations sfs_file_ops;
extern const struct file_operations sfs_dir_ops;
extern const struct address_space_operations sfs_aops;

/* Getters for superbock and inode */
#define SIMPLEFS_SB(sb) (sb->s_fs_info)
#define SIMPLEFS_INODE(inode) \
    (container_of(inode, struct sfs_inode_info, vfs_inode))

#endif /* __KERNEL__ */

#endif /* SIMPLEFS_H */
