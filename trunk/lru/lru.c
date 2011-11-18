#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <malloc.h>
#include <assert.h>

#include "lru.h"

typedef struct node_t {
	uint32_t key1;
	uint32_t key2;
	struct node_t *hash_next;
	struct node_t *lru_prev;
	struct node_t *lru_next;
	uint32_t size;
	void *data;
} node_t;

typedef struct recycle_t {
	struct recycle_t *next;
} recycle_t;

struct lru_t {
	volatile int mutex;
	int hash;
	int size;
	uint32_t release_data_slot;
	uint32_t total_free_size;
	node_t *lru_head;
	node_t *lru_tail;
	node_t *nodes;
	node_t *idle;
	recycle_t *data_recycle;
	node_t **barrel;
	/* data_free_fn data_free_fn; */
};

#ifndef NDEBUG
# define DEBUG_LOG(fmt, arg...) printf("<%s(%s:%d)> " fmt, __FUNCTION__, __FILE__, __LINE__, ##arg)
# else
# define DEBUG_LOG(fmt, arg...)
#endif

#define HASH_PNEXT(node) (node)->hash_next
#define HASH_NNEXT(node) (node).hash_next
#define IDLE_PNEXT(node) (node)->hash_next

static inline void lru_node_free(lru_t *lru, node_t *node);
static inline node_t *lru_node_new(lru_t *lru);

static void lru_data_free(lru_t *lru, char *data);
static void *lru_data_new(lru_t *lru, size_t size);

static inline void lru_node_move2idle(lru_t *lru, node_t *node, int barrel_idx, node_t *hash_prev)
{
	assert(lru != NULL);
	assert(node != NULL);
	assert(barrel_idx > -1);
	//barrel;
	if (lru->barrel[barrel_idx] == node) {
		lru->barrel[barrel_idx] = HASH_PNEXT (node);
	} else {
		assert(hash_prev != NULL);
		HASH_PNEXT( hash_prev ) = HASH_PNEXT (node);
	}
	//lru queue
	if ( ! node->lru_prev ) {
		lru->lru_head = node->lru_next;
		lru->lru_head->lru_prev = NULL;
		if (lru->lru_tail == node) {
			lru->lru_tail = NULL;
		}
	} else {
		node->lru_prev->lru_next = node->lru_next;
		if ( node->lru_next ) {
			node->lru_next->lru_prev = node->lru_prev;
		} else {
			lru->lru_tail = node->lru_prev;
		}
	}
	lru_node_free(lru, node);
}

static inline void lru_node_move2lru_head(lru_t *lru, node_t *node)
{
	assert(lru != NULL);
	assert(node != NULL);
	if (lru->lru_head == node) {
		return;
	}
	node->lru_prev->lru_next = node->lru_next;
	if (node->lru_next) {
		node->lru_next->lru_prev = node->lru_prev;
	} else {
		lru->lru_tail = node->lru_prev;
	}
	node->lru_prev = NULL;
	node->lru_next = lru->lru_head;
	lru->lru_head->lru_prev = node;
	lru->lru_head = node;
	return;
}

static inline void lru_lock(lru_t *lru)
{
	assert(lru != NULL);
	if (lru) {
		while (__sync_lock_test_and_set(&lru->mutex, 1)) {
			sched_yield();
		}
	}
}

static inline void lru_unlock(lru_t *lru)
{
	assert(lru != NULL);
	if (lru) {
		__sync_synchronize ();
		__sync_lock_release (&lru->mutex);
	}
}

int lru_set(lru_t *lru, lru_find_t *node, int num)
{
	if (!lru) return -1;
	lru_lock(lru);
	int i, idx, find;
	node_t *seek;
	for (i=0; i<num; i++) {
		idx = (node[i].key1 + node[i].key2) % lru->hash;
		seek = lru->barrel[idx];
		find = 0;
		while( seek ) {
			if (seek->key1 != node[i].key1 || seek->key2 != node[i].key2) {
				seek = HASH_PNEXT (seek);
				continue;
			}
			find = 1;
			size_t usable_size = malloc_usable_size(seek->data);
			if (usable_size >= node[i].size) {
				if (seek->data) {
					memcpy(seek->data, node[i].data, node[i].size);
				}
			} else {
				void *data = lru_data_new(lru, node[i].size);
				seek->data = data;
				if (data) {
					memcpy(data, node[i].data, node[i].size);
				}
			}
			seek->size = node[i].size;
			lru_node_move2lru_head(lru, seek);
			break;
		}
		if (find == 0) {
			node_t *new_node = lru_node_new(lru);
			void *data = lru_data_new(lru, node[i].size);

			new_node->key1 = node[i].key1;
			new_node->key2 = node[i].key2;
			new_node->data = data;
			new_node->size = node[i].size;
			if (data) {
				memcpy(data, node[i].data, node[i].size);
			}

			HASH_PNEXT (new_node) = lru->barrel[idx];
			lru->barrel[idx] = new_node;
			
			new_node->lru_prev = NULL;
			new_node->lru_next = lru->lru_head;
			if (lru->lru_head) {
				lru->lru_head->lru_prev = new_node;
			}
			lru->lru_head = new_node;
			if (! lru->lru_tail ) {
				lru->lru_tail = lru->lru_head;
			}
		}
	}
	lru_unlock(lru);
	return 0;
}

