#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "sfs.h"

/*
 * Mount a sfs partition
 */
struct dentry *sfs_mount(struct file_system_type *fs_type,
                              int flags,
                              const char *dev_name,
                              void *data)
{
    struct dentry *dentry =
        mount_bdev(fs_type, flags, dev_name, data, sfs_fill_super);
    if (IS_ERR(dentry))
        pr_err("'%s' mount failure\n", dev_name);
    else
        pr_info("'%s' mount success\n", dev_name);

    return dentry;
}

/*
 * Unmount a sfs partition
 */
void sfs_kill_sb(struct super_block *sb)
{
    kill_block_super(sb);

    pr_info("unmounted disk\n");
}

static struct file_system_type sfs_file_system_type = {
    .owner = THIS_MODULE,
    .name = "sfs",
    .mount = sfs_mount,
    .kill_sb = sfs_kill_sb,
    .fs_flags = FS_REQUIRES_DEV,
    .next = NULL,
};

static int __init sfs_init(void)
{
    int ret = sfs_init_inode_cache();
    if (ret) {
        pr_err("inode cache creation failed\n");
        goto end;
    }

    ret = register_filesystem(&sfs_file_system_type);
    if (ret) {
        pr_err("register_filesystem() failed\n");
        goto end;
    }

    pr_info("module loaded\n");
end:
    return ret;
}

static void __exit sfs_exit(void)
{
    int ret = unregister_filesystem(&sfs_file_system_type);
    if (ret)
        pr_err("unregister_filesystem() failed\n");

    sfs_destroy_inode_cache();

    pr_info("module unloaded\n");
}

module_init(sfs_init);
module_exit(sfs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("James Lau");
MODULE_DESCRIPTION("a simple file system");
