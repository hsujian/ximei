#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include "lru.h"

typedef struct node_t {
	uint32_t key1;
	uint32_t key2;
	node_t *prev;
	node_t *next;
	node_t *lru_prev;
	node_t *lru_next;
	uint32_t size;
	void *data;
} node_t;

struct lru_t {
	volatile int mutex;
	int hash;
	int size;
	int refs;
	node_t *head;
	node_t *tail;
	node_t *nodes;
	node_t *idle;
	node_t **barrel;
};

static void lru_node_free(node_t *node);

#ifndef NDEBUG
# define DEBUG_LOG(fmt, arg...) printf("<%s(%s:%d)> " fmt, __FUNCTION__, __FILE__, __LINE__, ##arg)
# else
# define DEBUG_LOG(fmt, arg...)
#endif

#define LRU_NODE_REMOVE(head, prev, node) do {\
	if ( ! (node)->prev ) {\
		(head) = (node)->next;\
		if ( (node)->next ) {\
			(node)->next->prev = NULL;\
		}\
	} else {\
		(node)->prev->next = (node)->next;\
		if ( (node)->next ) {\
			(node)->next->prev = (node)->prev;\
		}\
	}\
} while(0)

static int lru_node_move2head(lru_t *lru, node_t *node)
{
	if (!lru || !node || lru->head == node) {
		return 0;
	}
	node->lru_prev->lru_next = node->lru_next;
	if (node->lru_next) {
		node->lru_next->lru_prev = node->lru_prev;
	}
	if (lru->tail == node) {
		lru->tail == node->lru_prev;
	}
	node->lru_prev = NULL;
	node->lru_next = lru->head;
	lru->head->lru_prev = node;
	lru->head = node;
	return 0;
}

void lru_lock(lru_t *lru)
{
	while(lru) {
		if (__sync_bool_compare_and_swap(&lru->mutex, 0, 1))
			return;
	}
}

void lru_unlock(lru_t *lru)
{
	if (lru) {
		lru->mutex = 0;
	}
}

int lru_set(lru_t *lru, node_t *node, int num)
{
	if (!(lru)) return -1;
	int idx = (key1 + key2) % lru->hash;
	node_t *node = lru->barrel[idx];
	while( node ) {
		if (node->key1 != key1 || node->key2 != key2) {
			node = node->next;
			continue;
		}
		if ( ! node->prev ) {
			lru->barrel[idx] = node->next;
			if ( node->next ) {
				node->next->prev = NULL;
			}
		} else {
			node->prev->next = node->next;
			if ( node->next ) {
				node->next->prev = node->prev;
			}
		}
		lru->refs--;
		break;
	}
	return 0;
}

int lru_get(lru_t *lru, lru_find_t *get, int num)
{
	if (!(lru)) return -1;
	int i;
	int idx;
	node_t *n;
	for (i=num; i--;) {
		idx = (get[i].key1 + get[i].key2) % lru->hash;
		n = lru->barrel[idx];
		while( n ) {
			if (n->key1 != get[i].key1 || n->key2 != get[i].key2) {
				n = n->next;
				continue;
			}
			get[i].size = n->size;
			get[i].data = n->data;
			lru_node_move2head(lru, n);
			break;
		}
		get[i].size = 0;
		get[i].data = NULL;
	}
	return 0;
}

int lru_del(lru_t *lru, uint32_t key1, uint32_t key2)
{
	if (!(lru)) return -1;
	int idx = (key1 + key2) % lru->hash;
	node_t *node = lru->barrel[idx];
	while( node ) {
		if (node->key1 != key1 || node->key2 != key2) {
			node = node->next;
			continue;
		}
		LRU_NODE_REMOVE( lru->barrel[idx], node );
		lru_node_free( node );
		lru->refs--;
		break;
	}
	return 0;
}

void lru_node_free(node_t *node)
{
	if (node) {
		if (node->data) {
			free(node->data);
		}
		free(node);
	}
}

void lru_free(lru_t *lru)
{
	if ( lru ) {
		if ( lru->barrel ) {
			free(lru->barrel);
		}
		if ( lru->nodes ) {
			int i;
			for (i=lru->size; i--;) {
				if (lru->nodes[i].data) {
					free(lru->nodes[i].data);
				}
			}
			free(lru->nodes);
		}
		free(lru);
	}
}

lru_t *lru_new(int lru_size, int hash)
{
	if (lru_size < 2) {
		return NULL;
	}
	if (hash < 1) {
		hash = 1024;
	}
	lru_t *_new = (lru_t *)malloc(sizeof(lru_t));
	if ( ! _new ) {
		return NULL;
	}
#ifdef _XDJ_LRU_NEED_MUTEX
	_new->mutex = 0;
#endif
	_new->size = lru_size;
	_new->refs = 0;
	_new->head = NULL;
	_new->tail = NULL;
	_new->hash = hash;
	_new->nodes = (node_t *) calloc(lru_size, sizeof(node_t));
	if ( ! _new->nodes ) {
		lru_free(_new);
		return NULL;
	}
	int i;
	_new->nodes[lru_size - 1].next = NULL;
	for (i=lru_size - 1; i--;) {
		_new->nodes[i].next = _new->nodes[i+1];
	}
	_new->idle = _new->nodes;

	_new->barrel = (node_t **) calloc(hash, sizeof(node_t*));
	if ( ! _new->barrel ) {
		lru_free(_new);
		return NULL;
	}
	return _new;
}

