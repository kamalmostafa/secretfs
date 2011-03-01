/* src/sharefarm.c
 *
 * The GF Share farm (listing it, etc)
 *
 * Copyright 2011 Daniel Silverstone <dsilvers@digital-scurf.org>
 *
 */

#include "secretfs.h"

#include <sys/mman.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdbool.h>


static char *share_farm = NULL;
struct stat farm_sbuf;

int
init_sharefarm(char *farm)
{
  DIR *d;
  
  if (stat(farm, &farm_sbuf) < 0)
    return -1;
  
  if (!S_ISDIR(farm_sbuf.st_mode)) {
    errno = ENOTDIR;
    return -1;
  }
  
  if ((d = opendir(farm)) == NULL)
    return -1;
  
  closedir(d);
  
  share_farm = farm;
  return 0;
}

typedef struct {
  const char *fname;
  int sharenum;
  bool present;
} sharefarm_content_shares;

typedef struct sharefarm_content_s {
  struct sharefarm_content_s *next;
  const char *stem;
  struct stat statinfo;
  int sharecount;
  sharefarm_content_shares *shares;
} sharefarm_content;

static void
free_content(sharefarm_content *c)
{
  sharefarm_content *next_c;
  while (c != NULL) {
    next_c = c->next;
    free(c->shares);
    free(c);
    c = next_c;
  }
}

static sharefarm_content *
find_content(sharefarm_content *base, const char *name)
{
  while (base != NULL) {
    if (strcmp(base->stem, name) == 0)
      return base;
    base = base->next;
  }
  return NULL;
}

static int
find_sharenum(const char *fname, char **stem, int *sharenum)
{
  char *dot;
  
  dot = strrchr(fname, '.');
  if (dot == NULL) {
    errno = EINVAL;
    return -1;
  }
  
  if (dot != (fname + strlen(fname) - 4)) {
    errno = EINVAL;
    return -1;
  }
  
  if (dot < (fname + 3)) {
    errno = EINVAL;
    return -1;
  }
  
  *sharenum = atoi(dot+1);
  if (*sharenum < 1 || *sharenum > 255) {
    errno = EINVAL;
    return -1;
  }
  
  *stem = strdup(fname);
  if (*stem == NULL) {
    errno = ENOMEM;
    return -1;
  }
  
  (*stem)[dot - fname] = '\0';
  
  return 0;
}

static void
update_share_mode(sharefarm_content *c)
{
  int i;
  
  for (i = 0; i < c->sharecount; ++i) {
    if (c->shares[i].present == false) {
      c->statinfo.st_mode &= ~(0777);
    }
  }
  c->statinfo.st_mode &= ~(0377);
}

static sharefarm_content *
inject_share(sharefarm_content *tail, const char *fname)
{
  char *stem;
  int sharenum, i;
  sharefarm_content *c;
  struct stat sbuf;
  
  if (find_sharenum(fname, &stem, &sharenum) < 0) {
    return tail;
  }
  
  i = stat(fname, &sbuf);
  
  if ((i < 0) && errno != ENOENT) {
    free(stem);
    return tail;
  }
  
  printf("stat(%s) == %d (errno == %d)\n", fname, i, errno);
  
  c = find_content(tail, stem);
  
  if (c == NULL) {
    c = calloc(1, sizeof(*c));
    c->next = tail;
    c->stem = stem;
    c->statinfo.st_mode = S_IFREG;
    if (i == 0)
      memcpy(&c->statinfo, &sbuf, sizeof(sbuf));
    c->sharecount = 1;
    c->shares = calloc(1, sizeof(*c->shares));
    c->shares[0].fname = strdup(fname);
    c->shares[0].sharenum = sharenum;
    c->shares[0].present = (i == 0);
    update_share_mode(c);
    return c;
  }
  
  if (i == 0)
    memcpy(&c->statinfo, &sbuf, sizeof(sbuf));
  
  c->shares = realloc(c->shares, sizeof(*c->shares) * (c->sharecount + 1));
  c->shares[c->sharecount].fname = strdup(fname);
  c->shares[c->sharecount].sharenum = sharenum;
  c->shares[c->sharecount].present = (i == 0);
  c->sharecount++;
  free(stem);
  
  update_share_mode(c);
  
  return tail;
}

