/** @license 2016 Neil Edelman, distributed under the terms of the
 [MIT License](https://opensource.org/licenses/MIT).

 @subtitle Contiguous Dynamic Array (Vector)

 ![Example of Array](../web/array.png)

 <tag:<T>array> is a dynamic array that stores contiguous `ARRAY_TYPE`. When
 modifying the array, to ensure that the capacity is greater then or equal to
 the size, resizing may be necessary. This incurs amortised cost and any
 pointers to this memory may become stale.

 `<T>array` is not synchronised. Errors are returned with `errno`. The
 parameters are preprocessor macros. Assertions are used in this file; to stop
 them, define `NDEBUG` before `assert.h`.

 @param[ARRAY_NAME, ARRAY_TYPE]
 `<T>` that satisfies `C` naming conventions when mangled and a valid tag-type
 associated therewith; required. `<PT>` is private, whose names are prefixed in
 a manner to avoid collisions.

 @param[ARRAY_EXPECT_TRAIT]
 Do not un-define certain variables for subsequent inclusion in a trait.

 @param[ARRAY_TO_STRING_NAME, ARRAY_TO_STRING]
 To string trait contained in <ToString.h>; `<A>` that satisfies `C` naming
 conventions when mangled and function implementing <typedef:<PT>to_string>.
 There can be multiple to string traits, but only one can omit
 `ARRAY_TO_STRING_NAME`.

 @param[ARRAY_TEST]
 To string trait contained in <../test/ArrayTest.h>; optional unit testing
 framework using `assert`. Can only be defined once _per_ array. Must be
 defined equal to a (random) filler function, satisfying <typedef:<PT>action>.
 Output will be shown with the to string trait in which it's defined; provides
 tests for the base code and all later traits.

 @param[ARRAY_COMPARABLE_NAME, ARRAY_IS_EQUAL, ARRAY_COMPARE]
 Comparable trait; `<C>` that satisfies `C` naming conventions when mangled
 and a function implementing, for `ARRAY_IS_EQUAL` <typedef:<PT>bipredicate>
 that establishes an equivalence relation, or for `ARRAY_COMPARE`
 <typedef:<PT>compare> that establishes a total order. There can be multiple
 contrast traits, but only one can omit `ARRAY_COMPARABLE_NAME`.

 @std C89
 @cf [Heap](https://github.com/neil-edelman/Heap)
 @cf [List](https://github.com/neil-edelman/List)
 @cf [Orcish](https://github.com/neil-edelman/Orcish)
 @cf [Pool](https://github.com/neil-edelman/Pool)
 @cf [Set](https://github.com/neil-edelman/Set)
 @cf [Trie](https://github.com/neil-edelman/Trie) */

#include <stdlib.h> /* realloc free */
#include <assert.h> /* assert */
#include <string.h> /* memcpy memmove (strlen) (strerror strcpy memcmp) */
#include <limits.h> /* LONG_MAX */
#include <errno.h>  /* errno */


#if !defined(ARRAY_NAME) || !defined(ARRAY_TYPE)
#error Name ARRAY_NAME or tag type ARRAY_TYPE undefined.
#endif
#if defined(ARRAY_TO_STRING_NAME) || defined(ARRAY_TO_STRING)
#define ARRAY_TO_STRING_TRAIT 1
#else
#define ARRAY_TO_STRING_TRAIT 0
#endif
#if defined(ARRAY_COMPARABLE_NAME) || defined(ARRAY_COMPARE) \
	|| defined(ARRAY_IS_EQUAL)
#define ARRAY_COMPARABLE_TRAIT 1
#else
#define ARRAY_COMPARABLE_TRAIT 0
#endif
#define ARRAY_TRAITS ARRAY_TO_STRING_TRAIT + ARRAY_COMPARABLE_TRAIT
#if ARRAY_TRAITS > 1
#error Only one trait per include is allowed; use ARRAY_EXPECT_TRAIT.
#endif
#if (ARRAY_TRAITS == 0) && defined(ARRAY_TEST)
#error ARRAY_TEST must be defined in ARRAY_TO_STRING trait.
#endif
#if defined(ARRAY_TO_STRING_NAME) && !defined(ARRAY_TO_STRING)
#error ARRAY_TO_STRING_NAME requires ARRAY_TO_STRING.
#endif
#if defined(ARRAY_COMPARABLE_NAME) \
	&& ((!defined(ARRAY_COMPARE) && !defined(ARRAY_IS_EQUAL)) || \
	(defined(ARRAY_COMPARE) && defined(ARRAY_IS_EQUAL)))
