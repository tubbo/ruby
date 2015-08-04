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
 */

#ifndef ID_TABLE_IMPL
#define ID_TABLE_IMPL 10
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

typedef rb_id_serial_t id_key_t;
typedef unsigned int sa_index_t;

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

static inline sa_index_t
calc_pos(register sa_table* table, id_key_t key)
{
    /* this formula is empirical */
    /* it has no good avalance, but works well in our case */
    key ^= key >> 16;
    key *= 0x445229;
    return (key + (key >> 16)) % table->num_bins;
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
    pos &= FLOOR_TO_4;
    entry = table->entries+pos;

    if (entry->next == SA_EMPTY) { new_pos = pos; goto check; }
    pos++; entry++;
    if (entry->next == SA_EMPTY) { new_pos = pos; goto check; }
    pos++; entry++;
    if (entry->next == SA_EMPTY) { new_pos = pos; goto check; }
    pos++; entry++;
    if (entry->next == SA_EMPTY) { new_pos = pos; goto check; }

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
    sa_index_t msb = num_entries;
    msb |= msb >> 1;
    msb |= msb >> 2;
    msb |= msb >> 4;
    msb |= msb >> 8;
    msb |= msb >> 16;
    msb = ((msb >> 4) + 1) << 3;
    return (num_entries & (msb | (msb >> 1))) + (msb >> 1);
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

void
rb_id_table_foreach(sa_table *table, enum rb_id_table_iterator_result (*func)(ID, VALUE, void *), void *arg)
{
    sa_index_t i;

    if (table->num_bins > 0) {
	for(i = 0; i < table->num_bins ; i++) {
	    if (table->entries[i].next != SA_EMPTY) {
		id_key_t key = table->entries[i].key;
		st_data_t val = table->entries[i].value;
		enum rb_id_table_iterator_result ret = (*func)(key2id(key), val, arg);
		if (ret == ID_TABLE_STOP) break;
	    }
	}
    }
}

#endif /* ID_TABLE_USE_COALESCED_HASHING */