int lru_get(lru_t *lru, lru_find_t *get, int num)
{
	if (!(lru)) return -1;
	lru_lock(lru);
	int i,
		idx,
		gnum = 0;
	node_t *n;
	for (i=num; i--;) {
		idx = (get[i].key1 + get[i].key2) % lru->hash;
		n = lru->barrel[idx];
		get[i].size = 0;
		get[i].data = NULL;
		while( n ) {
			if (n->key1 != get[i].key1 || n->key2 != get[i].key2) {
				n = HASH_PNEXT (n);
				continue;
			}
			get[i].size = n->size;
			get[i].data = n->data;
			lru_node_move2lru_head(lru, n);
			gnum++;
			break;
		}
	}
	lru_unlock(lru);
	return gnum;
}

int lru_del(lru_t *lru, uint32_t key1, uint32_t key2)
{
	if (!lru) return -1;
	lru_lock(lru);
	int idx = (key1 + key2) % lru->hash;
	node_t *node = lru->barrel[idx];
	node_t *prev_node = NULL;
	while( node ) {
		if (node->key1 != key1 || node->key2 != key2) {
			prev_node = node;
			node = HASH_PNEXT (node);
			continue;
		}
		lru_node_move2idle(lru, node, idx, prev_node);
		break;
	}
	lru_unlock(lru);
	return 0;
}

static inline node_t *lru_node_new(lru_t *lru)
{
	assert(lru != NULL);
	if ( lru->idle ) {
		node_t *node = lru->idle;
		lru->idle = IDLE_PNEXT (lru->idle);
		return node;
	}

	node_t *node = lru->lru_tail;
	assert(node != NULL);
	lru->lru_tail = lru->lru_tail->lru_prev;
	assert(lru->lru_tail != NULL);
	lru->lru_tail->lru_next = NULL;
	return node;
}

static inline void lru_node_free(lru_t *lru, node_t *node)
{
	assert(lru != NULL);
	assert(node != NULL);
	if (node->data) {
		lru_data_free(lru, node->data);
	}
	IDLE_PNEXT (node) = lru->idle;
	lru->idle = node;
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
		while (lru->data_recycle) {
			recycle_t *cyc = lru->data_recycle;
			lru->data_recycle = cyc->next;
			free(cyc);
		}
		free(lru);
	}
}

uint32_t lru_set_release_data_slot(lru_t *lru, uint32_t slot_size)
{
	if (lru) {
		uint32_t orig = lru->release_data_slot;
		lru->release_data_slot = slot_size;
		return orig;
	}
	return 0;
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
	_new->mutex = 0;
	_new->size = lru_size;
	_new->release_data_slot = 100 * 1024 * 1024;
	_new->total_free_size = 0;
	_new->lru_head = NULL;
	_new->lru_tail = NULL;
	_new->hash = hash;
	_new->nodes = (node_t *) calloc(lru_size, sizeof(node_t));
	if ( ! _new->nodes ) {
		lru_free(_new);
		return NULL;
	}
	int i;
	HASH_NNEXT (_new->nodes[lru_size - 1]) = NULL;
	for (i=lru_size - 1; i--;) {
		HASH_NNEXT (_new->nodes[i]) = &(_new->nodes[i+1]);
	}
	_new->idle = _new->nodes;
	_new->data_recycle = NULL;

	_new->barrel = (node_t **) calloc(hash, sizeof(node_t*));
	if ( ! _new->barrel ) {
		lru_free(_new);
		return NULL;
	}
	return _new;
}

static void lru_data_free(lru_t *lru, char *data)
{
	if (lru->total_free_size > lru->release_data_slot) {
		while (lru->data_recycle) {
			recycle_t *cyc = lru->data_recycle;
			lru->data_recycle = cyc->next;
			free(cyc);
		}
		lru->total_free_size = 0;
	}

	size_t usable_size = malloc_usable_size(data);
	lru->total_free_size += usable_size;

	recycle_t *this = (recycle_t*) data;
	if (lru->data_recycle == NULL) {
		this->next = NULL;
		lru->data_recycle = this;
		return;
	}

	recycle_t *find = lru->data_recycle;
	recycle_t *last = NULL;
	size_t size = malloc_usable_size(find);
	if (usable_size < size) {
		lru->data_recycle = this;
		this->next = find;
		return;
	}
	for (last = find, find = find->next; find; last = find, find = find->next) {
		size = malloc_usable_size(find);
		if (usable_size < size) {
			last->next = this;
			this->next = find;
			return;
		}
	}
	last->next = this;
	this->next = NULL;
}

static void *lru_data_new(lru_t *lru, size_t size)
{
	if (lru->data_recycle == NULL) {
		return malloc(size);
	}

	recycle_t *find = lru->data_recycle;
	recycle_t *last = NULL;
	size_t usable_size = malloc_usable_size(find);
	if (usable_size > size) {
		lru->data_recycle = find->next;
		return find;
	}
	for (last = find, find = find->next; find; last = find, find = find->next) {
		usable_size = malloc_usable_size(find);
		if (usable_size > size) {
			last->next = find->next;
			return find;
		}
	}
	return malloc(size);
}