#error ARRAY_COMPARABLE_NAME requires ARRAY_COMPARE or ARRAY_IS_EQUAL not both.
#endif


#if ARRAY_TRAITS == 0 /* <!-- base code */


/* <Kernighan and Ritchie, 1988, p. 231>. */
#if defined(T_) || defined(PT_) \
	|| (defined(ARRAY_SUBTYPE) ^ (defined(CAT) || defined(CAT_)))
#error Unexpected P?T_ or CAT_?; possible stray ARRAY_EXPECT_TRAIT?
#endif
#ifndef ARRAY_SUBTYPE /* <!-- !sub-type */
#define CAT_(x, y) x ## _ ## y
#define CAT(x, y) CAT_(x, y)
#endif /* !sub-type --> */
#define T_(thing) CAT(ARRAY_NAME, thing)
#define PT_(thing) CAT(array, T_(thing))

/** A valid tag type set by `ARRAY_TYPE`. */
typedef ARRAY_TYPE PT_(type);

/** Operates by side-effects. */
typedef void (*PT_(action))(PT_(type) *);

/** Returns a boolean given read-only `<T>`. */
typedef int (*PT_(predicate))(const PT_(type) *);

/** Returns a boolean given two read-only `<T>`. */
typedef int (*PT_(bipredicate))(const PT_(type) *, const PT_(type) *);

/** Returns a boolean given two `<T>`. */
typedef int (*PT_(biproject))(PT_(type) *, PT_(type) *);

/** Three-way comparison on a totally order set; returns an integer value less
 then, equal to, greater then zero, if `a < b`, `a == b`, `a > b`,
 respectively. */
typedef int (*PT_(compare))(const PT_(type) *a, const PT_(type) *b);

/** Responsible for turning the first argument into a 12-`char` null-terminated
 output string. Used for `ARRAY_TO_STRING`. */
typedef void (*PT_(to_string))(const PT_(type) *, char (*)[12]);

/** Manages the array field `data`, which is indexed up to `size`. To
 initialise it to an idle state, see <fn:<T>array>, `ARRAY_IDLE`, `{0}`
 (`C99`,) or being `static`.

 ![States.](../web/states.png) */
struct T_(array);
/* !data -> !size, data -> capacity >= min && size <= capacity <= max */
struct T_(array) { PT_(type) *data; size_t size, capacity; };
#ifndef ARRAY_IDLE /* <!-- !zero; `{0}` is `C99`. */
#define ARRAY_IDLE { 0, 0, 0 }
#endif /* !zero --> */

/** Initialises `a` to idle. @order \Theta(1) @allow */
static void T_(array)(struct T_(array) *const a)
	{ assert(a), a->data = 0, a->capacity = a->size = 0; }

/** Destroys `a` and returns it to idle. @allow */
static void T_(array_)(struct T_(array) *const a)
	{ assert(a), free(a->data), T_(array)(a); }

/** Ensures `min_capacity` of `a`. @param[min_capacity] If zero, does nothing.
 @return Success; otherwise, `errno` will be set. @throws[ERANGE] Tried
 allocating more then can fit in `size_t` or `realloc` doesn't follow [POSIX
 ](https://pubs.opengroup.org/onlinepubs/009695399/functions/realloc.html).
 @throws[realloc] @allow */
static int T_(array_reserve)(struct T_(array) *const a,
	const size_t min_capacity) {
	size_t c0;
	PT_(type) *data;
	const size_t max_size = (size_t)-1 / sizeof *a->data;
	assert(a);
	if(a->data) {
		if(min_capacity <= a->capacity) return 1;
		c0 = a->capacity;
		if(c0 < 8) c0 = 8;
	} else { /* Idle. */
		if(!min_capacity) return 1;
		c0 = 8;
	}
	if(min_capacity > max_size) return errno = ERANGE, 0;
	/* `c_n = a1.625^n`, approximation golden ratio `\phi ~ 1.618`. */
	while(c0 < min_capacity) {
		size_t c1 = c0 + (c0 >> 1) + (c0 >> 3);
		if(c0 >= c1) { c0 = max_size; break; } /* Overflow; unlikely. */
		c0 = c1;
	}
	if(!(data = realloc(a->data, sizeof *a->data * c0)))
		{ if(!errno) errno = ERANGE; return 0; }
	a->data = data, a->capacity = c0;
	return 1;
}

