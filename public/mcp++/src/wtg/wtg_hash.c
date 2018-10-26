/*
 * wtg_hash.c:			Hash table.
 * Date:					2011-4-08
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wtg_hash.h"

// #define __HASH_DEBUG		// Print hash table debug.

#ifdef __HASH_DEBUG

static const	char *name = "wtg_hash";

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
 * wtg_hash_get_elem():		Get the element by the id and the idx.
 * @tlb:					The table struct.
 * @id:					Element id.
 * @idx:					Table hash index.
 * Returns:				The element pointer on success. NULL on not found.
 */
static WTG_HASHELEM*
wtg_hash_get_elem(wtg_hash_t *tlb, int id, unsigned int idx)
{
	WTG_HASHELEM *elem = NULL;	

	for (elem = tlb->elem[idx]; elem != NULL; elem = elem->next) {
		if (elem->id == (unsigned int)id) {
			break;
		}
	}

	return elem;
}

/*
 * wtg_hash_get_idx():	Convert the key to hash index.
 * @tlb:				The table struct.
 * @id:				Caller's element id.
 * Returns:			Return the index include zero.
 */
static inline unsigned int
wtg_hash_get_idx(wtg_hash_t *tlb, int id)
{
	return (unsigned int)(id % tlb->tlb_size);
}

/*
 * wtg_hash_get_idx_call():	Convert the key to hash index.
 * @tlb:					The table struct.
 * @id:					Caller's element id.
 * Returns:				Return the index include zero.
 */
unsigned int
wtg_hash_get_idx_call(wtg_hash_t *tlb, int id)
{
	return wtg_hash_get_idx(tlb, id);
}

/*
 * wtg_hash_init():	Initialize the hash table.
 * @tlb:				Point to the hash table. Must supply by caller.
 * @size:				Index table size of the table.
 * Returns:			0 on success, -1 on error.
 */
int
wtg_hash_init(wtg_hash_t *tlb, unsigned int size)
{
	tlb->elem = (WTG_HASHELEM **)malloc(sizeof(WTG_HASHELEM *) * size);
	if (!tlb->elem) {
#ifdef __HASH_DEBUG
		DEBUG_HEAD(name);
		printf("Alloc for the hash array fail!\n");
#endif
		return -1;
	}
	memset(tlb->elem, 0, sizeof(WTG_HASHELEM *) * size);

	tlb->tlb_size = size;
	tlb->count = 0;

	return 0;
}

/*
 * wtg_hash_destroy():	Clean up the hash table.
 * @tlb:				Point to the hash table.
 * NOTES:			This method only destroy the index array.
 */
void
wtg_hash_destroy(wtg_hash_t *tlb)
{
	if ( tlb->elem ) {
		free(tlb->elem);
		tlb->elem = NULL;
	}

	tlb->count = 0;
	tlb->tlb_size = 0;
}

/*
 * wtg_hash_add():	Add an element into the hash table.
 * @tlb:				Point to the hash table.
 * @id:				The element id. Will be convert to the hash index.
 * @data:			The element data.
 * @perr:			Store the return error code. Could be NULL.
 * Returns:			0 on success, -1 set the perr to the error code on error.
 */
int
wtg_hash_add(wtg_hash_t *tlb, int id, void *data, int *perr)
{
	int				tmperr;
	unsigned int	idx;
	WTG_HASHELEM	*elem = NULL;

	if (!perr) {
		perr = &tmperr;
	}

	elem = (WTG_HASHELEM *)malloc(sizeof(WTG_HASHELEM));
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
	
	idx = wtg_hash_get_idx(tlb, id);

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
 * wtg_hash_del():		Delete an element from the hash table.
 * @tlb:				Point to the hash table.
 * @id:				The element id. Will be convert to the hash index.
 * @perr:			Store the return error code. Could be NULL.
 * Returns:			0 on success, -1 set the perr to the error code on error.
 * NOTES:			Only free the element node, not include the data.
 */
int
wtg_hash_del(wtg_hash_t *tlb, int id, int *perr)
{
	int				tmperr;
	unsigned int	idx;
	WTG_HASHELEM	*elem = NULL;
	
	if (!perr) {
		perr = &tmperr;
	}	

	idx = wtg_hash_get_idx(tlb, id);
	
	elem = wtg_hash_get_elem(tlb, id, idx);
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
 * wtg_hash_find():		Find the element by key.
 * @tlb:				Point to the hash table.
 * @id:				The element id. Will be convert to the hash index.
 * @perr:			Store err code. 0 no error, others error occurs. Set NULL when ignore error.
 * Returns:			Return the pointer to the element data on success. NULL on not found.
 */
void*
wtg_hash_find(wtg_hash_t *tlb, int id, int *perr)
{
	unsigned int	idx;
	WTG_HASHELEM	*elem = NULL;
	void			*data = NULL;
	int				tmperr = 0;

	if (!perr) {
		perr = &tmperr;
	}

	idx = wtg_hash_get_idx(tlb, id);
	
	elem = wtg_hash_get_elem(tlb, id, idx);
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
 * wtg_hash_get_nodelink():		Get the hash table link which hash value is convert by id.
 * @tlb:						Point to the hash table.
 * @id:						ID to convert to hash value.
 * Returns:					The hash node link include NULL.
 */
wtg_hash_elem_t*
wtg_hash_get_nodelink(wtg_hash_t *tlb, int id)
{	
	return tlb->elem[wtg_hash_get_idx(tlb, id)];	
}

