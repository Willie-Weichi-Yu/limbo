// vim:filetype=c:textwidth=80:shiftwidth=4:softtabstop=4:expandtab
/*
 * An automatically resizing array container.
 * The capacity doubles each time the current capacity is exhausted.
 * Elements of the vector are const void pointers.
 * This should help preventing unintended changes of elements in vectors.
 * Since our set is based on the vector, such changes might compromise the
 * set structure.
 *
 * Each vector object needs to be initialized with vector_init[_with_size]()
 * or vector_copy[_range](). The latter ones copy [the range of] a vector into
 * the new one. The functions vector_lazy_copy[_range]() rely on the source
 * vector not being modified afterwards; otherwise the behavior of the new
 * vector is undefined. All lazy copies of a vector may be modified freely,
 * however, without any interdependencies.
 * Deallocation is done with vector_cleanup(). It's not necessary to deallocate
 * a lazy copy (unless it was modified and thus is not lazy anymore). 
 *
 * vector_cmp() compares as follows. If the two vectors have different size,
 * the shorter one is less than the the longer one. If both vectors have the
 * same size, their elements are compared. If compar is NULL, this is done with
 * memcmp on elements, which usually are just pointers; otherwise compar is
 * called iteratively.
 * This characteristic is exploited particularly by setup.c.
 *
 * After vector_insert(), vector_get() returns the inserted element for the
 * respective index. After vector_insert_all[_range](), first element of the
 * [indicated range of the] source is at the respective index of the target,
 * followed by the remaining elements from [the range of] the target.
 *
 * There are (const) iterators. The non-const version also supports removal and
 * replacement of the current element. [const_]iter_next() advances to the next
 * element and must be called before accessing the first element.
 * The element can be accessed with the attributes val and val_unsafe in
 * vector_[const_]iter_t. This safes typing over vector_[const_]iter_get().
 * For further convience there are macros EACH[_CONST][_FROM] which are supposed
 * to be used within the brackets of a for-loop.
 * Other containers like sets (for now, only sets) implement the same interface.
 *
 * Indices start at 0.
 *
 * For all functions with _range in its name: the from index is meant
 * inclusively, the to index is exclusive. That is, the range has (to - from)
 * elements.
 *
 * schwering@kbsg.rwth-aachen.de
 */
#ifndef _VECTOR_H_
#define _VECTOR_H_

#include <stdbool.h>

typedef struct {
    void const **array;
    int capacity;
    int size;
} vector_t;

union vector_const_iter {
    const void * const val;
    void * const val_unsafe;
    const int index;
    struct {
        const void *val;
        int index;
        const vector_t *vec;
    } i;
};
typedef union vector_const_iter vector_const_iter_t;

typedef union vector_iter vector_iter_t;
union vector_iter {
    const void * const val;
    void * const val_unsafe;
    const int index;
    struct {
        const void *val;
        vector_t *vec;
        int index;
        vector_iter_t **audience;
    } i;
};

vector_t vector_init(void);
vector_t vector_init_with_size(int size);
vector_t vector_copy(const vector_t *src);
vector_t vector_copy_range(const vector_t *src, int from, int to);
vector_t vector_lazy_copy(const vector_t *src);
vector_t vector_lazy_copy_range(const vector_t *src, int from, int to);
vector_t vector_prepend_copy(const void *elem, const vector_t *src);
vector_t vector_copy_append(const vector_t *src, const void *elem);
vector_t vector_singleton(const void *e);
vector_t vector_concat(const vector_t *vec1, const vector_t *vec2);
vector_t vector_from_array(const void *array[], int n);
void vector_cleanup(vector_t *vec);
bool vector_is_lazy_copy(const vector_t *vec);

const void *vector_get(const vector_t *vec, int index);
void *vector_get_unsafe(vector_t *vec, int index);
const void **vector_array(const vector_t *vec);
int vector_size(const vector_t *vec);

int vector_cmp(const vector_t *vec1, const vector_t *vec2,
        int (*compar)(const void *, const void *));
bool vector_eq(const vector_t *vec1, const vector_t *vec2,
        int (*compar)(const void *, const void *));
bool vector_is_prefix(const vector_t *vec1, const vector_t *vec2,
        int (*compar)(const void *, const void *));

void vector_set(vector_t *vec, int index, const void *elem);

void vector_prepend(vector_t *vec, const void *elem);
void vector_append(vector_t *vec, const void *elem);
void vector_insert(vector_t *vec, int index, const void *elem);