/** Adds `buffer` un-initialised elements at the back of `a`.
 @param[buffer] If zero, returns null.
 @return A pointer to previous end of `a`, where `buffer` objects are, or null
 and `errno` will be set for `buffer != 0`. @throws[realloc, ERANGE] @allow */
static PT_(type) *T_(array_buffer)(struct T_(array) *const a,
	const size_t buffer) {
	size_t prev_size;
	assert(a);
	if(!buffer) return 0;
	if(a->size > (size_t)-1 - buffer) /* `size_t` overflow; unlikely */
		{ errno = ERANGE; return 0; }
	if(!T_(array_reserve)(a, a->size + buffer)) return 0;
	prev_size = a->size, a->size += buffer;
	assert(a->data);
	return a->data + prev_size;
}

/** @return A new un-initialized datum of `a`.
 @throws[realloc, ERANGE] @allow */
static PT_(type) *T_(array_new)(struct T_(array) *const a)
	{ return T_(array_buffer)(a, 1); }

/** Returns a new un-initialised datum of `a` and updates `update_ptr`, which
 must be in `a`. @throws[realloc, ERANGE] @allow */
static PT_(type) *T_(array_update_new)(struct T_(array) *const a,
	PT_(type) **const update_ptr) {
	PT_(type) *n;
	size_t idx = *update_ptr - a->data;
	assert(a && a->data && update_ptr
		&& *update_ptr >= a->data && *update_ptr <= a->data + a->capacity);
	if(!(n = T_(array_new)(a))) return 0;
	*update_ptr = a->data + idx;
	return n;
}

/** Shrinks the capacity `a` to the size, freeing unsed memory. If the size is
 zero, it will be in an idle state.
 @return Success. @throws[ERANGE, realloc] Unlikely `realloc` error. */
static int T_(array_shrink)(struct T_(array) *const a) {
	PT_(type) *data;
	assert(a && a->capacity >= a->size);
	if(!a->data) return assert(!a->size), 1;
	if(!(data = realloc(a->data, sizeof *a->data * a->size)))
		{ if(!errno) errno = ERANGE; return 0; } /* Unlikely. */
	a->data = data, a->capacity = a->size;
	return 1;
}

/** Removes `datum` from `a`. @order \O(n). @allow */
static void T_(array_remove)(struct T_(array) *const a,
	PT_(type) *const datum) {
	const size_t n = datum - a->data;
	assert(a && datum && datum >= a->data && datum < a->data + a->size);
	memmove(datum, datum + 1, sizeof *datum * (--a->size - n));
}

/** Removes `datum` from `a` and replaces it with the tail.
 @order \O(1). @allow */
static void T_(array_lazy_remove)(struct T_(array) *const a,
	PT_(type) *const datum) {
	size_t n = datum - a->data;
	assert(a && datum && datum >= a->data && datum < a->data + a->size);
	if(--a->size != n) memcpy(datum, a->data + a->size, sizeof *datum);
}

/** Sets `a` to be empty. That is, the size of `a` will be zero, but if it was
 previously in an active non-idle state, it continues to be.
 @order \Theta(1) @allow */
static void T_(array_clear)(struct T_(array) *const a)
	{ assert(a), a->size = 0; }

/** @return The last element or null if `a` is empty. @order \Theta(1) @allow */
static PT_(type) *T_(array_peek)(const struct T_(array) *const a)
	{ return assert(a), a->size ? a->data + a->size - 1 : 0; }

/** @return Value from the the top of `a` that is removed or null if the array
 is empty. @order \Theta(1) @allow */
static PT_(type) *T_(array_pop)(struct T_(array) *const a)
	{ return assert(a), a->size ? a->data + --a->size : 0; }

/** `a` indices [`i0`, `i1`) will be replaced with a copy of `b`.
 @return Success. @throws[realloc, ERANGE] */
