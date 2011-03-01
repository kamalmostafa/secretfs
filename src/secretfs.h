/* src/secretfs.h
 *
 * Primary defines for secretfs
 *
 * Copyright 2011 Daniel Silverstone <dsilvers@digital-scurf.org>
 */

#ifndef SECRETFS_H
#define SECRETFS_H

#define FUSE_USE_VERSION 26

#include "fuse.h"
#include "libgfshare.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

/* Initialise the share farm */
extern int init_sharefarm(char *farm);

typedef int (*sharefarm_enumerate_helper_cb_f)(void *, const char *, const struct stat *);

/* Helper to enumerate all shares */
extern int sharefarm_enumerate_helper(sharefarm_enumerate_helper_cb_f cb, void *ctx);

/* Convenient sbuf of the share farm */
extern struct stat farm_sbuf;

/* Tool to stat an entry in the share farm */
extern int sharefarm_stat(const char *path, struct stat *sbuf);

/* Read a share into a RAM buffer */
extern int sharefarm_read(const char *path, size_t *len, char **buf);

#endif /* SECRETFS_H */
