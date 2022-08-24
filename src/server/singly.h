#ifndef __SINGLY__
#define __SINGLY__


#include <stdlib.h>


/* Static initializer for a singly_node, usually for global ones */
#define SINGLY_NODE_INITIALIZER {0, 0, 0}


/* List structure */
struct singly_node {
	
	struct singly_node* next;
	void* data;
	int key;
	
};


/* Initializes structures */
struct singly_node* singly_init(void);

/* Inserts a value at the next singly_node */
struct singly_node* singly_insert(struct singly_node* node, void* data, int key);

/* Inserts in ascending order */
void singly_orderedInsert(struct singly_node* node, void* data, int key);

/* Searchs for a value in the list */
int singly_search(struct singly_node* node, int key);

/* Deletes the next singly_node */
struct singly_node* singly_delete(struct singly_node* node, int position);

/* Picks a singly_node by position */
struct singly_node* singly_pick(struct singly_node* node, int position);

/* Finds the tail of the list */
int singly_tail(struct singly_node* node);

/* Frees all memory (not data though, that's user's responsability) */
void singly_destroy(struct singly_node* node);


#endif