static int T_(array_splice)(struct T_(array) *const a, const size_t i0,
	const size_t i1, const struct T_(array) *const b) {
	const size_t a_range = i1 - i0, b_range = b ? b->size : 0;
	assert(a && a != b && i0 <= i1 && i1 <= a->size);
	if(a_range < b_range) { /* The output is bigger. */
		const size_t diff = b_range - a_range;
		if(a->size > (size_t)-1 - diff) return errno = ERANGE, 0;
		if(!T_(array_reserve)(a, a->size + diff)) return 0;
		memmove(a->data + i1 + diff, a->data + i1,
			(a->size - i1) * sizeof *a->data);
		a->size += diff;
	} else if(b_range < a_range) { /* The output is smaller. */
		memmove(a->data + i0 + b_range, a->data + i1,
			(a->size - i1) * sizeof *a->data);
		a->size -= a_range - b_range;
	}
	if(b) memcpy(a->data + i0, b->data, b->size * sizeof *a->data);
	return 1;
}

/** @return Converts `i` to an index in `a` from [0, `a.size`]. Negative values
 are implicitly plus `a.size`. @order \Theta(1) @allow */
static size_t T_(array_clip)(const struct T_(array) *const a, const long i) {
	/* `SIZE_MAX` is `C99`; assumes two's-compliment, which is the only
	 sequential ring representation with 000 = 0. I haven't tested `PDP-11`. */
	assert(a && (size_t)-1 >= (size_t)LONG_MAX
		&& (unsigned long)((size_t)-1) >= LONG_MAX);
	return i < 0
		? (size_t)-i >= a->size ? 0 : a->size + i
		: (size_t)i > a->size ? a->size : (size_t)i;
}

/** For all elements of `a`, calls `keep`, and for each element, if the return
 value is false, lazy deletes that item, calling `destruct` if not-null.
 @order \O(`a.size` \times `keep` \times `destruct`) @allow */
static void T_(array_keep_if)(struct T_(array) *const a,
	const PT_(predicate) keep, const PT_(action) destruct) {
	PT_(type) *erase = 0, *t;
	const PT_(type) *retain = 0, *end;
	int keep0 = 1, keep1 = 0;
	assert(a && keep);
	for(t = a->data, end = a->data + a->size; t < end; keep0 = keep1, t++) {
		if(!(keep1 = !!keep(t)) && destruct) destruct(t);
			if(!(keep0 ^ keep1)) continue; /* Not a falling/rising edge. */
		if(keep1) { /* Rising edge. */
			assert(erase && !retain);
			retain = t;
		} else if(erase) { /* Falling edge. */
			size_t n = t - retain;
			assert(erase < retain && retain < t);
			memmove(erase, retain, n * sizeof *t);
			erase += n;
			retain = 0;
		} else { /* Falling edge, (first time only.) */
			erase = t;
		}
	}
	if(!erase) return; /* All elements were kept. */
	if(keep1) { /* Delayed move when the iteration ended; repeat. */
		size_t n = t - retain;
		assert(retain && erase < retain && retain < t);
		memmove(erase, retain, n * sizeof *t);
		erase += n;
	}
	/* Adjust the size. */
	assert((size_t)(erase - a->data) <= a->size);
	a->size = erase - a->data;
}

/** Removes at either end of `a` of things that `predicate` returns true.
 @order \O(`a.size` \times `predicate`) @allow */
static void T_(array_trim)(struct T_(array) *const a,
	const PT_(predicate) predicate) {
	size_t i;
	assert(a && predicate);
	while(a->size && predicate(a->data + a->size - 1)) a->size--;
	for(i = 0; i < a->size && predicate(a->data + i); i++);
	if(!i) return;
	assert(i < a->size);
	memmove(a->data, a->data + i, sizeof *a->data * i), a->size -= i;
}

/** Iterates through `a` and calls `action` on all the elements. The topology
 of the list should not change while in this function.
 @order \O(`a.size` \times `action`) @allow */
static void T_(array_each)(struct T_(array) *const a,
	const PT_(action) action) {
	PT_(type) *i, *i_end;
	assert(a && action);
	for(i = a->data, i_end = i + a->size; i < i_end; i++) action(i);
}

/** Iterates through `a` and calls `action` on all the elements for which
 `predicate` returns true. The topology of the list should not change while in
 this function. @order \O(`a.size` \times `predicate` \times `action`) @allow */
