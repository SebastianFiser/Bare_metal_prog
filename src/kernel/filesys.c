#include "filesys.h"

#define MAX_FILES 16

struct file {
    char name[32];
    char data[512];
    int size;
    int used;
};

static struct file files[MAX_FILES];