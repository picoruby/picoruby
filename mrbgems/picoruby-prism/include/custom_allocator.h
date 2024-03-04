#ifndef PRISM_CUSTOM_ALLOCATOR_H
#define PRISM_CUSTOM_ALLOCATOR_H

void *picorb_alloc(unsigned int size);
void *picorb_calloc(unsigned int nmemb, unsigned int size);
void *picorb_realloc(void *ptr, unsigned int size);
void  picorb_free(void *ptr);

#define xmalloc      picorb_alloc
#define xcalloc      picorb_calloc
#define xrealloc     picorb_realloc
#define xfree        picorb_free

#endif

