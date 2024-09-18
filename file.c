#define pr_fmt(fmt) "simplefs: " fmt

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mpage.h>

#include "bitmap.h"
#include "sfs.h"

/*
 * Map the buffer_head passed in argument with the iblock-th block of the file
 * represented by inode. If the requested block is not allocated and create is
 * true,  allocate a new block on disk and map it.
 */
static int sfs_file_get_block(struct inode *inode,
                                   sector_t iblock,
                                   struct buffer_head *bh_result,
                                   int create)
{
    struct super_block *sb = inode->i_sb;
    struct sfs_sb_info *sbi = SIMPLEFS_SB(sb);
    struct sfs_inode_info *ci = SIMPLEFS_INODE(inode);
    struct sfs_file_index_block *index;
    struct buffer_head *bh_index;
    bool alloc = false;
    int ret = 0, bno;

    pr_info("start to enter sfs_file_get_block\n");
    /* If block number exceeds filesize, fail */
    if (iblock >= SIMPLEFS_BLOCK_SIZE >> 2)
        return -EFBIG;

    /* Read index block from disk */
    bh_index = sb_bread(sb, ci->index_block);
    if (!bh_index)
        return -EIO;
    index = (struct sfs_file_index_block *) bh_index->b_data;

    /*
     * Check if iblock is already allocated. If not and create is true,
     * allocate it. Else, get the physical block number.
     */
    if (index->blocks[iblock] == 0) {
        if (!create)
            return 0;
        bno = get_free_block(sbi);
        if (!bno) {
            ret = -ENOSPC;
            goto brelse_index;
        }
        index->blocks[iblock] = bno;
        alloc = true;
    } else {
        bno = index->blocks[iblock];
    }

    /* Map the physical block to to the given buffer_head */
    map_bh(bh_result, sb, bno);

brelse_index:
    brelse(bh_index);

    return ret;
}

/*
 * Called by the page cache to read a page from the physical disk and map it in
 * memory.
 */
static int sfs_readpage(struct file *file, struct page *page)
{
    pr_info("start to enter sfs_readpage\n");
    return mpage_readpage(page, sfs_file_get_block);
}

/*
 * Called by the page cache to write a dirty page to the physical disk (when
 * sync is called or when memory is needed).
 */
static int sfs_writepage(struct page *page, struct writeback_control *wbc)
{
    pr_info("start to enter sfs_writepage\n");
    return block_write_full_page(page, sfs_file_get_block, wbc);
}

/*
 * Called by the VFS when a write() syscall occurs on file before writing the
 * data in the page cache. This functions checks if the write will be able to
 * complete and allocates the necessary blocks through block_write_begin().
 */
static int sfs_write_begin(struct file *file,
                                struct address_space *mapping,
                                loff_t pos,
                                unsigned int len,
                                unsigned int flags,
                                struct page **pagep,
                                void **fsdata)
{
    struct sfs_sb_info *sbi = SIMPLEFS_SB(file->f_inode->i_sb);
    int err;
    uint32_t nr_allocs = 0;


    pr_info("start to enter sfs_write_begin\n");
    /* Check if the write can be completed (enough space?) */
    if (pos + len > SIMPLEFS_MAX_FILESIZE)
        return -ENOSPC;
    nr_allocs = max(pos + len, file->f_inode->i_size) / SIMPLEFS_BLOCK_SIZE;
    if (nr_allocs > file->f_inode->i_blocks - 1)
        nr_allocs -= file->f_inode->i_blocks - 1;
    else
        nr_allocs = 0;
    if (nr_allocs > sbi->nr_free_blocks)
        return -ENOSPC;

    /* prepare the write */
    err = block_write_begin(mapping, pos, len, flags, pagep,
                            sfs_file_get_block);
    /* if this failed, reclaim newly allocated blocks */
    if (err < 0)
        pr_err("newly allocated blocks reclaim not implemented yet\n");
    return err;
}

/*
 * Called by the VFS after writing data from a write() syscall to the page
 * cache. This functions updates inode metadata and truncates the file if
 * necessary.
 */
static int sfs_write_end(struct file *file,
                              struct address_space *mapping,
                              loff_t pos,
                              unsigned int len,
                              unsigned int copied,
                              struct page *page,
                              void *fsdata)
{
    struct inode *inode = file->f_inode;
    struct sfs_inode_info *ci = SIMPLEFS_INODE(inode);
    struct super_block *sb = inode->i_sb;
    uint32_t nr_blocks_old;


    /* Complete the write() */
    int ret = generic_write_end(file, mapping, pos, len, copied, page, fsdata);
    pr_info("start to enter sfs_write_end\n");
    if (ret < len) {
        pr_err("wrote less than requested.");
        return ret;
    }

    nr_blocks_old = inode->i_blocks;

    /* Update inode metadata */
    inode->i_blocks = inode->i_size / SIMPLEFS_BLOCK_SIZE + 2;
    inode->i_mtime = inode->i_ctime = current_time(inode);
    mark_inode_dirty(inode);

    /* If file is smaller than before, free unused blocks */
    if (nr_blocks_old > inode->i_blocks) {
        int i;
        struct buffer_head *bh_index;
        struct sfs_file_index_block *index;

        /* Free unused blocks from page cache */
        truncate_pagecache(inode, inode->i_size);

        /* Read index block to remove unused blocks */
        bh_index = sb_bread(sb, ci->index_block);
        if (!bh_index) {
            pr_err("failed truncating '%s'. we just lost %lu blocks\n",
                   file->f_path.dentry->d_name.name,
                   nr_blocks_old - inode->i_blocks);
            goto end;
        }
        index = (struct sfs_file_index_block *) bh_index->b_data;

        for (i = inode->i_blocks - 1; i < nr_blocks_old - 1; i++) {
            put_block(SIMPLEFS_SB(sb), index->blocks[i]);
            index->blocks[i] = 0;
        }
        mark_buffer_dirty(bh_index);
        brelse(bh_index);
    }
end:
    return ret;
}

const struct address_space_operations sfs_aops = {
    .readpage = sfs_readpage,
    .writepage = sfs_writepage,
    .write_begin = sfs_write_begin,
    .write_end = sfs_write_end};

const struct file_operations sfs_file_ops = {
    .llseek = generic_file_llseek,
    .owner = THIS_MODULE,
    .read_iter = generic_file_read_iter,
    .write_iter = generic_file_write_iter};
