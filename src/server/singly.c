#include "singly.h"


struct singly_node* singly_init(void){
	
	/* Create "head" singly_node */
	struct singly_node* head = malloc(sizeof(struct singly_node));
	
	head->next = NULL;
	head->data = NULL;
	head->key = 0;
	
	return head;
	
}


struct singly_node* singly_insert(struct singly_node* node, void* data, int key){
	
	struct singly_node* newnode;

	/* Prevent from dereferencing NULL */
	if(!node) return NULL;

	newnode = malloc(sizeof(struct singly_node));

	/* Plug the new singly_node and data into the next one */
	newnode->next = node->next;
	newnode->data = data;
	newnode->key = key;

	/* Plug the current singly_node into the new one */
	node->next = newnode;

	/* Return the new singly_node */
	/* Good for optimizing insertions */
	return newnode;
	
}


void singly_orderedInsert(struct singly_node* node, void* data, int key){
	
	while(node->next){
		
		if(key < node->next->key)
			break;

		node = node->next;
		
	}

	singly_insert(node, data, key);
	
}


struct singly_node* singly_delete(struct singly_node* node, int position){
	
	struct singly_node *prevnode = NULL;

	/* Prevents from dereferencing NULL */
	if(!node || position < 0) return NULL;
	
	/* Traverses the singly_node */
	for(int i = 0; i < position; i++){
		
		if(!node->next) return NULL;
		prevnode = node;
		node = node->next;
		
	}
	
	/* Plugs edges and frees target */
	prevnode->next = node->next;
	node->next = NULL;
	free(node);
	

	/* Return the next singly_node */
	/* Good for optimizing even spaced deletion */
	return prevnode->next;
	
}


int singly_search(struct singly_node* node, int key){

	/* Search the given value, return its index */
	for(int i = 0;; i++){
		
		if(node->key == key)
			return i;

		/* Nothing found, return an invalid index */
		if(!node->next)
			return -1;

		node = node->next;
		
	}
	
}


void singly_destroy(struct singly_node* node){
	
	/* Recursively reach the tail */
	if(node->next)
		singly_destroy(node->next);

	/* Free it backwards */
	free(node->data);
	free(node);
	
}


struct singly_node* singly_pick(struct singly_node* node, int position){
	
	if(position < 0) return NULL;

	for(int i = 0; i < position; i++){
		
		if(!node->next) return NULL;
		node = node->next;
		
	}

	return node;
	
}


int singly_tail(struct singly_node* node){
	
	int i;

	for(i = 0; node->next; i++)
		node = node->next;

	return i;
	
}