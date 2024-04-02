#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "list.h"
typedef int (*list_cmp_func_t)(void *, struct list_head *, struct list_head *);

void timsort(void *priv, struct list_head *head, list_cmp_func_t cmp);