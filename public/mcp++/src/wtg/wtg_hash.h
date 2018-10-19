/*
 * wtg_hash.h:			Hash table.
 * Date:					2011-4-08
 */

#ifndef __WTG_HASH_H
#define __WTG_HASH_H

typedef struct wtg_hash_elem{
	unsigned int			id;			// The element id.
	void					*data;		// Point to the data.
	struct wtg_hash_elem	*pre;		// Previous element node the same hash.
	struct wtg_hash_elem	*next;		// Next element node the same hash.
} WTG_HASHELEM;

typedef struct {
	unsigned int		tlb_size;	// The index table size.
	unsigned int		count;		// Element count in the table.
	WTG_HASHELEM		**elem;		// The element array.
} WTG_MYHASH;

typedef WTG_MYHASH		wtg_hash_t;
typedef WTG_HASHELEM	wtg_hash_elem_t;

/*
 * wtg_hash_init():		Initialize the hash table.
 * @tlb:					Point to the hash table. Must supply by caller.
 * @size:					Index table size of the table.
 * Returns:				0 on success, -1 on error.
 */
extern int
wtg_hash_init(wtg_hash_t *tlb, unsigned int size);

/*
 * wtg_hash_destroy():		Clean up the hash table.
 * @tlb:					Point to the hash table.
 * NOTES:				This method only destroy the index array.
 */
extern void
wtg_hash_destroy(wtg_hash_t *tlb);

/*
 * The error code set by wtg_hash_add() and wtg_hash_del().
 */
#define HASH_NOT_EXSIT		0xf010
#define HASH_OTHER_ERR		0xf020

/*
 * wtg_hash_add():	Add an element into the hash table.
 * @tlb:				Point to the hash table.
 * @id:				The element id. Will be convert to the hash index.
 * @data:			The element data.
 * @perr:			Store the return error code.
 * Returns:			0 on success, -1 set the perr to the error code on error.
 */
extern int
wtg_hash_add(wtg_hash_t *tlb, int id, void *data, int *perr);

/*
 * wtg_hash_del():	Delete an element from the hash table.
 * @tlb:				Point to the hash table.
 * @id:				The element id. Will be convert to the hash index.
 * @perr:			Store the return error code.
 * Returns:			0 on success, -1 set the perr to the error code on error.
 */
extern int
wtg_hash_del(wtg_hash_t *tlb, int id, int *perr);

/*
 * wtg_hash_find():	Find the element by key.
 * @tlb:				Point to the hash table.
 * @id:				The element id. Will be convert to the hash index.
 * @perr:			Store err code. 0 no error, others error occurs. Set NULL when ignore error.
 * Returns:			Return the pointer to the element data on success. NULL on not found.
 */
extern void*
wtg_hash_find(wtg_hash_t *tlb, int id, int *perr);

/*
 * wtg_hash_get_nodelink():	Get the hash table link which hash value is convert by id.
 * @tlb:						Point to the hash table.
 * @id:						ID to convert to hash value.
 * Returns:					The hash node link include NULL.
 */
extern wtg_hash_elem_t*
wtg_hash_get_nodelink(wtg_hash_t *tlb, int id);

/*
 * wtg_hash_get_idx_call():	Convert the key to hash index.
 * @tlb:					The table struct.
 * @id:					Caller's element id.
 * Returns:				Return the index include zero.
 */
extern unsigned int
wtg_hash_get_idx_call(wtg_hash_t *tlb, int id);

#endif

