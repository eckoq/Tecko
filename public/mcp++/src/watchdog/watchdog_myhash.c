/*
 * watchdog_myhash.h:	My hash table.
 * Date:					2011-02-21
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "watchdog_myhash.h"

// #define __HASH_DEBUG		// Print hash table debug.

#ifdef __HASH_DEBUG

static const	char *name = "myhash";

/*
 * DEBUG_HEAD():		Print debug information head.
 * @name:			Suggest the program name.
 */
#define DEBUG_HEAD(name)  {\
	printf("[%s] ", name); \
	printf("%s{%d} - %s():\n     ", __FILE__, __LINE__, __FUNCTION__); \
}
#endif

/*
 * myhash_get_elem():		Get the element by the id and the idx.
 * @tlb:					The table struct.
 * @id:					Element id.
 * @idx:					Table hash index.
 * Returns:				The element pointer on success. NULL on not found.
 */
static HASHELEM*
myhash_get_elem(myhash_t *tlb, int id, unsigned int idx)
{
	HASHELEM *elem = NULL;	

	for (elem = tlb->elem[idx]; elem != NULL; elem = elem->next) {
		if (elem->id == (unsigned int)id) {
			break;
		}
	}

	return elem;
}

/*
 * myhash_get_idx():	Convert the key to hash index.
 * @tlb:				The table struct.
 * @id:				Caller's element id.
 * Returns:			Return the index include zero.
 */
static inline unsigned int
myhash_get_idx(myhash_t *tlb, int id)
{
	return (unsigned int)(id % tlb->tlb_size);
}

/*
 * myhash_get_idx_call():	Convert the key to hash index.
 * @tlb:					The table struct.
 * @id:					Caller's element id.
 * Returns:				Return the index include zero.
 */
unsigned int
myhash_get_idx_call(myhash_t *tlb, int id)
{
	return myhash_get_idx(tlb, id);
}

/*
 * myhash_init():		Initialize the hash table.
 * @tlb:				Point to the hash table. Must supply by caller.
 * @size:				Index table size of the table.
 * Returns:			0 on success, -1 on error.
 */
int
myhash_init(myhash_t *tlb, unsigned int size)
{
	tlb->elem = (HASHELEM **)malloc(sizeof(HASHELEM *) * size);
	if (!tlb->elem) {
#ifdef __HASH_DEBUG
		DEBUG_HEAD(name);
		printf("Alloc for the hash array fail!\n");
#endif
		return -1;
	}
	memset(tlb->elem, 0, sizeof(HASHELEM *) * size);

	tlb->tlb_size = size;
	tlb->count = 0;

	return 0;
}

/*
 * myhash_destroy():	Clean up the hash table.
 * @tlb:				Point to the hash table.
 * NOTES:			This method only destroy the index array.
 */
void
myhash_destroy(myhash_t *tlb)
{
	free(tlb->elem);
	tlb->elem = NULL;

	tlb->count = 0;
	tlb->tlb_size = 0;
}

/*
 * myhash_add():		Add an element into the hash table.
 * @tlb:				Point to the hash table.
 * @id:				The element id. Will be convert to the hash index.
 * @data:			The element data.
 * @perr:			Store the return error code. Could be NULL.
 * Returns:			0 on success, -1 set the perr to the error code on error.
 */
int
myhash_add(myhash_t *tlb, int id, void *data, int *perr)
{
	int				tmperr;
	unsigned int	idx;
	HASHELEM		*elem = NULL;

	if (!perr) {
		perr = &tmperr;
	}

	elem = (HASHELEM *)malloc(sizeof(HASHELEM));
	if (!elem) {
#ifdef __HASH_DEBUG
		DEBUG_HEAD(name);
		printf("Alloc memory for hash element fail!\n");
#endif
		*perr = HASH_OTHER_ERR;
		return -1;
	}
	
	elem->id = id;
	elem->data = data;
	elem->pre = NULL;
	
	idx = myhash_get_idx(tlb, id);

	if (!tlb->elem[idx]) {
		tlb->elem[idx] = elem;
		elem->next = NULL;
	} else {
		tlb->elem[idx]->pre = elem;
		elem->next = tlb->elem[idx];
		tlb->elem[idx] = elem;		
	}

	tlb->count++;

	return 0;
}

/*
 * myhash_del():		Delete an element from the hash table.
 * @tlb:				Point to the hash table.
 * @id:				The element id. Will be convert to the hash index.
 * @perr:			Store the return error code. Could be NULL.
 * Returns:			0 on success, -1 set the perr to the error code on error.
 * NOTES:			Only free the element node, not include the data.
 */
int
myhash_del(myhash_t *tlb, int id, int *perr)
{
	int				tmperr;
	unsigned int	idx;
	HASHELEM		*elem = NULL;
	
	if (!perr) {
		perr = &tmperr;
	}	

	idx = myhash_get_idx(tlb, id);
	
	elem = myhash_get_elem(tlb, id, idx);
	if (!elem) {
		*perr = HASH_NOT_EXSIT;
		return -1;
	}

	if (elem->pre == NULL) {
		tlb->elem[idx] = elem->next;
		if (elem->next != NULL) {
			elem->next->pre = NULL;
		}
	} else if (elem->next == NULL) {
		elem->pre->next = NULL;
	} else {
		elem->pre->next = elem->next;
		elem->next->pre = elem->pre;
	}

	tlb->count--;
	
	free(elem);

	return 0;
}

/*
 * myhash_find():		Find the element by key.
 * @tlb:				Point to the hash table.
 * @id:				The element id. Will be convert to the hash index.
 * @perr:			Store err code. 0 no error, others error occurs. Set NULL when ignore error.
 * Returns:			Return the pointer to the element data on success. NULL on not found.
 */
void*
myhash_find(myhash_t *tlb, int id, int *perr)
{
	unsigned int	idx;
	HASHELEM		*elem = NULL;
	void			*data = NULL;
	int				tmperr = 0;

	if (!perr) {
		perr = &tmperr;
	}

	idx = myhash_get_idx(tlb, id);
	
	elem = myhash_get_elem(tlb, id, idx);
	if (elem) {
		data = elem->data;
	}
	if (!data) {
		// Should never occur.
		*perr = HASH_OTHER_ERR;
	}

	return data;
}

/*
 * myhash_get_nodelink():		Get the hash table link which hash value is convert by id.
 * @tlb:						Point to the hash table.
 * @id:						ID to convert to hash value.
 * Returns:					The hash node link include NULL.
 */
hash_elem_t*
myhash_get_nodelink(myhash_t *tlb, int id)
{	
	return tlb->elem[myhash_get_idx(tlb, id)];	
}

