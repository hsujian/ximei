#ifndef __XDJ_LRU_H_
#define __XDJ_LRU_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct lru_find_t {
		uint32_t key1;
		uint32_t key2;
		uint32_t size;
		void *data;
	} lru_find_t;

	typedef struct lru_t lru_t;

	int lru_set(lru_t *lru, lru_find_t *set, uint32_t num);
	int lru_get(lru_t *lru, lru_find_t *get, uint32_t num);
	int lru_del(lru_t *lru, lru_find_t *del, uint32_t num);
	void lru_free(lru_t *lru);
	lru_t *lru_new(uint32_t lru_size, uint32_t hash);
	void lru_lock(lru_t *lru);
	void lru_unlock(lru_t *lru);


#ifdef __cplusplus
}
#endif

#endif /* __XDJ_LRU_H_ */

