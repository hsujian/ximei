#ifndef __XDJ_LRU_H_
#define __XDJ_LRU_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

	/* typedef void (*data_free_fn)(void *data, uint32_t size); */

	typedef struct lru_find_t {
		uint32_t key1;
		uint32_t key2;
		uint32_t size;
		void *data;
	} lru_find_t;

	typedef struct lru_t lru_t;

	/**
	 * -1 error
	 *  0 ok
	 */
	int lru_set(lru_t *lru, lru_find_t *set, int num);

	/**
	 * -1 error
	 *  >=0 get num
	 */
	int lru_get(lru_t *lru, lru_find_t *get, int num);

	/**
	 * -1 error
	 *  0 succ or nofind
	 */
	int lru_del(lru_t *lru, uint32_t key1, uint32_t key2);

	void lru_free(lru_t *lru);
	/* void lru_set_data_free_fn(lru_t *lru, data_free_fn free_fn); */

	/**
	 * @return point or NULL
	 */
	lru_t *lru_new(int lru_size, int hash);

	/**
	 * set data release free memory slot
	 */
	uint32_t lru_set_release_data_slot(lru_t *lru, uint32_t slot_size);

#ifdef __cplusplus
}
#endif

#endif /* __XDJ_LRU_H_ */

