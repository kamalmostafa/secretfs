/* src/secretfs.c
 *
 * SecretFS - core functionality
 *
 * Copyright 2011 Daniel Silverstone <dsilvers@digital-scurf.org>
 */

#include "secretfs.h"

#include <stdio.h>

#include <errno.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

static int
secretfs_getattr(const char *path, struct stat *sbuf)
{
  if (strcmp(path, "/") == 0) {
    memcpy(sbuf, &farm_sbuf, sizeof(*sbuf));
    return 0;
  }
  return sharefarm_stat(path + 1, sbuf);
}

#define FH_PTR_GET(ffi) (void*)(ffi->fh)
#define FH_PTR_SET(ffi,p) ffi->fh = (uint64_t)(p)

typedef struct {
  size_t len;
  char *buf;
} secretfs_fh;

static int
secretfs_open(const char *path, struct fuse_file_info *ffi)
{
  secretfs_fh *fh = calloc(1, sizeof(*fh));
  int ret;
  
  if (fh == NULL)
    return -ENOMEM;
  
  if ((ret = sharefarm_read(path + 1, &fh->len, &fh->buf)) < 0) {
    free(fh);
    return ret;
  }
  
  FH_PTR_SET(ffi, fh);
  
  return 0;
}

static int
secretfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *ffi)
{
  secretfs_fh *fh = FH_PTR_GET(ffi);
  if (fh == NULL) {
    return -EINVAL;
  }
  
  if ((offset + size) > fh->len) {
    size = fh->len - offset;
  }
  
  if (size == 0)
    return 0;
  
  memcpy(buf, fh->buf + offset, size);
  
  return size;
}

static int
secretfs_release(const char *path, struct fuse_file_info *ffi)
{
  secretfs_fh *fh = FH_PTR_GET(ffi);
  if (fh != NULL) {
    munlock(fh->buf, fh->len);
    free(fh->buf);
    free(fh);
    FH_PTR_SET(ffi, NULL);
  }
  return 0;
}

static int
secretfs_opendir(const char *path, struct fuse_file_info *ffi)
{
  if (strcmp(path, "/") != 0)
    return -ENOENT;
  
  return 0;
}

typedef struct { fuse_fill_dir_t filler; void *buf; } secretfs_readdir_cb_ctx;

static int
secretfs_readdir_cb(void *_ctx, const char *name, const struct stat *stbuf)
{
  secretfs_readdir_cb_ctx *ctx = _ctx;
  return ctx->filler(ctx->buf, name, stbuf, 0);
}

static int
secretfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *ffi)
{
  secretfs_readdir_cb_ctx ctx = { filler = filler, buf = buf };
  
  filler(buf, ".", &farm_sbuf, 0);
  filler(buf, "..", &farm_sbuf, 0);
  
  return sharefarm_enumerate_helper(secretfs_readdir_cb, &ctx);
}

static int
secretfs_releasedir(const char *path, struct fuse_file_info *ffi)
{
  return 0;
}

static int
secretfs_statfs(const char *path, struct statvfs *svbuf)
{
  svbuf->f_bsize = 1024;    /* optimal transfer block size */
  svbuf->f_blocks = 1024;   /* total data blocks in file system */
  svbuf->f_bfree = 1;    /* free blocks in fs */
  svbuf->f_bavail = 1;   /* free blocks avail to non-superuser */
  svbuf->f_files = 1024;    /* total file nodes in file system */
  svbuf->f_ffree = 1023;    /* free file nodes in fs */
  svbuf->f_fsid = 0;     /* file system id */
  return 0;
}

static struct fuse_operations secretfs_op = {
  .getattr = secretfs_getattr,
  .readlink = NULL,
  .getdir = NULL,
  .mknod = NULL,
  .mkdir = NULL,
  .unlink = NULL,
  .rmdir = NULL,
  .symlink = NULL,
  .rename = NULL,
  .link = NULL,
  .chmod = NULL,
  .chown = NULL,
  .truncate = NULL,
  .utime = NULL,
  .open = secretfs_open,
  .read = secretfs_read,
  .write = NULL,
  .statfs = secretfs_statfs,
  .flush = NULL,
  .release = secretfs_release,
  .fsync = NULL,
  .setxattr = NULL,
  .getxattr = NULL,
  .listxattr = NULL,
  .removexattr = NULL,
  .opendir = secretfs_opendir,
  .readdir = secretfs_readdir,
  .releasedir = secretfs_releasedir,
  .fsyncdir = NULL,
  .init = NULL,
  .destroy = NULL,
  .access = NULL,
  .create = NULL,
  .ftruncate = NULL,
  .fgetattr = NULL,
  .lock = NULL,
  .utimens = NULL,
  .bmap = NULL,
  .flag_nullpath_ok = 0,
  .flag_reserved = 0,
  .ioctl = NULL,
  .poll = NULL,
};

int
main(int argc, char **argv)
{
  int i;
  char *ptmp;
  
  if (sizeof(uint64_t) < sizeof(void*))
    return 10;
  
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <sharefarm> <mountpoint> [<fuse mount options>]\n", argv[0]);
    return 1;
  }
  
  ptmp = realpath(argv[1], NULL);
  
  if (ptmp == NULL) {
    perror("realpath");
    fprintf(stderr, "Unable to translate %s into a full path\n", argv[1]);
    return 1;
  }
  
  argv[1] = ptmp;
  
  ptmp = realpath(argv[2], NULL);
  
  if (ptmp == NULL) {
    perror("realpath");
    fprintf(stderr, "Unble to translate %s into a full path\n", argv[2]);
    return 1;
  }
  
  argv[2] = ptmp;
  
  if ((i = init_sharefarm(argv[1])) < 0) {
    perror("init_sharefarm()");
    return 1;
  }
  
  printf("SecretFS mounting %s onto %s...\n", argv[1], argv[2]);
  
  argv[1] = argv[0];
  
  --argc; ++argv;
  
  return fuse_main(argc, argv, &secretfs_op, NULL);
}
