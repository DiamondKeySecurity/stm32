#include <stdint.h>
#include <stddef.h>

/* alignment for file allocations */
#define SDFS_ALIGN 16

/* maximum length of file name */
#define SDFS_NAMELEN 32

typedef struct {
  char name[SDFS_NAMELEN];
  uint8_t *loc;
  size_t len;
} filetab_t;

extern int sdfs_init(void);
extern int sdfs_count(void);
extern filetab_t *sdfs_first(void);
extern filetab_t *sdfs_next(void);
extern int sdfs_open(unsigned char *name);
extern int sdfs_write(unsigned char *buf, int len);
extern int sdfs_close(void);
extern int sdfs_cancel(void);