static void T_(array_if_each)(struct T_(array) *const a,
	const PT_(predicate) predicate, const PT_(action) action) {
	PT_(type) *i, *i_end;
	assert(a && predicate && action);
	for(i = a->data, i_end = i + a->size; i < i_end; i++)
		if(predicate(i)) action(i);
}

/** Iterates through `a` and calls `predicate` until it returns true.
 @return The first `predicate` that returned true, or, if the statement is
 false on all, null. @order \O(`a.size` \times `predicate`) @allow */
static PT_(type) *T_(array_any)(const struct T_(array) *const a,
	const PT_(predicate) predicate) {
	PT_(type) *i, *i_end;
	if(!a || !predicate) return 0;
	for(i = a->data, i_end = i + a->size; i < i_end; i++)
		if(predicate(i)) return i;
	return 0;
}

/** Contains all iteration parameters in one for iteration in traits. */
struct PT_(iterator);
struct PT_(iterator) { const struct T_(array) *a; size_t i; };
/** Advances `it`. */
static PT_(type) *PT_(next)(struct PT_(iterator) *const it) {
	assert(it && it->a);
	return it->i < it->a->size ? it->a->data + it->i++ : 0;
}
#define ITERATE_TYPE PT_(type)
#define ITERATE_ITERATOR PT_(iterator)
#define ITERATE_NEXT &PT_(next)

static void PT_(unused_base_coda)(void);
static void PT_(unused_base)(void) {
	T_(array)(0); T_(array_update_new)(0, 0); T_(array_shrink)(0);
	T_(array_remove)(0, 0); T_(array_lazy_remove)(0, 0); T_(array_clear)(0);
	T_(array_peek)(0); T_(array_pop)(0); T_(array_splice)(0, 0, 0, 0);
	T_(array_clip)(0, 0); T_(array_keep_if)(0, 0, 0); T_(array_trim)(0, 0);
	T_(array_each)(0, 0); T_(array_if_each)(0, 0, 0); T_(array_any)(0, 0);
	PT_(next)(0); PT_(unused_base_coda)();
}
static void PT_(unused_base_coda)(void) { PT_(unused_base)(); }


#elif defined(ARRAY_TO_STRING) /* base code --><!-- to string trait */


#if !defined(T_) || !defined(PT_) || !defined(CAT) || !defined(CAT_)
#error P?T_ or CAT_? not yet defined; traits must be defined separately?
#endif

#ifdef ARRAY_TO_STRING_NAME /* <!-- name */
#define PTA_(thing) CAT(PT_(thing), ARRAY_TO_STRING_NAME)
#define T_A_(thing1, thing2) CAT(T_(thing1), CAT(ARRAY_TO_STRING_NAME, thing2))
#else /* name --><!-- !name */
#define PTA_(thing) CAT(PT_(thing), anonymous)
#define T_A_(thing1, thing2) CAT(T_(thing1), thing2)
#endif /* !name --> */

/* Check that `ARRAY_TO_STRING` is a function implementing
 <typedef:<PT>to_string>. */
static const PT_(to_string) PTA_(to_str12) = (ARRAY_TO_STRING);

/** Writes `it` to `str` and advances or returns false.
 @implements <A>next_to_string */
static int PTA_(next_to_str12)(struct PT_(iterator) *const it,
	char (*const str)[12]) {
	assert(it && str);
	if(!it->a || it->i >= it->a->size) return 0;
	PTA_(to_str12)(it->a->data + it->i++, str);
	return 1;
}

#define A_ PTA_
#define TO_STRING_ITERATOR struct PT_(iterator)
#define TO_STRING_NEXT &PTA_(next_to_str12)
#include "ToString.h"

/** @return Print the contents of `a` in a static string buffer with the
 limitations of `ToString.h`. @order \Theta(1) @allow */
static const char *T_A_(array, to_string)(const struct T_(array) *const a) {
	struct PT_(iterator) it = { 0, 0 };
	it.a = a;
	return PTA_(iterator_to_string)(&it, '(', ')'); /* In ToString. */
}

static void PTA_(unused_to_string_coda)(void);
static void PTA_(unused_to_string)(void)
	{ T_A_(array, to_string)(0); PTA_(unused_to_string_coda)(); }
