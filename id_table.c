/* This file is included by symbol.c */

#include "id_table.h"

#ifndef ID_TABLE_DEBUG
#define ID_TABLE_DEBUG 0
#endif

#if ID_TABLE_DEBUG == 0
#define NDEBUG
#endif
#include <assert.h>

/*
 * 0: using st with debug information.
 * 1: using st.
 * 2: simple array. ids = [ID1, ID2, ...], values = [val1, val2, ...]
 * 3: simple array, and use rb_id_serial_t instead of ID.
 * 4: simple array, and use rb_id_serial_t instead of ID. Swap recent access.
 * 5: sorted array, and use rb_id_serial_t instead of ID.
 */

#ifndef ID_TABLE_IMPL
#define ID_TABLE_IMPL 5
#endif

#if ID_TABLE_IMPL == 0
#define ID_TABLE_USE_ST 1
#define ID_TABLE_USE_ST_DEBUG 1

#elif ID_TABLE_IMPL == 1
#define ID_TABLE_USE_ST 1
#define ID_TABLE_USE_ST_DEBUG 0

#elif ID_TABLE_IMPL == 2
#define ID_TABLE_USE_LIST 1

#elif ID_TABLE_IMPL == 3
#define ID_TABLE_USE_LIST 1
#define ID_TABLE_USE_ID_SERIAL 1

#elif ID_TABLE_IMPL == 4
#define ID_TABLE_USE_LIST 1
#define ID_TABLE_USE_ID_SERIAL 1
#define ID_TABLE_SWAP_RECENT_ACCESS 1

#elif ID_TABLE_IMPL == 5
#define ID_TABLE_USE_LIST 1
#define ID_TABLE_USE_ID_SERIAL 1
#define ID_TABLE_USE_LIST_SORTED 1

#else
#error
#endif


#if ID_TABLE_SWAP_RECENT_ACCESS && ID_TABLE_USE_LIST_SORTED
#error
#endif

/***************************************************************
 * 0: using st with debug information.
 * 1: using st.
 ***************************************************************/
#if ID_TABLE_USE_ST

#if ID_TABLE_USE_ST_DEBUG
#define ID_TABLE_MARK 0x12345678

struct rb_id_table {
    struct st_table *st;
    unsigned int check;
};

static struct st_table *
tbl2st(struct rb_id_table *tbl) {
    if (tbl->check != ID_TABLE_MARK) rb_bug("tbl2st: check error %x", tbl->check);
    return tbl->st;
}

struct rb_id_table *
rb_id_table_create(size_t size)
{
    struct rb_id_table *tbl = ALLOC(struct rb_id_table);
    tbl->st = st_init_numtable_with_size(size);
    tbl->check = ID_TABLE_MARK;
    return tbl;
}

void
rb_id_table_free(struct rb_id_table *tbl)
{
    st_free_table(tbl->st);
    xfree(tbl);
}

#else /* ID_TABLE_USE_ST_DEBUG */

struct rb_id_table {
    struct st_table st;
};

static struct st_table *
tbl2st(struct rb_id_table *tbl) {
    return (struct st_table *)tbl;
}

struct rb_id_table *
rb_id_table_create(size_t size)
{
    return (struct rb_id_table *)st_init_numtable_with_size(size);
}

void
rb_id_table_free(struct rb_id_table *tbl)
{
    st_free_table((struct st_table*)tbl);
}

#endif /* ID_TABLE_USE_ST_DEBUG */

void
rb_id_table_clear(struct rb_id_table *tbl)
{
    st_clear(tbl2st(tbl));
}

size_t
rb_id_table_size(struct rb_id_table *tbl)
{
    return tbl2st(tbl)->num_entries;
}

size_t
rb_id_table_memsize(struct rb_id_table *tbl)
{
    size_t header_size = ID_TABLE_USE_ST_DEBUG ? sizeof(struct rb_id_table) : 0;
    return header_size + st_memsize(tbl2st(tbl));
}

int
rb_id_table_lookup(struct rb_id_table *tbl, ID id, VALUE *val)
{
    return st_lookup(tbl2st(tbl), (st_data_t)id, (st_data_t *)val);
}

int
rb_id_table_insert(struct rb_id_table *tbl, ID id, VALUE val)
{
    return st_insert(tbl2st(tbl), id, val);
}

int
rb_id_table_delete(struct rb_id_table *tbl, ID id)
{
    return st_delete(tbl2st(tbl), (st_data_t *)&id, NULL);
}

void
rb_id_table_foreach(struct rb_id_table *tbl, enum rb_id_table_iterator_result (*func)(ID id, VALUE val, void *data), void *data)
{
    st_foreach(tbl2st(tbl), (int (*)(ANYARGS))func, (st_data_t)data);
}