static int
find_all_shares(sharefarm_content **ct)
{
  DIR *d;
  int ret = 0;
  int i;
  struct dirent *dire = NULL, *direp;
  sharefarm_content *c = NULL;
  
  chdir(share_farm);
  
  d = opendir(".");
  
  if (d == NULL)
    return -ENOENT;
  
  dire = malloc(sizeof(struct dirent) + PATH_MAX + 1);
  
  while ((i = readdir_r(d, dire, &direp)) == 0) {
    if (direp == NULL)
      break;
    if (dire->d_name[0] == '.')
      continue;
    c = inject_share(c, dire->d_name);
  }
  
  if (i != 0) ret = -i;
  
  *ct = c;
  
  out:
  if (dire != NULL)
    free(dire);
  closedir(d);
  return ret;
}

int
sharefarm_enumerate_helper(sharefarm_enumerate_helper_cb_f cb, void *ctx)
{
  sharefarm_content *c = NULL;
  sharefarm_content *cc;
  int ret = 0;
  
  if ((ret = find_all_shares(&c)) < 0)
    return ret;
  
  for (cc = c; cc != NULL; cc = cc->next) {
    cb(ctx, cc->stem, &cc->statinfo);
  }
  
  free_content(c);
  
  return 0;
}

int
sharefarm_stat(const char *path, struct stat *sbuf)
{
  sharefarm_content *c = NULL, *cc = NULL;
  int ret = 0;
  
  if ((ret = find_all_shares(&c)) < 0)
    return ret;
  
  cc = find_content(c, path);
  
  if (cc == NULL) {
    free_content(c);
    return -ENOENT;
  }
  
  memcpy(sbuf, &cc->statinfo, sizeof(*sbuf));
  
  free_content(c);
  
  return 0;
}

int
sharefarm_read(const char *path, size_t *len, char **buf)
{
  sharefarm_content *c = NULL, *cc = NULL;
  int ret, i;
  gfshare_ctx *gfc = NULL;
  unsigned char *sharenrs = NULL;
  unsigned char *sparebuf = NULL;
  
  if ((ret = find_all_shares(&c)) < 0)
    return ret;
  
  cc = find_content(c, path);
  
  if (cc == NULL) {
    free_content(c);
    return -ENOENT;
  }
  
  sharenrs = malloc(cc->sharecount);
  if (sharenrs == NULL) {
    free_content(c);
    return -ENOMEM;
  }
  
  for (i = 0; i < cc->sharecount; ++i)
    sharenrs[i] = (unsigned char)(cc->shares[i].sharenum);
  
  *len = cc->statinfo.st_size;
  *buf = calloc(1, *len);
  if (*buf == NULL) {
    ret = -ENOMEM;
    goto out;
  }
  
  mlock(*buf, *len);
  
  sparebuf = malloc(*len);
  if (sparebuf == NULL) {
    ret = -ENOMEM;
    goto out;
  }
  
  gfc = gfshare_ctx_init_dec(sharenrs, cc->sharecount, cc->statinfo.st_size);
  
  if (gfc == NULL) {
    ret = -ENOMEM;
    goto out;
  }
  
  for (i = 0; i < cc->sharecount; ++i) {
    int fd = open(cc->shares[i].fname, O_RDONLY);
    
    if (fd == -1) {
      ret = -errno;
      goto out;
    }
    
    if (read(fd, sparebuf, *len) != *len) {
      ret = -errno;
      close(fd);
      goto out;
    }
    
    close(fd);
    
    gfshare_ctx_dec_giveshare(gfc, i, sparebuf);
  }
  
  gfshare_ctx_dec_extract(gfc, (unsigned char *)*buf);

  out: 
  if (ret < 0 ) {
    if (*buf != NULL) {
      munlock(*buf, *len);
      free(*buf);
    }
  }
  if (gfc != NULL) {
    gfshare_ctx_free(gfc);
  }
  free(sharenrs);
  free_content(c);
  return ret;
}