static void PTA_(unused_to_string_coda)(void) { PTA_(unused_to_string)(); }

#if !defined(ARRAY_TEST_BASE) && defined(ARRAY_TEST) /* <!-- test */
#define ARRAY_TEST_BASE /* Only one instance of base tests. */
#include "../test/TestArray.h"
#endif /* test --> */

#undef PTA_
#undef T_A_
#undef ARRAY_TO_STRING
#ifdef ARRAY_TO_STRING_NAME
#undef ARRAY_TO_STRING_NAME
#endif


#else /* to string trait --><!-- comparable trait */


#if !defined(T_) || !defined(PT_) || !defined(CAT) || !defined(CAT_)
#error P?T_ or CAT_? not yet defined; traits must be defined separately?
#endif

#ifdef ARRAY_COMPARABLE_NAME /* <!-- name */
#define PTC_(thing) CAT(PT_(thing), ARRAY_COMPARABLE_NAME)
#define T_C_(thing1, thing2) CAT(T_(thing1), CAT(ARRAY_COMPARABLE_NAME, thing2))
#else /* name --><!-- !name */
#define PTC_(thing) CAT(PT_(thing), anonymous)
#define T_C_(thing1, thing2) CAT(T_(thing1), thing2)
#endif /* !name --> */

#ifdef ARRAY_COMPARE /* <!-- compare */

/* Check that `ARRAY_COMPARE` is a function implementing
 <typedef:<PT>Compare>. */
static const PT_(compare) PTC_(compare) = (ARRAY_COMPARE);

/* fixme: static int T_C_(array, compare)() */

/** `a` should be partitioned true/false with less-then `value`.
 @return The first index of `a` that is not less then `value`.
 @order \O(log `a.size`) @allow */
static size_t T_C_(array, lower_bound)(const struct T_(array) *const a,
	const PT_(type) *const value) {
	size_t low = 0, high = a->size, mid;
	assert(a && value);
	while(low < high) if(PTC_(compare)(value, a->data
		+ (mid = low + ((high - low) >> 1))) <= 0) high = mid;
		else low = mid + 1;
	return low;
}

/** `a` should be partitioned false/true with greater-than or equals `value`.
 @return The first index of `a` that is greater then `value`.
 @order \O(log `a.size`) @allow */
static size_t T_C_(array, upper_bound)(const struct T_(array) *const a,
	const PT_(type) *const value) {
	size_t low = 0, high = a->size, mid;
	assert(a && value);
	while(low < high) if(PTC_(compare)(value, a->data
		+ (mid = low + ((high - low) >> 1))) >= 0) low = mid + 1;
		else high = mid;
	return low;
}	
	
/** Copies `datum` at the lower bound of a sorted `a`.
 @return Success. @order \O(`a.size`) @throws[realloc, ERANGE] */
static int T_C_(array, insert)(struct T_(array) *const a,
	const PT_(type) *const datum) {
	size_t bound;
	assert(a && datum);
	bound = T_C_(array, lower_bound)(a, datum);
	if(!T_(array_new)(a)) return 0;
	memmove(a->data + bound + 1, a->data + bound,
		sizeof *a->data * (a->size - bound - 1));
	memcpy(a->data + bound, datum, sizeof *datum);
	return 1;
}

/** Wrapper with void `a` and `b`. @implements qsort bsearch */
static int PTC_(vcompar)(const void *const a, const void *const b)
	{ return PTC_(compare)(a, b); }

/** Sorts `a` by `qsort` on `ARRAY_COMPARE`.
 @order \O(`a.size` \log `a.size`) @allow */
static void T_C_(array, sort)(struct T_(array) *const a)
	{ assert(a), qsort(a->data, a->size, sizeof *a->data, PTC_(vcompar)); }

/** Wrapper with void `a` and `b`. @implements qsort bsearch */
static int PTC_(vrevers)(const void *const a, const void *const b)
	{ return PTC_(compare)(b, a); }	

/** Sorts `a` in reverse by `qsort` on `ARRAY_COMPARE`.
 @order \O(`a.size` \log `a.size`) @allow */
static void T_C_(array, reverse)(struct T_(array) *const a)
	{ assert(a), qsort(a->data, a->size, sizeof *a->data, PTC_(vrevers)); }

