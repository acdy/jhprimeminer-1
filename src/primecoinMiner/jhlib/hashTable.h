#include "../inttype.h"
#include <stdlib.h>

typedef struct  
{
	int32_t itemIndex;
}_HashTable_uint32Iterable_entry_t;

typedef struct
{
	_HashTable_uint32Iterable_entry_t *entrys;
	uint32_t *itemKeyArray;
	void **itemValueArray;
	uint32_t size;
	uint32_t count;
}hashTable_t;

void hashTable_init(hashTable_t *hashTable, int32_t itemLimit);
void hashTable_destroy(hashTable_t *hashTable);
void hashTable_clear(hashTable_t *hashTable);
bool hashTable_set(hashTable_t *hashTable, uint32_t key, void *item);
void *hashTable_get(hashTable_t *hashTable, uint32_t key);

void** hashTable_getValueArray(hashTable_t *hashTable);
uint32_t* hashTable_getKeyArray(hashTable_t *hashTable);
uint32_t hashTable_getCount(hashTable_t *hashTable);

bool hashTable_set(hashTable_t *hashTable, char *key, void *item);
void *hashTable_get(hashTable_t *hashTable, char *key);

