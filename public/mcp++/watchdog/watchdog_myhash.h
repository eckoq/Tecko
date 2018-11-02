/*
 * watchdog_myhash.h:	My hash table.
 * Date:					2011-02-21
 */

#ifndef __WATCHDOG_MYHASH_H
#define __WATCHDOG_MYHASH_H

typedef struct hash_elem{
	unsigned int		id;			// The element id.
	void				*data;		// Point to the data.
	struct hash_elem	*pre;		// Previous element node the same hash.
	struct hash_elem	*next;		// Next element node the same hash.
} HASHELEM;

typedef struct {
	unsigned int		tlb_size;	// The index table size.
	unsigned int		count;		// Element count in the table.
	HASHELEM			**elem;		// The element array.
} MYHASH;

typedef MYHASH		myhash_t;
typedef HASHELEM	hash_elem_t;

/*
 * myhash_init():		Initialize the hash table.
 * @tlb:				Point to the hash table. Must supply by caller.
 * @size:				Index table size of the table.
 * Returns:			0 on success, -1 on error.
 */
int
myhash_init(myhash_t *tlb, unsigned int size);

/*
 * myhash_destroy():	Clean up the hash table.
 * @tlb:				Point to the hash table.
 * NOTES:			This method only destroy the index array.
 */
void
myhash_destroy(myhash_t *tlb);

/*
 * The error code set by myhash_add() and myhash_del().
 */
#define HASH_NOT_EXSIT		0xf010
#define HASH_OTHER_ERR		0xf020

/*
 * myhash_add():		Add an element into the hash table.
 * @tlb:				Point to the hash table.
 * @id:				The element id. Will be convert to the hash index.
 * @data:			The element data.
 * @perr:			Store the return error code.
 * Returns:			0 on success, -1 set the perr to the error code on error.
 */
int
myhash_add(myhash_t *tlb, int id, void *data, int *perr);

/*
 * myhash_del():		Delete an element from the hash table.
 * @tlb:				Point to the hash table.
 * @id:				The element id. Will be convert to the hash index.
 * @perr:			Store the return error code.
 * Returns:			0 on success, -1 set the perr to the error code on error.
 */
int
myhash_del(myhash_t *tlb, int id, int *perr);

/*
 * myhash_find():		Find the element by key.
 * @tlb:				Point to the hash table.
 * @id:				The element id. Will be convert to the hash index.
 * @perr:			Store err code. 0 no error, others error occurs. Set NULL when ignore error.
 * Returns:			Return the pointer to the element data on success. NULL on not found.
 */
void*
myhash_find(myhash_t *tlb, int id, int *perr);

/*
 * myhash_get_nodelink():		Get the hash table link which hash value is convert by id.
 * @tlb:						Point to the hash table.
 * @id:						ID to convert to hash value.
 * Returns:					The hash node link include NULL.
 */
hash_elem_t*
myhash_get_nodelink(myhash_t *tlb, int id);

/*
 * myhash_get_idx_call():	Convert the key to hash index.
 * @tlb:					The table struct.
 * @id:					Caller's element id.
 * Returns:				Return the index include zero.
 */
unsigned int
myhash_get_idx_call(myhash_t *tlb, int id);

#endif