void vector_prepend_all(vector_t *vec, const vector_t *elems);
void vector_append_all(vector_t *vec, const vector_t *elems);
void vector_insert_all(vector_t *vec, int index, const vector_t *elems);

void vector_prepend_all_range(vector_t *vec,
        const vector_t *elems, int from, int to);
void vector_append_all_range(vector_t *vec,
        const vector_t *elems, int from, int to);
void vector_insert_all_range(vector_t *vec, int index,
        const vector_t *elems, int from, int to);

const void *vector_remove_first(vector_t *vec);
const void *vector_remove_last(vector_t *vec);
const void *vector_remove(vector_t *vec, int index);
void vector_remove_range(vector_t *vec, int from, int to);
void vector_remove_all(vector_t *vec, const int indices[], int n_indices);
void vector_clear(vector_t *vec);

vector_iter_t vector_iter(vector_t *vec, int index);
bool vector_iter_next(vector_iter_t *iter);
const void *vector_iter_get(const vector_iter_t *iter);
int vector_iter_index(const vector_iter_t *iter);
void vector_iter_add_auditor(vector_iter_t *iter, vector_iter_t *auditor);
void vector_iter_dispatch_removals(vector_iter_t *iter, const int index);
void vector_iter_remove(vector_iter_t *iter);
void vector_iter_replace(vector_iter_t *iter, const void *new_elem);

#define EACH(prefix, container, itername) \
    EACH_FROM(prefix, container, itername, 0)
#define EACH_FROM(prefix, container, itername, from) \
    prefix##_iter_t itername = prefix##_iter(container, from);\
    prefix##_iter_next(&itername);

vector_const_iter_t vector_const_iter(const vector_t *vec, int index);
bool vector_const_iter_next(vector_const_iter_t *iter);
const void *vector_const_iter_get(const vector_const_iter_t *iter);

#define EACH_CONST(prefix, container, itername) \
    EACH_CONST_FROM(prefix, container, itername, 0)
#define EACH_CONST_FROM(prefix, container, itername, from) \
    prefix##_const_iter_t itername = prefix##_const_iter(container, from);\
    prefix##_const_iter_next(&itername);

#define VECTOR_DECL(prefix, type) \
    typedef union { vector_t v; } prefix##_t;\
    typedef union {\
        const type const val;\
        type const val_unsafe;\
        vector_iter_t i;\
    } prefix##_iter_t;\
    typedef union {\
        const type const val;\
        type const val_unsafe;\
        vector_const_iter_t i;\
    } prefix##_const_iter_t;\
    prefix##_t prefix##_init(void);\
    prefix##_t prefix##_init_with_size(int size);\
    prefix##_t prefix##_copy(const prefix##_t *src);\
    prefix##_t prefix##_copy_range(const prefix##_t *src, int from, int to);\
    prefix##_t prefix##_lazy_copy(const prefix##_t *src);\
    prefix##_t prefix##_lazy_copy_range(const prefix##_t *src,\
            int from, int to);\
    prefix##_t prefix##_copy_append(const prefix##_t *src, const type elem);\
    prefix##_t prefix##_prepend_copy(const type elem, const prefix##_t *src);\
    prefix##_t prefix##_singleton(const type e);\
    prefix##_t prefix##_concat(const prefix##_t *vec1, const prefix##_t *vec2);\
    prefix##_t prefix##_from_array(const type array[], int n);\
    void prefix##_cleanup(prefix##_t *v);\
    bool prefix##_is_lazy_copy(const prefix##_t *v);\
    const type prefix##_get(const prefix##_t *v, int index);\
    type prefix##_get_unsafe(prefix##_t *v, int index);\
    const type *prefix##_array(const prefix##_t *v);\
    int prefix##_size(const prefix##_t *v);\
    int prefix##_cmp(const prefix##_t *v1, const prefix##_t *v2);\
    bool prefix##_eq(const prefix##_t *v1, const prefix##_t *v2);\
    bool prefix##_is_prefix(const prefix##_t *v1, const prefix##_t *v2);\
    void prefix##_set(prefix##_t *v, int index, const type elem);\
    void prefix##_prepend(prefix##_t *v, const type elem);\
    void prefix##_append(prefix##_t *v, const type elem);\
    void prefix##_insert(prefix##_t *v, int index, const type elem);\
    void prefix##_prepend_all(prefix##_t *v, const prefix##_t *elems);\
    void prefix##_append_all(prefix##_t *v, const prefix##_t *elems);\
    void prefix##_insert_all(prefix##_t *v,\
            int index, const prefix##_t *elems);\
    void prefix##_prepend_all_range(\
            prefix##_t *v, const prefix##_t *elems, int from, int to);\
    void prefix##_append_all_range(prefix##_t *v,\
            const prefix##_t *elems, int from, int to);\
    void prefix##_insert_all_range(prefix##_t *v,\
            int index, const prefix##_t *elems, int from, int to);\
    const type prefix##_remove_first(prefix##_t *v);\
    const type prefix##_remove_last(prefix##_t *v);\
    const type prefix##_remove(prefix##_t *v, int index);\
    void prefix##_remove_range(prefix##_t *v, int from, int to);\
    void prefix##_remove_all(prefix##_t *v, const int indices[],\
            int n_indices);\
    void prefix##_clear(prefix##_t *v);\
    prefix##_iter_t prefix##_iter(prefix##_t *vec, int index);\
    bool prefix##_iter_next(prefix##_iter_t *iter);\
    const type prefix##_iter_get(const prefix##_iter_t *iter);\
    int prefix##_iter_index(const prefix##_iter_t *iter);\
    void prefix##_iter_add_auditor(prefix##_iter_t *iter,\
            prefix##_iter_t *auditor);\
    void prefix##_const_iter_remove(prefix##_const_iter_t *iter);\
    void prefix##_iter_replace(prefix##_iter_t *iter, const type new_elem);\
    prefix##_const_iter_t prefix##_const_iter(const prefix##_t *vec,\
            int index);\
    bool prefix##_const_iter_next(prefix##_const_iter_t *iter);\
    const type prefix##_const_iter_get(const prefix##_const_iter_t *iter);

