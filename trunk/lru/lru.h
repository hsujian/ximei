#ifndef __XDJ_LRU_H_
#define __XDJ_LRU_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//	typedef void (*data_free_fn)(void *data, uint32_t size);

	typedef struct lru_find_t {
		uint32_t key1;
		uint32_t key2;
		uint32_t size;
		void *data;
	} lru_find_t;

	typedef struct lru_t lru_t;

	int lru_set(lru_t *lru, lru_find_t *set, int num);
	int lru_get(lru_t *lru, lru_find_t *get, int num);
	int lru_del(lru_t *lru, uint32_t key1, uint32_t key2);
	void lru_free(lru_t *lru);
//	void lru_set_data_free_fn(lru_t *lru, data_free_fn free_fn);
	lru_t *lru_new(int lru_size, int hash);
	void lru_lock(lru_t *lru);
	void lru_unlock(lru_t *lru);


#ifdef __cplusplus
}
#endif

#endif /* __XDJ_LRU_H_ */