struct values_iter_data {
    enum rb_id_table_iterator_result (*values_i)(VALUE val, void *data);
    void *data;
};

static int
each_values(st_data_t key, st_data_t val, st_data_t ptr)
{
    struct values_iter_data *values_iter_data = (struct values_iter_data *)ptr;
    return values_iter_data->values_i(val, values_iter_data->data);
}

void
rb_id_table_foreach_values(struct rb_id_table *tbl, enum rb_id_table_iterator_result (*func)(VALUE val, void *data), void *data)
{
    struct values_iter_data values_iter_data;
    values_iter_data.values_i = func;
    values_iter_data.data = data;
    st_foreach(tbl2st(tbl), each_values, (st_data_t)&values_iter_data);
}

#endif /* ID_TABLE_USE_ST */

#if ID_TABLE_USE_LIST

#if ID_TABLE_USE_ID_SERIAL

typedef rb_id_serial_t id_key_t;
static inline ID
key2id(id_key_t key)
{
    return rb_id_serial_to_id(key);
}

static inline id_key_t
id2key(ID id)
{
    return rb_id_to_serial(id);
}

#else /* ID_TABLE_USE_ID_SERIAL */

typedef ID id_key_t;
#define key2id(key) key
#define id2key(id)  id

#endif /* ID_TABLE_USE_ID_SERIAL */

#define TABLE_MIN_CAPA 8

struct rb_id_table {
    int capa;
    int num;
    id_key_t *keys;
    VALUE *values;
};

struct
rb_id_table *rb_id_table_create(size_t capa)
{
    struct rb_id_table *tbl = ZALLOC(struct rb_id_table);

    if (capa > 0) {
	tbl->capa = (int)capa;
	tbl->keys = ALLOC_N(id_key_t, capa);
	tbl->values = ALLOC_N(VALUE, capa);
    }
    return tbl;
}

void
rb_id_table_free(struct rb_id_table *tbl)
{
    xfree(tbl->keys);
    xfree(tbl->values);
    xfree(tbl);
}

void
rb_id_table_clear(struct rb_id_table *tbl)
{
    tbl->num = 0;
}

size_t
rb_id_table_size(struct rb_id_table *tbl)
{
    return (size_t)tbl->num;
}

size_t
rb_id_table_memsize(struct rb_id_table *tbl)
{
    return (sizeof(ID) + sizeof(VALUE)) * tbl->capa + sizeof(struct rb_id_table);
}

static void
table_extend(struct rb_id_table *tbl)
{
    if (tbl->capa == tbl->num) {
	tbl->capa = tbl->capa == 0 ? TABLE_MIN_CAPA : (tbl->capa * 2);
	tbl->keys = (id_key_t *)xrealloc(tbl->keys, sizeof(id_key_t) * tbl->capa);
	tbl->values = (VALUE *)xrealloc(tbl->values, sizeof(VALUE) * tbl->capa);
    }
}

#if ID_TABLE_DEBUG
static void
tbl_show(struct rb_id_table *tbl)
{
    const id_key_t *keys = tbl->keys;
    const int num = tbl->num;
    int i;

    fprintf(stderr, "tbl: %p (num: %d)\n", tbl, num);
    for (i=0; i<num; i++) {
	fprintf(stderr, " -> [%d] %s %d\n", i, rb_id2name(key2id(keys[i])), (int)keys[i]);
    }
}
#endif

static void
tbl_assert(struct rb_id_table *tbl)
{
#if ID_TABLE_DEBUG
#if ID_TABLE_USE_LIST_SORTED
    const id_key_t *keys = tbl->keys;
    const int num = tbl->num;
    int i;

    for (i=0; i<num-1; i++) {
	if (keys[i] >= keys[i+1]) {
	    tbl_show(tbl);
	    rb_bug(": not sorted.");
	}
    }
#endif
#endif
}

#if ID_TABLE_USE_LIST_SORTED
static int
ids_bsearch(const id_key_t *keys, id_key_t key, int num)
{
    int p, min = 0, max = num;

    while (1) {
	p = min + (max - min) / 2;

	if (min >= max) {
	    break;
	}
	else {
	    id_key_t kp = keys[p];
	    assert(p < max);
	    assert(p >= min);

	    if      (kp > key) max = p;
	    else if (kp < key) min = p+1;
	    else {
		assert(kp == key);
		assert(p >= 0);
		assert(p < num);
		return p;
	    }
	}
    }

    assert(min == max);
    assert(min == p);
    return -p-1;
}
#endif /* ID_TABLE_USE_LIST_SORTED */