#define VECTOR_IMPL(prefix, type, compar) \
    prefix##_t prefix##_init(void) {\
        return (prefix##_t) { .v = vector_init() }; }\
    prefix##_t prefix##_init_with_size(int size) {\
        return (prefix##_t) { .v = vector_init_with_size(size) }; }\
    prefix##_t prefix##_copy(const prefix##_t *src) {\
        return (prefix##_t) { .v = vector_copy(&src->v) }; }\
    prefix##_t prefix##_copy_range(const prefix##_t *src, int from, int to) {\
        return (prefix##_t) { .v = vector_copy_range(&src->v, from, to) }; }\
    prefix##_t prefix##_lazy_copy(const prefix##_t *src) {\
        return (prefix##_t) { .v = vector_lazy_copy(&src->v) }; }\
    prefix##_t prefix##_lazy_copy_range(const prefix##_t *src,\
            int from, int to) {\
        return (prefix##_t) { .v = vector_lazy_copy_range(&src->v,\
                from, to) }; }\
    prefix##_t prefix##_copy_append(const prefix##_t *src, const type elem) {\
        return (prefix##_t) { .v = vector_copy_append(&src->v,\
                (const void *) elem) }; }\
    prefix##_t prefix##_prepend_copy(const type elem, const prefix##_t *src) {\
        return (prefix##_t) { .v = vector_prepend_copy(\
                (const void *) elem, &src->v) }; }\
    prefix##_t prefix##_singleton(const type e) {\
        return (prefix##_t) { .v = vector_singleton((const void *) e) }; }\
    prefix##_t prefix##_concat(const prefix##_t *vec1,\
            const prefix##_t *vec2) {\
        return (prefix##_t) { .v = vector_concat(&vec1->v, &vec2->v) }; }\
    prefix##_t prefix##_from_array(const type array[], int n) {\
        return (prefix##_t) {\
            .v = vector_from_array((const void **) array, n) }; }\
    void prefix##_cleanup(prefix##_t *v) {\
        vector_cleanup(&v->v); }\
    bool prefix##_is_lazy_copy(const prefix##_t *v) {\
        return vector_is_lazy_copy(&v->v); }\
    const type prefix##_get(const prefix##_t *v, int index) {\
        return (const type) vector_get(&v->v, index); }\
    type prefix##_get_unsafe(prefix##_t *v, int index) {\
        return (type) vector_get_unsafe(&v->v, index); }\
    const type *prefix##_array(const prefix##_t *v) {\
        return (const type *) vector_array(&v->v); }\
    int prefix##_size(const prefix##_t *v) {\
        return vector_size(&v->v); }\
    int prefix##_cmp(const prefix##_t *v1, const prefix##_t *v2) {\
        return vector_cmp(&v1->v, &v2->v,\
                (int (*)(const void *, const void *)) compar); }\
    bool prefix##_eq(const prefix##_t *v1, const prefix##_t *v2) {\
        return vector_eq(&v1->v, &v2->v,\
                (int (*)(const void *, const void *)) compar); }\
    bool prefix##_is_prefix(const prefix##_t *v1, const prefix##_t *v2) {\
        return vector_is_prefix(&v1->v, &v2->v,\
                (int (*)(const void *, const void *)) compar); }\
    void prefix##_set(prefix##_t *v, int index, const type elem) {\
        vector_set(&v->v, index, (const void *) elem); }\
    void prefix##_prepend(prefix##_t *v, const type elem) {\
        vector_prepend(&v->v, (const void *) elem); }\
    void prefix##_append(prefix##_t *v, const type elem) {\
        vector_append(&v->v, (const void *) elem); }\
    void prefix##_insert(prefix##_t *v, int index, const type elem) {\
        vector_insert(&v->v, index, (const void *) elem); }\
    void prefix##_prepend_all(prefix##_t *v, const prefix##_t *elems) {\
        vector_prepend_all(&v->v, &elems->v); }\
    void prefix##_append_all(prefix##_t *v, const prefix##_t *elems) {\
        vector_append_all(&v->v, &elems->v); }\
    void prefix##_insert_all(prefix##_t *v,\
            int index, const prefix##_t *elems) {\
        vector_insert_all(&v->v, index, &elems->v); }\
    void prefix##_prepend_all_range(\
            prefix##_t *v, const prefix##_t *elems, int from, int to) {\
        vector_prepend_all_range(&v->v, &elems->v, from, to); }\
    void prefix##_append_all_range(prefix##_t *v,\
            const prefix##_t *elems, int from, int to) {\
        vector_append_all_range(&v->v, &elems->v, from, to); }\
    void prefix##_insert_all_range(prefix##_t *v,\
            int index, const prefix##_t *elems, int from, int to) {\
        vector_insert_all_range(&v->v, index, &elems->v, from, to); }\
    const type prefix##_remove_first(prefix##_t *v) {\
        return (const type) vector_remove_first(&v->v); }\
    const type prefix##_remove_last(prefix##_t *v) {\
        return (const type) vector_remove_last(&v->v); }\
    const type prefix##_remove(prefix##_t *v, int index) {\
        return (const type) vector_remove(&v->v, index); }\
    void prefix##_remove_range(prefix##_t *v, int from, int to) {\
        vector_remove_range(&v->v, from, to); }\
    void prefix##_remove_all(prefix##_t *v,\
            const int indices[], int n_indices) {\
        vector_remove_all(&v->v, indices, n_indices); }\
    void prefix##_clear(prefix##_t *v) {\
        vector_clear(&v->v); }\
    prefix##_iter_t prefix##_iter(prefix##_t *vec, int index) {\
        return (prefix##_iter_t) { .i = vector_iter(&vec->v, index) }; }\
    bool prefix##_iter_next(prefix##_iter_t *iter) {\
        return vector_iter_next(&iter->i); }\
    const type prefix##_iter_get(const prefix##_iter_t *iter) {\
        return (const type) vector_iter_get(&iter->i); }\
    int prefix##_iter_index(const prefix##_iter_t *iter) {\
        return vector_iter_index(&iter->i); }\
    void prefix##_iter_add_auditor(prefix##_iter_t *iter,\
            prefix##_iter_t *auditor) {\
        vector_iter_add_auditor(&iter->i, &auditor->i); }\
    void prefix##_iter_remove(prefix##_iter_t *iter) {\
        return vector_iter_remove(&iter->i); }\
    void prefix##_iter_replace(prefix##_iter_t *iter, const type new_elem) {\
        return vector_iter_replace(&iter->i, (const void *) new_elem); }\
    prefix##_const_iter_t prefix##_const_iter(const prefix##_t *vec,\
            int index) {\
        return (prefix##_const_iter_t) {\
            .i = vector_const_iter(&vec->v, index) }; }\
    bool prefix##_const_iter_next(prefix##_const_iter_t *iter) {\
        return vector_const_iter_next(&iter->i); }\
    const type prefix##_const_iter_get(const prefix##_const_iter_t *iter) {\
        return (const type) vector_const_iter_get(&iter->i); }

#endif

