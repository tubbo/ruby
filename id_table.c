/* This file is included by symbol.c */

#include "id_table.h"

/*
 * 0: using st with debug information.
 * 1: using st.
 */
#ifndef ID_TABLE_IMPL
#define ID_TABLE_IMPL 1
#endif

/***************************************************************
 * 0: using st with debug information.
 * 1: using st.
 ***************************************************************/
#if ID_TABLE_IMPL == 0 || ID_TABLE_IMPL == 1

#if ID_TABLE_IMPL == 0
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
#elif ID_TABLE_IMPL == 1
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
#else
#error
#endif

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
    size_t header_size = (ID_TABLE_IMPL == 0) ? sizeof(struct rb_id_table) : 0;
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

#else
#error "Not supported ID_TABLE_IMPL."
#endif
