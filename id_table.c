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
 * 10: funny falcon's Coalesced Hashing implementation [Feature #6962]
 * 11: simple open addressing with quadratic probing.
 */

#ifndef ID_TABLE_IMPL
#define ID_TABLE_IMPL 11
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

#elif ID_TABLE_IMPL == 10
#define ID_TABLE_USE_COALESCED_HASHING 1
#define ID_TABLE_USE_ID_SERIAL 1

#elif ID_TABLE_IMPL == 11
#define ID_TABLE_USE_SMALL_HASH 1
#define ID_TABLE_USE_ID_SERIAL 1

#else
#error
#endif

#if ID_TABLE_SWAP_RECENT_ACCESS && ID_TABLE_USE_LIST_SORTED
#error
#endif

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
    return (sizeof(id_key_t) + sizeof(VALUE)) * tbl->capa + sizeof(struct rb_id_table);
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

static int
id_table_delete(struct rb_id_table *tbl, id_key_t key, int index)
{
    if (index >= 0) {
#if ID_TABLE_USE_LIST_SORTED
	int i;
	const int num = tbl->num;
	id_key_t *keys = tbl->keys;
	VALUE *values = tbl->values;

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

int
rb_id_table_delete(struct rb_id_table *tbl, ID id)
{
    const id_key_t key = id2key(id);
    int index = table_index(tbl, key);
    return id_table_delete(tbl, key, index);
}

#define FOREACH_LAST() do {   \
    switch (ret) {            \
      case ID_TABLE_CONTINUE: \
      case ID_TABLE_STOP:     \
	break;                \
      case ID_TABLE_DELETE:   \
	id_table_delete(tbl, key, i); \
	num = tbl->num;               \
	i--; /* redo smae index */    \
	break; \
    } \
} while (0)

void
rb_id_table_foreach(struct rb_id_table *tbl, enum rb_id_table_iterator_result (*func)(ID id, VALUE val, void *data), void *data)
{
    int num = tbl->num;
    int i;
    const id_key_t *keys = tbl->keys;

    for (i=0; i<num; i++) {
	const id_key_t key = keys[i];
	enum rb_id_table_iterator_result ret = (*func)(key2id(key), tbl->values[i], data);
	assert(key != 0);

	FOREACH_LAST();
	if (ret == ID_TABLE_STOP) return;
    }
}

void
rb_id_table_foreach_values(struct rb_id_table *tbl, enum rb_id_table_iterator_result (*func)(VALUE val, void *data), void *data)
{
    int num = tbl->num;
    int i;
    const id_key_t *keys = tbl->keys;

    for (i=0; i<num; i++) {
	const id_key_t key = keys[i];
	enum rb_id_table_iterator_result ret = (*func)(tbl->values[i], data);
	assert(key != 0);

	FOREACH_LAST();
	if (ret == ID_TABLE_STOP) return;
    }
}
#endif /* ID_TABLE_USE_LIST */


#if ID_TABLE_USE_COALESCED_HASHING
/* implementation is based on
 * https://bugs.ruby-lang.org/issues/6962 by funny_falcon
 */

typedef unsigned int sa_index_t;

#define SA_EMPTY    0
#define SA_LAST     1
#define SA_OFFSET   2
#define SA_MIN_SIZE 4

typedef struct sa_entry {
    sa_index_t next;
    id_key_t key;
    VALUE value;
} sa_entry;

typedef struct rb_id_table {
    sa_index_t num_bins;
    sa_index_t num_entries;
    sa_index_t free_pos;
    sa_entry *entries;
} sa_table;

static void
sa_init_table(register sa_table *table, sa_index_t num_bins)
{
    if (num_bins) {
        table->num_entries = 0;
        table->entries = ZALLOC_N(sa_entry, num_bins);
        table->num_bins = num_bins;
        table->free_pos = num_bins;
    }
}

sa_table*
rb_id_table_create(size_t size)
{
    sa_table* table = ZALLOC(sa_table);
    sa_init_table(table, size);
    return table;
}

void
rb_id_table_clear(sa_table *table)
{
    xfree(table->entries);
    memset(table, 0, sizeof(sa_table));
}

void
rb_id_table_free(sa_table *table)
{
    xfree(table->entries);
    xfree(table);
}

size_t
rb_id_table_memsize(sa_table *table)
{
    return sizeof(sa_table) + table->num_bins * sizeof (sa_entry);
}

static inline sa_index_t
calc_pos(register sa_table* table, id_key_t key)
{
    return key & (table->num_bins - 1);
}

static void
fix_empty(register sa_table* table)
{
    while(--table->free_pos &&
            table->entries[table->free_pos-1].next != SA_EMPTY);
}

#define FLOOR_TO_4 ((~((sa_index_t)0)) << 2)
static sa_index_t
find_empty(register sa_table* table, register sa_index_t pos)
{
    sa_index_t new_pos = table->free_pos-1;
    sa_entry *entry;
    static unsigned offsets[][3] = {
	    {1, 2, 3},
	    {2, 3, 0},
	    {3, 1, 0},
	    {2, 1, 0}
    };
    unsigned *check = offsets[pos&3];
    pos &= FLOOR_TO_4;
    entry = table->entries+pos;

    if (entry[check[0]].next == SA_EMPTY) { new_pos = pos + check[0]; goto check; }
    if (entry[check[1]].next == SA_EMPTY) { new_pos = pos + check[1]; goto check; }
    if (entry[check[2]].next == SA_EMPTY) { new_pos = pos + check[2]; goto check; }

  check:
    if (new_pos+1 == table->free_pos) fix_empty(table);
    return new_pos;
}

static void resize(register sa_table* table);
static int insert_into_chain(register sa_table*, register sa_index_t, st_data_t, sa_index_t pos);
static int insert_into_main(register sa_table*, sa_index_t, st_data_t, sa_index_t pos, sa_index_t prev_pos);

static int
sa_insert(register sa_table* table, id_key_t key, VALUE value)
{
    register sa_entry *entry;
    sa_index_t pos, main_pos;

    if (table->num_bins == 0) {
        sa_init_table(table, SA_MIN_SIZE);
    }

    pos = calc_pos(table, key);
    entry = table->entries + pos;

    if (entry->next == SA_EMPTY) {
        entry->next = SA_LAST;
        entry->key = key;
        entry->value = value;
        table->num_entries++;
        if (pos+1 == table->free_pos) fix_empty(table);
        return 0;
    }

    if (entry->key == key) {
        entry->value = value;
        return 1;
    }

    if (table->num_entries + (table->num_entries >> 2) > table->num_bins) {
        resize(table);
	return sa_insert(table, key, value);
    }

    main_pos = calc_pos(table, entry->key);
    if (main_pos == pos) {
        return insert_into_chain(table, key, value, pos);
    }
    else {
        if (!table->free_pos) {
            resize(table);
            return sa_insert(table, key, value);
        }
        return insert_into_main(table, key, value, pos, main_pos);
    }
}

int
rb_id_table_insert(register sa_table* table, ID id, VALUE value)
{
    return sa_insert(table, id2key(id), value);
}

static int
insert_into_chain(register sa_table* table, id_key_t key, st_data_t value, sa_index_t pos)
{
    sa_entry *entry = table->entries + pos, *new_entry;
    sa_index_t new_pos;

    while (entry->next != SA_LAST) {
        pos = entry->next - SA_OFFSET;
        entry = table->entries + pos;
        if (entry->key == key) {
            entry->value = value;
            return 1;
        }
    }

    if (!table->free_pos) {
        resize(table);
        return sa_insert(table, key, value);
    }

    new_pos = find_empty(table, pos);
    new_entry = table->entries + new_pos;
    entry->next = new_pos + SA_OFFSET;

    new_entry->next = SA_LAST;
    new_entry->key = key;
    new_entry->value = value;
    table->num_entries++;
    return 0;
}

static int
insert_into_main(register sa_table* table, id_key_t key, st_data_t value, sa_index_t pos, sa_index_t prev_pos)
{
    sa_entry *entry = table->entries + pos;
    sa_index_t new_pos = find_empty(table, pos);
    sa_entry *new_entry = table->entries + new_pos;
    sa_index_t npos;

    *new_entry = *entry;

    while((npos = table->entries[prev_pos].next - SA_OFFSET) != pos) {
        prev_pos = npos;
    }
    table->entries[prev_pos].next = new_pos + SA_OFFSET;

    entry->next = SA_LAST;
    entry->key = key;
    entry->value = value;
    table->num_entries++;
    return 0;
}

static sa_index_t
new_size(sa_index_t num_entries)
{
    sa_index_t size = num_entries >> 3;
    size |= size >> 1;
    size |= size >> 2;
    size |= size >> 4;
    size |= size >> 8;
    size |= size >> 16;
    return (size + 1) << 3;
}

static void
resize(register sa_table *table)
{
    sa_table tmp_table;
    sa_entry *entry;
    sa_index_t i;

    if (table->num_entries == 0) {
        xfree(table->entries);
	memset(table, 0, sizeof(sa_table));
        return;
    }

    sa_init_table(&tmp_table, new_size(table->num_entries + (table->num_entries >> 2)));
    entry = table->entries;

    for(i = 0; i < table->num_bins; i++, entry++) {
        if (entry->next != SA_EMPTY) {
            sa_insert(&tmp_table, entry->key, entry->value);
        }
    }
    xfree(table->entries);
    *table = tmp_table;
}

int
rb_id_table_lookup(register sa_table *table, ID id, VALUE *valuep)
{
    register sa_entry *entry;
    id_key_t key = id2key(id);

    if (table->num_entries == 0) return 0;

    entry = table->entries + calc_pos(table, key);
    if (entry->next == SA_EMPTY) return 0;

    if (entry->key == key) goto found;
    if (entry->next == SA_LAST) return 0;

    entry = table->entries + (entry->next - SA_OFFSET);
    if (entry->key == key) goto found;

    while(entry->next != SA_LAST) {
        entry = table->entries + (entry->next - SA_OFFSET);
        if (entry->key == key) goto found;
    }
    return 0;
found:
    if (valuep) *valuep = entry->value;
    return 1;
}

size_t
rb_id_table_size(sa_table *table)
{
    return table->num_entries;
}

int
rb_id_table_delete(sa_table *table, ID id)
{
    sa_index_t pos, prev_pos = ~0;
    sa_entry *entry;
    id_key_t key = id2key(id);

    if (table->num_entries == 0) goto not_found;

    pos = calc_pos(table, key);
    entry = table->entries + pos;

    if (entry->next == SA_EMPTY) goto not_found;

    do {
        if (entry->key == key) {
            if (entry->next != SA_LAST) {
                sa_index_t npos = entry->next - SA_OFFSET;
                *entry = table->entries[npos];
                memset(table->entries + npos, 0, sizeof(sa_entry));
            }
            else {
                memset(table->entries + pos, 0, sizeof(sa_entry));
                if (~prev_pos) {
                    table->entries[prev_pos].next = SA_LAST;
                }
            }
            table->num_entries--;
            if (table->num_entries < table->num_bins / 4) {
                resize(table);
            }
            return 1;
        }
        if (entry->next == SA_LAST) break;
        prev_pos = pos;
        pos = entry->next - SA_OFFSET;
        entry = table->entries + pos;
    } while(1);

not_found:
    return 0;
}

enum foreach_type {
    foreach_key_values,
    foreach_values
};

static void
id_table_foreach(sa_table *table, enum rb_id_table_iterator_result (*func)(ANYARGS), void *arg, enum foreach_type type)
{
    sa_index_t i;

    if (table->num_bins > 0) {
	for(i = 0; i < table->num_bins ; i++) {
	    if (table->entries[i].next != SA_EMPTY) {
		id_key_t key = table->entries[i].key;
		st_data_t val = table->entries[i].value;
		enum rb_id_table_iterator_result ret;

		switch (type) {
		  case foreach_key_values:
		    ret = (*func)(key2id(key), val, arg);
		    break;
		  case foreach_values:
		    ret = (*func)(val, arg);
		    break;
		}

		switch (ret) {
		  case ID_TABLE_DELETE:
		    rb_warn("unsupported yet");
		    break;
		  default:
		    break;
		}
		if (ret == ID_TABLE_STOP) break;
	    }
	}
    }
}

void
rb_id_table_foreach(sa_table *table, enum rb_id_table_iterator_result (*func)(ID, VALUE, void *), void *arg)
{
    id_table_foreach(table, func, arg, foreach_key_values);
}

void
rb_id_table_foreach_values(sa_table *table, enum rb_id_table_iterator_result (*func)(VALUE, void *), void *arg)
{
    id_table_foreach(table, func, arg, foreach_values);
}

#endif /* ID_TABLE_USE_COALESCED_HASHING */

#ifdef ID_TABLE_USE_SMALL_HASH
/* simple open addressing with quadratic probing.
   uses mark-bit on collisions - need extra 1 bit,
   ID is strictly 3 bits larger than rb_id_serial_t */

typedef struct rb_id_item {
    id_key_t key;
#if SIZEOF_VALUE == 8
    int      collision;
#endif
    VALUE    val;
} item_t;

struct rb_id_table {
    int capa;
    int num;
    int used;
    item_t *items;
};

#if SIZEOF_VALUE == 8
#define ITEM_GET_KEY(tbl, i) ((tbl)->items[i].key)
#define ITEM_KEY_ISSET(tbl, i) ((tbl)->items[i].key)
#define ITEM_COLLIDED(tbl, i) ((tbl)->items[i].collision)
#define ITEM_SET_COLLIDED(tbl, i) ((tbl)->items[i].collision = 1)
static inline void
ITEM_SET_KEY(struct rb_id_table *tbl, int i, id_key_t key)
{
    tbl->items[i].key = key;
}
#else
#define ITEM_GET_KEY(tbl, i) ((tbl)->items[i].key >> 1)
#define ITEM_KEY_ISSET(tbl, i) ((tbl)->items[i].key > 1)
#define ITEM_COLLIDED(tbl, i) ((tbl)->items[i].key & 1)
#define ITEM_SET_COLLIDED(tbl, i) ((tbl)->items[i].key |= 1)
static inline void
ITEM_SET_KEY(struct rb_id_table *tbl, int i, id_key_t key)
{
    tbl->items[i].key = (key << 1) | ITEM_COLLIDED(tbl, i);
}
#endif

static inline int
round_capa(int capa) {
    /* minsize is 4 */
    capa >>= 2;
    capa |= capa >> 1;
    capa |= capa >> 2;
    capa |= capa >> 4;
    capa |= capa >> 8;
    capa |= capa >> 16;
    return (capa + 1) << 2;
}

struct
rb_id_table *rb_id_table_create(size_t capa)
{
    struct rb_id_table *tbl = ZALLOC(struct rb_id_table);

    if (capa > 0) {
	capa = round_capa(capa);
	tbl->capa = (int)capa;
	tbl->items = ZALLOC_N(item_t, capa);
    }
    return tbl;
}

void
rb_id_table_free(struct rb_id_table *tbl)
{
    xfree(tbl->items);
    xfree(tbl);
}

void
rb_id_table_clear(struct rb_id_table *tbl)
{
    tbl->num = 0;
    tbl->used = 0;
    MEMZERO(tbl->items, item_t, tbl->capa);
}

size_t
rb_id_table_size(struct rb_id_table *tbl)
{
    return (size_t)tbl->num;
}

size_t
rb_id_table_memsize(struct rb_id_table *tbl)
{
    return sizeof(item_t) * tbl->capa + sizeof(struct rb_id_table);
}

static int
table_index(struct rb_id_table* tbl, id_key_t key)
{
    if (tbl->capa > 0) {
	int mask = tbl->capa - 1;
	int ix = key & mask;
	int d = 1;
	while (key != ITEM_GET_KEY(tbl, ix)) {
	    if (!ITEM_COLLIDED(tbl, ix))
		return -1;
	    ix = (ix + d) & mask;
	    d++;
	}
	return ix;
    }
    return -1;
}

static void
table_raw_insert(struct rb_id_table *tbl, id_key_t key, VALUE val)
{
    int mask = tbl->capa - 1;
    int ix = key & mask;
    int d = 1;
    assert(key != 0);
    while (ITEM_KEY_ISSET(tbl, ix)) {
	ITEM_SET_COLLIDED(tbl, ix);
	ix = (ix + d) & mask;
	d++;
    }
    tbl->num++;
    if (!ITEM_COLLIDED(tbl, ix)) {
	tbl->used++;
    }
    ITEM_SET_KEY(tbl, ix, key);
    tbl->items[ix].val = val;
}

static int
id_table_delete(struct rb_id_table *tbl, int ix)
{
    if (ix >= 0) {
	if (!ITEM_COLLIDED(tbl, ix)) {
	    tbl->used--;
	}
	tbl->num--;
	ITEM_SET_KEY(tbl, ix, 0);
	tbl->items[ix].val = 0;
	return TRUE;
    } else {
	return FALSE;
    }
}

static void
table_extend(struct rb_id_table* tbl)
{
    if (tbl->used + (tbl->used >> 1) >= tbl->capa) {
	int new_cap = round_capa(tbl->num + (tbl->num >> 1));
	int i;
	item_t* old;
	struct rb_id_table tmp_tbl = {new_cap, 0, 0};
	tmp_tbl.items = ZALLOC_N(item_t, new_cap);
	for (i = 0; i < tbl->capa; i++) {
	    id_key_t key = ITEM_GET_KEY(tbl, i);
	    if (key != 0) {
		table_raw_insert(&tmp_tbl, key, tbl->items[i].val);
	    }
	}
	old = tbl->items;
	*tbl = tmp_tbl;
	xfree(old);
    }
}

#if ID_TABLE_DEBUG
static void
tbl_show(struct rb_id_table *tbl)
{
    const id_key_t *keys = tbl->keys;
    const int capa = tbl->capa;
    int i;

    fprintf(stderr, "tbl: %p (capa: %d, num: %d, used: %d)\n", tbl, tbl->capa, tbl->num, tbl->used);
    for (i=0; i<capa; i++) {
	if (ITEM_KEY_ISSET(tbl, i)) {
	    fprintf(stderr, " -> [%d] %s %d\n", i, rb_id2name(key2id(keys[i])), (int)keys[i]);
	}
    }
}
#endif

int
rb_id_table_lookup(struct rb_id_table *tbl, ID id, VALUE *valp)
{
    id_key_t key = id2key(id);
    int index = table_index(tbl, key);

    if (index >= 0) {
	*valp = tbl->items[index].val;
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
	tbl->items[index].val = val;
    }
    else {
	table_extend(tbl);
	table_raw_insert(tbl, key, val);
    }
    return TRUE;
}

int
rb_id_table_delete(struct rb_id_table *tbl, ID id)
{
    const id_key_t key = id2key(id);
    int index = table_index(tbl, key);
    return id_table_delete(tbl, index);
}

void
rb_id_table_foreach(struct rb_id_table *tbl, enum rb_id_table_iterator_result (*func)(ID id, VALUE val, void *data), void *data)
{
    int i, capa = tbl->capa;

    for (i=0; i<capa; i++) {
	if (ITEM_KEY_ISSET(tbl, i)) {
	    const id_key_t key = ITEM_GET_KEY(tbl, i);
	    enum rb_id_table_iterator_result ret = (*func)(key2id(key), tbl->items[i].val, data);
	    assert(key != 0);

	    if (ret == ID_TABLE_DELETE)
		id_table_delete(tbl, i);
	    else if (ret == ID_TABLE_STOP)
		return;
	}
    }
}

void
rb_id_table_foreach_values(struct rb_id_table *tbl, enum rb_id_table_iterator_result (*func)(VALUE val, void *data), void *data)
{
    int i, capa = tbl->capa;

    for (i=0; i<capa; i++) {
	if (ITEM_KEY_ISSET(tbl, i)) {
	    enum rb_id_table_iterator_result ret = (*func)(tbl->items[i].val, data);

	    if (ret == ID_TABLE_DELETE)
		id_table_delete(tbl, i);
	    else if (ret == ID_TABLE_STOP)
		return;
	}
    }
}
#endif /* ID_TABLE_USE_SMALL_HASH */