/** !compare(`a`, `b`) == equals(`a`, `b`) for not `ARRAY_IS_EQUAL`.
 @implements <PT>bipredicate */
static int PTC_(is_equal)(const void *const a, const void *const b)
	{ return !PTC_(compare)(a, b); }
	
#else /* compare --><!-- is equal */

/* Check that `ARRAY_IS_EQUAL` is a function implementing
 <typedef:<PT>bipredicate>. */
static const PT_(bipredicate) PTC_(is_equal) = (ARRAY_IS_EQUAL);

#endif /* is equal --> */

/* fixme: static int T_C_(array, is_equal)() */

/** Tests equality for each consecutive pair of elements in `a` and, if
 true, surjects two one according to `merge`.
 @param[merge] Can be null, in which case, all duplicate entries are erased.
 @order \O(`a.size`) @allow */
static void T_C_(array, compactify)(struct T_(array) *const a,
	const PT_(biproject) merge) {
	size_t target, from, cursor, choice, next, move;
	const size_t last = a->size;
	int is_first, is_last;
	assert(a);
	for(target = from = cursor = 0; cursor < last; cursor += next) {
		/* Bijective `[from, cursor)` is moved lazily. */
		for(choice = 0, next = 1; cursor + next < last && PTC_(is_equal)(a->data
			+ cursor + choice, a->data + cursor + next); next++)
			if(merge && merge(a->data + choice, a->data + next)) choice = next;
		if(next == 1) continue;
		/* Must move injective `cursor + choice \in [cursor, cursor + next)`. */
		is_first = !choice;
		is_last  = (choice == next - 1);
		move = cursor - from + is_first;
		memmove(a->data + target, a->data + from, sizeof *a->data * move),
		target += move;
		if(!is_first && !is_last) memcpy(a->data + target,
			a->data + cursor + choice, sizeof *a->data), target++;
		from = cursor + next - is_last;
	}
	/* Last differed move. */
	move = last - from;
	memmove(a->data + target, a->data + from, sizeof *a->data * move),
	target += move, assert(a->size >= target);
	a->size = target;
}	

static void PTC_(unused_contrast_coda)(void);
static void PTC_(unused_contrast)(void) {
#ifdef ARRAY_COMPARE /* <!-- compare */
	T_C_(array, lower_bound)(0, 0); T_C_(array, upper_bound)(0, 0);
	T_C_(array, insert)(0, 0); T_C_(array, sort)(0); T_C_(array, reverse)(0);
#endif /* compare --> */
	T_C_(array, compactify)(0, 0); PTC_(unused_contrast_coda)();
}
static void PTC_(unused_contrast_coda)(void) { PTC_(unused_contrast)(); }

#if defined(ARRAY_TEST_BASE) && defined(ARRAY_TEST) /* <!-- test */
#include "../test/TestArray.h" /** \include */
#endif /* test --> */

#undef PTC_
#undef T_C_
#ifdef ARRAY_COMPARE
#undef ARRAY_COMPARE
#endif
#ifdef ARRAY_IS_EQUAL
#undef ARRAY_IS_EQUAL
#endif
#ifdef ARRAY_COMPARABLE_NAME
#undef ARRAY_COMPARABLE_NAME
#endif


#endif /* traits --> */


#ifdef ARRAY_EXPECT_TRAIT /* <!-- trait */
#undef ARRAY_EXPECT_TRAIT
#else /* trait --><!-- !trait */
#ifndef ARRAY_SUBTYPE /* <!-- !sub-type */
#undef CAT
#undef CAT_
#else /* !sub-type --><!-- sub-type */
#undef ARRAY_SUBTYPE
#endif /* sub-type --> */
#undef T_
#undef PT_
#undef ARRAY_NAME
#undef ARRAY_TYPE
#ifdef ARRAY_TEST
#undef ARRAY_TEST
#endif
#ifdef ARRAY_TEST_BASE
#undef ARRAY_TEST_BASE
#endif
#undef ITERATE_TYPE
#undef ITERATE_ITERATOR
#undef ITERATE_NEXT
#endif /* !trait --> */

#undef ARRAY_TO_STRING_TRAIT
#undef ARRAY_COMPARABLE_TRAIT
#undef ARRAY_TRAITS