static int
table_index(struct rb_id_table *tbl, id_key_t key)
{
    const int num = tbl->num;
    const id_key_t *keys = tbl->keys;

#if ID_TABLE_USE_LIST_SORTED
    return ids_bsearch(keys, key, num);
#else /* ID_TABLE_USE_LIST_SORTED */
    int i;

    for (i=0; i<num; i++) {
	assert(keys[i] != 0);

	if (keys[i] == key) {
	    return (int)i;
	}
    }
    return -1;
#endif
}

int
rb_id_table_lookup(struct rb_id_table *tbl, ID id, VALUE *valp)
{
    id_key_t key = id2key(id);
    int index = table_index(tbl, key);

    if (index >= 0) {
	*valp = tbl->values[index];

#if ID_TABLE_SWAP_RECENT_ACCESS
	if (index > 0) {
	    id_key_t tk = tbl->keys[index-1];
	    VALUE tv = tbl->values[index-1];
	    tbl->keys[index-1] = tbl->keys[index];
	    tbl->keys[index] = tk;
	    tbl->values[index-1] = tbl->values[index];
	    tbl->values[index] = tv;
	}
#endif /* ID_TABLE_SWAP_RECENT_ACCESS */
	return TRUE;
    }
    else {
	return FALSE;
    }
}

int
rb_id_table_insert(struct rb_id_table *tbl, ID id, VALUE val)
{
    const id_key_t key = id2key(id);
    const int index = table_index(tbl, key);

    if (index >= 0) {
	tbl->values[index] = val;
    }
    else {
	table_extend(tbl);
	{
	    const int num = tbl->num++;
#if ID_TABLE_USE_LIST_SORTED
	    const int insert_index = -(index + 1);
	    id_key_t *keys = tbl->keys;
	    VALUE *values = tbl->values;
	    int i;

	    if (0) fprintf(stderr, "insert: %d into %d on\n", (int)key, insert_index);

	    for (i=num; i>insert_index; i--) {
		keys[i] = keys[i-1];
		values[i] = values[i-1];
	    }
	    keys[i] = key;
	    values[i] = val;

	    tbl_assert(tbl);
#else
	    tbl->keys[num] = key;
	    tbl->values[num] = val;
#endif
	}
    }

    return TRUE;
}

int
rb_id_table_delete(struct rb_id_table *tbl, ID id)
{
    const id_key_t key = id2key(id);
    int index = table_index(tbl, key);

    if (index >= 0) {
#if ID_TABLE_USE_LIST_SORTED
	int i;
	const int num = tbl->num;
	id_key_t *keys = tbl->keys;
	VALUE *values = tbl->values;

	if (0) fprintf(stderr, "delete: %s from %d\n", rb_id2name(id), index);

	for (i=index+1; i<num; i++) { /* compaction */
	    keys[i-1] = keys[i];
	    values[i-1] = values[i];
	}
#else
	tbl->keys[index] = tbl->keys[tbl->num];
	tbl->values[index] = tbl->values[tbl->num];
#endif
	tbl->num--;
	tbl_assert(tbl);

	return TRUE;
    }
    else {
	return FALSE;
    }
}

void
rb_id_table_foreach(struct rb_id_table *tbl, enum rb_id_table_iterator_result (*func)(ID id, VALUE val, void *data), void *data)
{
    const int num = tbl->num;
    int i;
    const id_key_t *keys = tbl->keys;

    for (i=0; i<num; i++) {
	if (keys[i] != 0) {
	    enum rb_id_table_iterator_result ret = (*func)(key2id(keys[i]), tbl->values[i], data);

	    switch (ret) {
	      case ID_TABLE_CONTINUE:
		break;
	      case ID_TABLE_STOP:
		return;
	      default:
		rb_warn("unknown return value of id_table_foreach(): %d", ret);
		break;
	    }
	}
    }
}

void
rb_id_table_foreach_values(struct rb_id_table *tbl, enum rb_id_table_iterator_result (*func)(VALUE val, void *data), void *data)
{
    const int num = tbl->num;
    int i;
    const id_key_t *keys = tbl->keys;

    for (i=0; i<num; i++) {
	if (keys[i] != 0) {
	    enum rb_id_table_iterator_result ret = (*func)(tbl->values[i], data);

	    switch (ret) {
	      case ID_TABLE_CONTINUE:
		break;
	      case ID_TABLE_STOP:
		return;
	      default:
		rb_warn("unknown return value of rb_id_table_foreach_values(): %d", ret);
		break;
	    }
	}
    }
}

#endif /* ID_TABLE_USE_LIST */
