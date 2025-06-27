#include "solver.h"

#define SOLVER_IMPLEMENTATION
#define flint_solver_implemented


#include <assert.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define FLINT_SOLVER_EXTERNAL     (0)
#define FLINT_SOLVER_SLACK        (1)
#define FLINT_SOLVER_ERROR        (2)
#define FLINT_SOLVER_DUMMY        (3)

#define flint_solver_isexternal(key)   ((key).type == FLINT_SOLVER_EXTERNAL)
#define flint_solver_isslack(key)      ((key).type == FLINT_SOLVER_SLACK)
#define flint_solver_iserror(key)      ((key).type == FLINT_SOLVER_ERROR)
#define flint_solver_isdummy(key)      ((key).type == FLINT_SOLVER_DUMMY)
#define flint_solver_ispivotable(key)  (flint_solver_isslack(key) || flint_solver_iserror(key))

#define FLINT_SOLVER_POOLSIZE     4096
#define FLINT_SOLVER_MIN_HASHSIZE 4
#define FLINT_SOLVER_MAX_SIZET    ((~(size_t)0)-100)

#define FLINT_SOLVER_UNSIGNED_BITS (sizeof(unsigned)*CHAR_BIT)

#ifdef AM_USE_FLOAT
# define FLINT_SOLVER_NUM_MAX FLT_MAX
# define FLINT_SOLVER_NUM_EPS 1e-4f
#else
# define FLINT_SOLVER_NUM_MAX DBL_MAX
# define FLINT_SOLVER_NUM_EPS 1e-6
#endif

SOLVER_NS_BEGIN


typedef struct flint_solver_Symbol {
    unsigned id   : FLINT_SOLVER_UNSIGNED_BITS - 2;
    unsigned type : 2;
} flint_solver_Symbol;

typedef struct flint_solver_MemPool {
    size_t size;
    void  *freed;
    void  *pages;
} flint_solver_MemPool;

typedef struct flint_solver_Entry {
    int       next;
    flint_solver_Symbol key;
} flint_solver_Entry;

typedef struct flint_solver_Table {
    size_t    size;
    size_t    count;
    size_t    entry_size;
    size_t    lastfree;
    flint_solver_Entry *hash;
} flint_solver_Table;

typedef struct flint_solver_Iterator {
    const flint_solver_Table *t;
    const flint_solver_Entry *entry;
} flint_solver_Iterator;

typedef struct flint_solver_VarEntry {
    flint_solver_Entry entry;
    struct flint_solver_Var  *var;
} flint_solver_VarEntry;

typedef struct flint_solver_ConsEntry {
    flint_solver_Entry       entry;
    struct flint_solver_Constraint *constraint;
} flint_solver_ConsEntry;

typedef struct flint_solver_Term {
    flint_solver_Entry entry;
    flint_solver_Num   multiplier;
} flint_solver_Term;

typedef struct flint_solver_Row {
    flint_solver_Entry  entry;
    flint_solver_Symbol infeasible_next;
    flint_solver_Table  terms;
    flint_solver_Num    constant;
} flint_solver_Row;

struct flint_solver_Var {
    flint_solver_Symbol      sym;
    unsigned       refcount : FLINT_SOLVER_UNSIGNED_BITS - 1;
    unsigned       dirty    : 1;
    struct flint_solver_Var        *next;
    struct flint_solver_Solver     *solver;
    struct flint_solver_Constraint *constraint;
    flint_solver_Num         edit_value;
    flint_solver_Num         value;
};

struct flint_solver_Constraint {
    flint_solver_Row     expression;
    flint_solver_Symbol  marker;
    flint_solver_Symbol  other;
    int        relation;
    struct flint_solver_Solver *solver;
    flint_solver_Num     strength;
};

struct flint_solver_Solver {
    flint_solver_Allocf *allocf;
    void      *ud;
    flint_solver_Row     objective;
    flint_solver_Table   vars;            /* symbol -> VarEntry */
    flint_solver_Table   constraints;     /* symbol -> ConsEntry */
    flint_solver_Table   rows;            /* symbol -> Row */
    flint_solver_MemPool varpool;
    flint_solver_MemPool conspool;
    unsigned   symbol_count;
    unsigned   constraint_count;
    unsigned   auto_update;
    flint_solver_Symbol  infeasible_rows;
    struct flint_solver_Var    *dirty_vars;
};


/* utils */

static flint_solver_Symbol flint_solver_newsymbol(struct flint_solver_Solver *solver, int type);

static int flint_solver_approx(flint_solver_Num a, flint_solver_Num b)
{ return a > b ? a - b < FLINT_SOLVER_NUM_EPS : b - a < FLINT_SOLVER_NUM_EPS; }

static int flint_solver_nearzero(flint_solver_Num a)
{ return flint_solver_approx(a, 0.0f); }

static flint_solver_Symbol flint_solver_null(void)
{ flint_solver_Symbol null = { 0, 0 }; return null; }

static void flint_solver_initsymbol(struct flint_solver_Solver *solver, flint_solver_Symbol *sym, int type)
{ if (sym->id == 0) *sym = flint_solver_newsymbol(solver, type); }

static void flint_solver_initpool(flint_solver_MemPool *pool, size_t size) {
    pool->size  = size;
    pool->freed = pool->pages = NULL;
    assert(size > sizeof(void*) && size < FLINT_SOLVER_POOLSIZE/4);
}

static void flint_solver_freepool(struct flint_solver_Solver *solver, flint_solver_MemPool *pool) {
    const size_t offset = FLINT_SOLVER_POOLSIZE - sizeof(void*);
    while (pool->pages != NULL) {
        void *next = *(void**)((char*)pool->pages + offset);
        solver->allocf(solver->ud, pool->pages, 0, FLINT_SOLVER_POOLSIZE);
        pool->pages = next;
    }
    flint_solver_initpool(pool, pool->size);
}

static void *flint_solver_alloc(struct flint_solver_Solver *solver, flint_solver_MemPool *pool) {
    void *obj = pool->freed;
    if (obj == NULL) {
        const size_t offset = FLINT_SOLVER_POOLSIZE - sizeof(void*);
        void *end, *newpage = solver->allocf(solver->ud, NULL, FLINT_SOLVER_POOLSIZE, 0);
        *(void**)((char*)newpage + offset) = pool->pages;
        pool->pages = newpage;
        end = (char*)newpage + (offset/pool->size-1)*pool->size;
        while (end != newpage) {
            *(void**)end = pool->freed;
            pool->freed = (void**)end;
            end = (char*)end - pool->size;
        }
        return end;
    }
    pool->freed = *(void**)obj;
    return obj;
}

static void flint_solver_free(flint_solver_MemPool *pool, void *obj) {
    *(void**)obj = pool->freed;
    pool->freed = obj;
}

static flint_solver_Symbol flint_solver_newsymbol(struct flint_solver_Solver *solver, int type) {
    flint_solver_Symbol sym;
    unsigned id = ++solver->symbol_count;
    if (id > 0x3FFFFFFF) id = solver->symbol_count = 1;
    assert(type >= FLINT_SOLVER_EXTERNAL && type <= FLINT_SOLVER_DUMMY);
    sym.id   = id;
    sym.type = type;
    return sym;
}


/* hash table */

#define flint_solver_key(entry) (((flint_solver_Entry*)(entry))->key)

#define flint_solver_offset(lhs,rhs) ((int)((char*)(lhs) - (char*)(rhs)))
#define flint_solver_index(h,i)      ((flint_solver_Entry*)((char*)(h) + (i)))

static flint_solver_Entry *flint_solver_newkey(struct flint_solver_Solver *solver, flint_solver_Table *t, flint_solver_Symbol key);

static void flint_solver_delkey(flint_solver_Table *t, flint_solver_Entry *entry)
{ entry->key = flint_solver_null(), --t->count; }

static void flint_solver_inittable(flint_solver_Table *t, size_t entry_size)
{ memset(t, 0, sizeof(*t)), t->entry_size = entry_size; }

static flint_solver_Entry *flint_solver_mainposition(const flint_solver_Table *t, flint_solver_Symbol key)
{ return flint_solver_index(t->hash, (key.id & (t->size - 1))*t->entry_size); }

static void flint_solver_resettable(flint_solver_Table *t)
{ t->count = 0; memset(t->hash, 0, t->lastfree = t->size * t->entry_size); }

static size_t flint_solver_hashsize(flint_solver_Table *t, size_t len) {
    size_t newsize = FLINT_SOLVER_MIN_HASHSIZE;
    const size_t max_size = (FLINT_SOLVER_MAX_SIZET / 2) / t->entry_size;
    while (newsize < max_size && newsize < len)
        newsize <<= 1;
    assert((newsize & (newsize - 1)) == 0);
    return newsize < len ? 0 : newsize;
}

static void flint_solver_freetable(struct flint_solver_Solver *solver, flint_solver_Table *t) {
    size_t size = t->size*t->entry_size;
    if (size) solver->allocf(solver->ud, t->hash, 0, size);
    flint_solver_inittable(t, t->entry_size);
}

static size_t flint_solver_resizetable(struct flint_solver_Solver *solver, flint_solver_Table *t, size_t len) {
    size_t i, oldsize = t->size * t->entry_size;
    flint_solver_Table nt = *t;
    nt.size = flint_solver_hashsize(t, len);
    nt.lastfree = nt.size*nt.entry_size;
    nt.hash = (flint_solver_Entry*)solver->allocf(solver->ud, NULL, nt.lastfree, 0);
    memset(nt.hash, 0, nt.size*nt.entry_size);
    for (i = 0; i < oldsize; i += nt.entry_size) {
        flint_solver_Entry *e = flint_solver_index(t->hash, i);
        if (e->key.id != 0) {
            flint_solver_Entry *ne = flint_solver_newkey(solver, &nt, e->key);
            if (t->entry_size > sizeof(flint_solver_Entry))
                memcpy(ne + 1, e + 1, t->entry_size-sizeof(flint_solver_Entry));
        }
    }
    if (oldsize) solver->allocf(solver->ud, t->hash, 0, oldsize);
    *t = nt;
    return t->size;
}

static flint_solver_Entry *flint_solver_newkey(struct flint_solver_Solver *solver, flint_solver_Table *t, flint_solver_Symbol key) {
    if (t->size == 0) flint_solver_resizetable(solver, t, FLINT_SOLVER_MIN_HASHSIZE);
    for (;;) {
        flint_solver_Entry *mp = flint_solver_mainposition(t, key);
        if (mp->key.id != 0) {
            flint_solver_Entry *f = NULL, *othern;
            while (t->lastfree > 0) {
                flint_solver_Entry *e = flint_solver_index(t->hash, t->lastfree -= t->entry_size);
                if (e->key.id == 0 && e->next == 0)  { f = e; break; }
            }
            if (!f) { flint_solver_resizetable(solver, t, t->count*2); continue; }
            assert(f->key.id == 0);
            othern = flint_solver_mainposition(t, mp->key);
            if (othern != mp) {
                flint_solver_Entry *next;
                while ((next = flint_solver_index(othern, othern->next)) != mp)
                    othern = next;
                othern->next = flint_solver_offset(f, othern);
                memcpy(f, mp, t->entry_size);
                if (mp->next) f->next += flint_solver_offset(mp, f), mp->next = 0;
            } else {
                if (mp->next != 0)
                    f->next = flint_solver_offset(mp, f) + mp->next;
                else
                    assert(f->next == 0);
                mp->next = flint_solver_offset(f, mp), mp = f;
            }
        }
        mp->key = key;
        return mp;
    }
}

static const flint_solver_Entry *flint_solver_gettable(const flint_solver_Table *t, flint_solver_Symbol key) {
    const flint_solver_Entry *e;
    if (t->size == 0 || key.id == 0) return NULL;
    e = flint_solver_mainposition(t, key);
    for (; e->key.id != key.id; e = flint_solver_index(e, e->next))
        if (e->next == 0) return NULL;
    return e;
}

static flint_solver_Entry *flint_solver_settable(struct flint_solver_Solver *solver, flint_solver_Table *t, flint_solver_Symbol key) {
    flint_solver_Entry *e;
    assert(key.id != 0 && flint_solver_gettable(t, key) == NULL);
    e = flint_solver_newkey(solver, t, key);
    ++t->count;
    return e;
}

static flint_solver_Iterator flint_solver_itertable(const flint_solver_Table *t) {
    flint_solver_Iterator it;
    it.t = t;
    it.entry = NULL;
    return it;
}

static const flint_solver_Entry *flint_solver_nextentry(flint_solver_Iterator *it) {
    const flint_solver_Table *t = it->t;
    const flint_solver_Entry *end = flint_solver_index(t->hash, t->size*t->entry_size);
    const flint_solver_Entry *e = it->entry;
    e = e ? flint_solver_index(e, t->entry_size) : t->hash;
    for (; e < end; e = flint_solver_index(e, t->entry_size))
        if (e->key.id != 0) return it->entry = e;
    return it->entry = e;
}

/* expression (row) */

static int flint_solver_isconstant(flint_solver_Row *row)
{ return row->terms.count == 0; }

static void flint_solver_freerow(struct flint_solver_Solver *solver, flint_solver_Row *row)
{ flint_solver_freetable(solver, &row->terms); }

static void flint_solver_resetrow(flint_solver_Row *row)
{ row->constant = 0.0f; flint_solver_resettable(&row->terms); }

static void flint_solver_initrow(flint_solver_Row *row) {
    flint_solver_key(row) = flint_solver_null();
    row->infeasible_next = flint_solver_null();
    row->constant = 0.0f;
    flint_solver_inittable(&row->terms, sizeof(flint_solver_Term));
}

static void flint_solver_multiply(flint_solver_Row *row, flint_solver_Num multiplier) {
    flint_solver_Iterator it = flint_solver_itertable(&row->terms);
    row->constant *= multiplier;
    while (flint_solver_nextentry(&it))
        ((flint_solver_Term*)it.entry)->multiplier *= multiplier;
}

static void flint_solver_addvar(struct flint_solver_Solver *solver, flint_solver_Row *row, flint_solver_Symbol sym, flint_solver_Num value) {
    flint_solver_Term *term;
    if (sym.id == 0 || flint_solver_nearzero(value)) return;
    term = (flint_solver_Term*)flint_solver_gettable(&row->terms, sym);
    if (term == NULL) {
        term = (flint_solver_Term*)flint_solver_settable(solver, &row->terms, sym);
        assert(term != NULL);
        term->multiplier = value;
    } else if (flint_solver_nearzero(term->multiplier += value))
        flint_solver_delkey(&row->terms, &term->entry);
}

static void flint_solver_addrow(struct flint_solver_Solver *solver, flint_solver_Row *row, const flint_solver_Row *other, flint_solver_Num multiplier) {
    flint_solver_Iterator it = flint_solver_itertable(&other->terms);
    flint_solver_Term *term;
    row->constant += other->constant*multiplier;
    while ((term = (flint_solver_Term*)flint_solver_nextentry(&it)))
        flint_solver_addvar(solver, row, flint_solver_key(term), term->multiplier*multiplier);
}

static void flint_solver_solvefor(struct flint_solver_Solver *solver, flint_solver_Row *row, flint_solver_Symbol enter, flint_solver_Symbol leave) {
    flint_solver_Term *term = (flint_solver_Term*)flint_solver_gettable(&row->terms, enter);
    flint_solver_Num reciprocal = 1.0f / term->multiplier;
    assert(enter.id != leave.id && !flint_solver_nearzero(term->multiplier));
    flint_solver_delkey(&row->terms, &term->entry);
    flint_solver_multiply(row, -reciprocal);
    if (leave.id != 0) flint_solver_addvar(solver, row, leave, reciprocal);
}

static void flint_solver_substitute(struct flint_solver_Solver *solver, flint_solver_Row *row, flint_solver_Symbol enter, const flint_solver_Row *other) {
    flint_solver_Term *term = (flint_solver_Term*)flint_solver_gettable(&row->terms, enter);
    if (!term) return;
    flint_solver_delkey(&row->terms, &term->entry);
    flint_solver_addrow(solver, row, other, term->multiplier);
}

/* variables & constraints */

SOLVER_API int flint_solver_variableid(struct flint_solver_Var *var) { return var ? var->sym.id : -1; }
SOLVER_API flint_solver_Num flint_solver_value(struct flint_solver_Var *var) { return var ? var->value : 0.0f; }
SOLVER_API void flint_solver_usevariable(struct flint_solver_Var *var) { if (var) ++var->refcount; }

static struct flint_solver_Var *flint_solver_sym2var(struct flint_solver_Solver *solver, flint_solver_Symbol sym) {
    flint_solver_VarEntry *ve = (flint_solver_VarEntry*)flint_solver_gettable(&solver->vars, sym);
    assert(ve != NULL);
    return ve->var;
}

SOLVER_API struct flint_solver_Var *flint_solver_newvariable(struct flint_solver_Solver *solver) {
    struct flint_solver_Var *var = (struct flint_solver_Var*)flint_solver_alloc(solver, &solver->varpool);
    flint_solver_Symbol sym = flint_solver_newsymbol(solver, FLINT_SOLVER_EXTERNAL);
    flint_solver_VarEntry *ve = (flint_solver_VarEntry*)flint_solver_settable(solver, &solver->vars, sym);
    assert(ve != NULL);
    memset(var, 0, sizeof(struct flint_solver_Var));
    var->sym      = sym;
    var->refcount = 1;
    var->solver   = solver;
    ve->var       = var;
    return var;
}

SOLVER_API void flint_solver_delvariable(struct flint_solver_Var *var) {
    if (var && --var->refcount == 0) {
        struct flint_solver_Solver *solver = var->solver;
        flint_solver_VarEntry *e = (flint_solver_VarEntry*)flint_solver_gettable(&solver->vars, var->sym);
        assert(!var->dirty && e != NULL);
        flint_solver_delkey(&solver->vars, &e->entry);
        flint_solver_remove(var->constraint);
        flint_solver_free(&solver->varpool, var);
    }
}

SOLVER_API struct flint_solver_Constraint *flint_solver_newconstraint(struct flint_solver_Solver *solver, flint_solver_Num strength) {
    struct flint_solver_Constraint *cons = (struct flint_solver_Constraint*)flint_solver_alloc(solver, &solver->conspool);
    memset(cons, 0, sizeof(*cons));
    cons->solver   = solver;
    cons->strength = flint_solver_nearzero(strength) ? FLINT_SOLVER_REQUIRED : strength;
    flint_solver_initrow(&cons->expression);
    flint_solver_key(cons).id = ++solver->constraint_count;
    flint_solver_key(cons).type = FLINT_SOLVER_EXTERNAL;
    ((flint_solver_ConsEntry*)flint_solver_settable(solver, &solver->constraints,\
        flint_solver_key(cons)))->constraint = cons;
    return cons;
}

SOLVER_API void flint_solver_delconstraint(struct flint_solver_Constraint *cons) {
    struct flint_solver_Solver *solver = cons ? cons->solver : NULL;
    flint_solver_Iterator it;
    flint_solver_ConsEntry *ce;
    if (cons == NULL) return;
    flint_solver_remove(cons);
    ce = (flint_solver_ConsEntry*)flint_solver_gettable(&solver->constraints, flint_solver_key(cons));
    assert(ce != NULL);
    flint_solver_delkey(&solver->constraints, &ce->entry);
    it = flint_solver_itertable(&cons->expression.terms);
    while (flint_solver_nextentry(&it))
        flint_solver_delvariable(flint_solver_sym2var(solver, it.entry->key));
    flint_solver_freerow(solver, &cons->expression);
    flint_solver_free(&solver->conspool, cons);
}

SOLVER_API struct flint_solver_Constraint *flint_solver_cloneconstraint(struct flint_solver_Constraint *other, flint_solver_Num strength) {
    struct flint_solver_Constraint *cons;
    if (other == NULL) return NULL;
    cons = flint_solver_newconstraint(other->solver, flint_solver_nearzero(strength) ? other->strength : strength);
    flint_solver_mergeconstraint(cons, other, 1.0f);
    cons->relation = other->relation;
    return cons;
}

SOLVER_API int flint_solver_mergeconstraint(struct flint_solver_Constraint *cons, const struct flint_solver_Constraint *other, flint_solver_Num multiplier) {
    flint_solver_Iterator it;
    if (cons == NULL || other == NULL || cons->marker.id != 0\
            || cons->solver != other->solver) return FLINT_SOLVER_FAILED;
    if (cons->relation == FLINT_SOLVER_GREATEQUAL) multiplier = -multiplier;
    cons->expression.constant += other->expression.constant*multiplier;
    it = flint_solver_itertable(&other->expression.terms);
    while (flint_solver_nextentry(&it)) {
        flint_solver_Term *term = (flint_solver_Term*)it.entry;
        flint_solver_usevariable(flint_solver_sym2var(cons->solver, flint_solver_key(term)));
        flint_solver_addvar(cons->solver, &cons->expression, flint_solver_key(term), term->multiplier*multiplier);
    }
    return FLINT_SOLVER_OK;
}

SOLVER_API void flint_solver_resetconstraint(struct flint_solver_Constraint *cons) {
    flint_solver_Iterator it;
    if (cons == NULL) return;
    flint_solver_remove(cons);
    cons->relation = 0;
    it = flint_solver_itertable(&cons->expression.terms);
    while (flint_solver_nextentry(&it))
        flint_solver_delvariable(flint_solver_sym2var(cons->solver, it.entry->key));
    flint_solver_resetrow(&cons->expression);
}

SOLVER_API int flint_solver_addterm(struct flint_solver_Constraint *cons, struct flint_solver_Var *var, flint_solver_Num multiplier) {
    if (cons == NULL || var == NULL || cons->marker.id != 0 ||\
            cons->solver != var->solver) return FLINT_SOLVER_FAILED;
    assert(var->sym.id != 0);
    assert(var->solver == cons->solver);
    if (cons->relation == FLINT_SOLVER_GREATEQUAL) multiplier = -multiplier;
    flint_solver_addvar(cons->solver, &cons->expression, var->sym, multiplier);
    flint_solver_usevariable(var);
    return FLINT_SOLVER_OK;
}

SOLVER_API int flint_solver_addconstant(struct flint_solver_Constraint *cons, flint_solver_Num constant) {
    if (cons == NULL || cons->marker.id != 0) return FLINT_SOLVER_FAILED;
    cons->expression.constant +=\
        cons->relation == FLINT_SOLVER_GREATEQUAL ? -constant : constant;
    return FLINT_SOLVER_OK;
}

SOLVER_API int flint_solver_setrelation(struct flint_solver_Constraint *cons, int relation) {
    assert(relation >= FLINT_SOLVER_LESSEQUAL && relation <= FLINT_SOLVER_GREATEQUAL);
    if (cons == NULL || cons->marker.id != 0 || cons->relation != 0)
        return FLINT_SOLVER_FAILED;
    if (relation != FLINT_SOLVER_GREATEQUAL) flint_solver_multiply(&cons->expression, -1.0f);
    cons->relation = relation;
    return FLINT_SOLVER_OK;
}

/* Cassowary algorithm */

SOLVER_API int flint_solver_hasedit(struct flint_solver_Var *var)
{ return var != NULL && var->constraint != NULL; }

SOLVER_API int flint_solver_hasconstraint(struct flint_solver_Constraint *cons)
{ return cons != NULL && cons->marker.id != 0; }

SOLVER_API void flint_solver_autoupdate(struct flint_solver_Solver *solver, int auto_update)
{ solver->auto_update = auto_update; }

static void flint_solver_infeasible(struct flint_solver_Solver *solver, flint_solver_Row *row) {
    if (row->constant < 0.0f && !flint_solver_isdummy(row->infeasible_next)) {
        row->infeasible_next.id = solver->infeasible_rows.id;
        row->infeasible_next.type = FLINT_SOLVER_DUMMY;
        solver->infeasible_rows = flint_solver_key(row);
    }
}

static void flint_solver_markdirty(struct flint_solver_Solver *solver, struct flint_solver_Var *var) {
    if (var->dirty) return;
    var->next = solver->dirty_vars;
    solver->dirty_vars = var;
    var->dirty = 1;
    ++var->refcount;
}

static void flint_solver_substitute_rows(struct flint_solver_Solver *solver, flint_solver_Symbol var, flint_solver_Row *expr) {
    flint_solver_Iterator it = flint_solver_itertable(&solver->rows);
    while (flint_solver_nextentry(&it)) {
        flint_solver_Row *row = (flint_solver_Row*)it.entry;
        flint_solver_substitute(solver, row, var, expr);
        if (flint_solver_isexternal(flint_solver_key(row)))
            flint_solver_markdirty(solver, flint_solver_sym2var(solver, flint_solver_key(row)));
        else
            flint_solver_infeasible(solver, row);
    }
    flint_solver_substitute(solver, &solver->objective, var, expr);
}

static int flint_solver_takerow(struct flint_solver_Solver *solver, flint_solver_Symbol sym, flint_solver_Row *dst) {
    flint_solver_Row *row = (flint_solver_Row*)flint_solver_gettable(&solver->rows, sym);
    flint_solver_key(dst) = flint_solver_null();
    if (row == NULL) return FLINT_SOLVER_FAILED;
    flint_solver_delkey(&solver->rows, &row->entry);
    dst->constant   = row->constant;
    dst->terms      = row->terms;
    return FLINT_SOLVER_OK;
}

static int flint_solver_putrow(struct flint_solver_Solver *solver, flint_solver_Symbol sym, const flint_solver_Row *src) {
    flint_solver_Row *row;
    assert(flint_solver_gettable(&solver->rows, sym) == NULL);
    row = (flint_solver_Row*)flint_solver_settable(solver, &solver->rows, sym);
    row->infeasible_next = flint_solver_null();
    row->constant = src->constant;
    row->terms    = src->terms;
    return FLINT_SOLVER_OK;
}

static void flint_solver_mergerow(struct flint_solver_Solver *solver, flint_solver_Row *row, flint_solver_Symbol var, flint_solver_Num multiplier) {
    flint_solver_Row *oldrow = (flint_solver_Row*)flint_solver_gettable(&solver->rows, var);
    if (oldrow)
        flint_solver_addrow(solver, row, oldrow, multiplier);
    else
        flint_solver_addvar(solver, row, var, multiplier);
}

static int flint_solver_optimize(struct flint_solver_Solver *solver, flint_solver_Row *objective) {
    for (;;) {
        flint_solver_Symbol enter = flint_solver_null(), leave = flint_solver_null();
        flint_solver_Num r, min_ratio = FLINT_SOLVER_NUM_MAX;
        flint_solver_Iterator it = flint_solver_itertable(&objective->terms);
        flint_solver_Row tmp, *row;
        flint_solver_Term *term;

        assert(solver->infeasible_rows.id == 0);
        while ((term = (flint_solver_Term*)flint_solver_nextentry(&it)))
            if (!flint_solver_isdummy(flint_solver_key(term)) && term->multiplier < 0.0f)
            { enter = flint_solver_key(term); break; }
        if (enter.id == 0) return FLINT_SOLVER_OK;

        it = flint_solver_itertable(&solver->rows);
        while ((row = (flint_solver_Row*)flint_solver_nextentry(&it))) {
            if (flint_solver_isexternal(flint_solver_key(row))) continue;
            term = (flint_solver_Term*)flint_solver_gettable(&row->terms, enter);
            if (term == NULL || term->multiplier > 0.0f) continue;
            r = -row->constant / term->multiplier;
            if (r < min_ratio || (flint_solver_approx(r, min_ratio)\
                        && flint_solver_key(row).id < leave.id))\
                min_ratio = r, leave = flint_solver_key(row);
        }
        assert(leave.id != 0);
        if (leave.id == 0) return FLINT_SOLVER_FAILED;

        flint_solver_takerow(solver, leave, &tmp);
        flint_solver_solvefor(solver, &tmp, enter, leave);
        flint_solver_substitute_rows(solver, enter, &tmp);
        if (objective != &solver->objective)
            flint_solver_substitute(solver, objective, enter, &tmp);
        flint_solver_putrow(solver, enter, &tmp);
    }
}

static flint_solver_Row flint_solver_makerow(struct flint_solver_Solver *solver, struct flint_solver_Constraint *cons) {
    flint_solver_Iterator it = flint_solver_itertable(&cons->expression.terms);
    flint_solver_Row row;
    flint_solver_initrow(&row);
    row.constant = cons->expression.constant;
    while (flint_solver_nextentry(&it)) {
        flint_solver_Term *term = (flint_solver_Term*)it.entry;
        flint_solver_markdirty(solver, flint_solver_sym2var(solver, flint_solver_key(term)));
        flint_solver_mergerow(solver, &row, flint_solver_key(term), term->multiplier);
    }
    if (cons->relation != FLINT_SOLVER_EQUAL) {
        flint_solver_initsymbol(solver, &cons->marker, FLINT_SOLVER_SLACK);
        flint_solver_addvar(solver, &row, cons->marker, -1.0f);
        if (cons->strength < FLINT_SOLVER_REQUIRED) {
            flint_solver_initsymbol(solver, &cons->other, FLINT_SOLVER_ERROR);
            flint_solver_addvar(solver, &row, cons->other, 1.0f);
            flint_solver_addvar(solver, &solver->objective, cons->other, cons->strength);
        }
    } else if (cons->strength >= FLINT_SOLVER_REQUIRED) {
        flint_solver_initsymbol(solver, &cons->marker, FLINT_SOLVER_DUMMY);
        flint_solver_addvar(solver, &row, cons->marker, 1.0f);
    } else {
        flint_solver_initsymbol(solver, &cons->marker, FLINT_SOLVER_ERROR);
        flint_solver_initsymbol(solver, &cons->other,  FLINT_SOLVER_ERROR);
        flint_solver_addvar(solver, &row, cons->marker, -1.0f);
        flint_solver_addvar(solver, &row, cons->other,   1.0f);
        flint_solver_addvar(solver, &solver->objective, cons->marker, cons->strength);
        flint_solver_addvar(solver, &solver->objective, cons->other,  cons->strength);
    }
    if (row.constant < 0.0f) flint_solver_multiply(&row, -1.0f);
    return row;
}

static void flint_solver_remove_errors(struct flint_solver_Solver *solver, struct flint_solver_Constraint *cons) {
    if (flint_solver_iserror(cons->marker))
        flint_solver_mergerow(solver, &solver->objective, cons->marker, -cons->strength);
    if (flint_solver_iserror(cons->other))
        flint_solver_mergerow(solver, &solver->objective, cons->other, -cons->strength);
    if (flint_solver_isconstant(&solver->objective))
        solver->objective.constant = 0.0f;
    cons->marker = cons->other = flint_solver_null();
}

static int flint_solver_add_with_artificial(struct flint_solver_Solver *solver, flint_solver_Row *row, struct flint_solver_Constraint *cons) {
    flint_solver_Symbol a = flint_solver_newsymbol(solver, FLINT_SOLVER_SLACK);
    flint_solver_Iterator it;
    flint_solver_Row tmp;
    flint_solver_Term *term;
    int ret;
    --solver->symbol_count; /* artificial variable will be removed */
    flint_solver_initrow(&tmp);
    flint_solver_addrow(solver, &tmp, row, 1.0f);
    flint_solver_putrow(solver, a, row);
    flint_solver_initrow(row); /* row is useless */
    flint_solver_optimize(solver, &tmp);
    ret = flint_solver_nearzero(tmp.constant) ? FLINT_SOLVER_OK : FLINT_SOLVER_UNBOUND;
    flint_solver_freerow(solver, &tmp);
    if (flint_solver_takerow(solver, a, &tmp) == FLINT_SOLVER_OK) {
        flint_solver_Symbol enter = flint_solver_null();
        if (flint_solver_isconstant(&tmp)) { flint_solver_freerow(solver, &tmp); return ret; }
        it = flint_solver_itertable(&tmp.terms);
        while ((term = (flint_solver_Term*)flint_solver_nextentry(&it)))
            if (flint_solver_ispivotable(flint_solver_key(term))) { enter = flint_solver_key(term); break; }
        if (enter.id == 0) { flint_solver_freerow(solver, &tmp); return FLINT_SOLVER_UNBOUND; }
        flint_solver_solvefor(solver, &tmp, enter, a);
        flint_solver_substitute_rows(solver, enter, &tmp);
        flint_solver_putrow(solver, enter, &tmp);
    }
    it = flint_solver_itertable(&solver->rows);
    while ((row = (flint_solver_Row*)flint_solver_nextentry(&it))) {
        term = (flint_solver_Term*)flint_solver_gettable(&row->terms, a);
        if (term) flint_solver_delkey(&row->terms, &term->entry);
    }
    term = (flint_solver_Term*)flint_solver_gettable(&solver->objective.terms, a);
    if (term) flint_solver_delkey(&solver->objective.terms, &term->entry);
    if (ret != FLINT_SOLVER_OK) flint_solver_remove(cons);
    return ret;
}

static int flint_solver_try_addrow(struct flint_solver_Solver *solver, flint_solver_Row *row, struct flint_solver_Constraint *cons) {
    flint_solver_Symbol subject = flint_solver_null();
    flint_solver_Term *term;
    flint_solver_Iterator it = flint_solver_itertable(&row->terms);
    while ((term = (flint_solver_Term*)flint_solver_nextentry(&it)))
        if (flint_solver_isexternal(flint_solver_key(term))) { subject = flint_solver_key(term); break; }
    if (subject.id == 0 && flint_solver_ispivotable(cons->marker)) {
        flint_solver_Term *mterm = (flint_solver_Term*)flint_solver_gettable(&row->terms, cons->marker);
        if (mterm->multiplier < 0.0f) subject = cons->marker;
    }
    if (subject.id == 0 && flint_solver_ispivotable(cons->other)) {
        flint_solver_Term *oterm = (flint_solver_Term*)flint_solver_gettable(&row->terms, cons->other);
        if (oterm->multiplier < 0.0f) subject = cons->other;
    }
    if (subject.id == 0) {
        it = flint_solver_itertable(&row->terms);
        while ((term = (flint_solver_Term*)flint_solver_nextentry(&it)))
            if (!flint_solver_isdummy(flint_solver_key(term))) break;
        if (term == NULL) {
            if (flint_solver_nearzero(row->constant))
                subject = cons->marker;
            else {
                flint_solver_freerow(solver, row);
                return FLINT_SOLVER_UNSATISFIED;
            }
        }
    }
    if (subject.id == 0)
        return flint_solver_add_with_artificial(solver, row, cons);
    flint_solver_solvefor(solver, row, subject, flint_solver_null());
    flint_solver_substitute_rows(solver, subject, row);
    flint_solver_putrow(solver, subject, row);
    return FLINT_SOLVER_OK;
}

static flint_solver_Symbol flint_solver_get_leaving_row(struct flint_solver_Solver *solver, flint_solver_Symbol marker) {
    flint_solver_Symbol first = flint_solver_null(), second = flint_solver_null(), third = flint_solver_null();
    flint_solver_Num r1 = FLINT_SOLVER_NUM_MAX, r2 = FLINT_SOLVER_NUM_MAX;
    flint_solver_Iterator it = flint_solver_itertable(&solver->rows);
    while (flint_solver_nextentry(&it)) {
        flint_solver_Row *row = (flint_solver_Row*)it.entry;
        flint_solver_Term *term = (flint_solver_Term*)flint_solver_gettable(&row->terms, marker);
        if (term == NULL) continue;
        if (flint_solver_isexternal(flint_solver_key(row)))
            third = flint_solver_key(row);
        else if (term->multiplier < 0.0f) {
            flint_solver_Num r = -row->constant / term->multiplier;
            if (r < r1) r1 = r, first = flint_solver_key(row);
        } else {
            flint_solver_Num r = row->constant / term->multiplier;
            if (r < r2) r2 = r, second = flint_solver_key(row);
        }
    }
    return first.id ? first : second.id ? second : third;
}

static void flint_solver_delta_edit_constant(struct flint_solver_Solver *solver, flint_solver_Num delta, struct flint_solver_Constraint *cons) {
    flint_solver_Iterator it;
    flint_solver_Row *row;
    if ((row = (flint_solver_Row*)flint_solver_gettable(&solver->rows, cons->marker)) != NULL)
    { row->constant -= delta; flint_solver_infeasible(solver, row); return; }
    if ((row = (flint_solver_Row*)flint_solver_gettable(&solver->rows, cons->other)) != NULL)
    { row->constant += delta; flint_solver_infeasible(solver, row); return; }
    it = flint_solver_itertable(&solver->rows);
    while ((row = (flint_solver_Row*)flint_solver_nextentry(&it))) {
        flint_solver_Term *term = (flint_solver_Term*)flint_solver_gettable(&row->terms, cons->marker);
        if (term == NULL) continue;
        row->constant += term->multiplier*delta;
        if (flint_solver_isexternal(flint_solver_key(row)))
            flint_solver_markdirty(solver, flint_solver_sym2var(solver, flint_solver_key(row)));
        else
            flint_solver_infeasible(solver, row);
    }
}

static void flint_solver_dual_optimize(struct flint_solver_Solver *solver) {
    while (solver->infeasible_rows.id != 0) {
        flint_solver_Symbol cur, enter = flint_solver_null(), leave;
        flint_solver_Term *objterm, *term;
        flint_solver_Num r, min_ratio = FLINT_SOLVER_NUM_MAX;
        flint_solver_Iterator it;
        flint_solver_Row tmp, *row =\
            (flint_solver_Row*)flint_solver_gettable(&solver->rows, solver->infeasible_rows);
        assert(row != NULL);
        leave = flint_solver_key(row);
        solver->infeasible_rows = row->infeasible_next;
        row->infeasible_next = flint_solver_null();
        if (flint_solver_nearzero(row->constant) || row->constant >= 0.0f) continue;
        it = flint_solver_itertable(&row->terms);
        while ((term = (flint_solver_Term*)flint_solver_nextentry(&it))) {
            if (flint_solver_isdummy(cur = flint_solver_key(term)) || term->multiplier <= 0.0f)
                continue;
            objterm = (flint_solver_Term*)flint_solver_gettable(&solver->objective.terms, cur);
            r = objterm ? objterm->multiplier / term->multiplier : 0.0f;
            if (min_ratio > r) min_ratio = r, enter = cur;
        }
        assert(enter.id != 0);
        flint_solver_takerow(solver, leave, &tmp);
        flint_solver_solvefor(solver, &tmp, enter, leave);
        flint_solver_substitute_rows(solver, enter, &tmp);
        flint_solver_putrow(solver, enter, &tmp);
    }
}

static void *flint_solver_default_allocf(void *ud, void *ptr, size_t nsize, size_t osize) {
    void *newptr;
    (void)ud, (void)osize;
    if (nsize == 0) { free(ptr); return NULL; }
    newptr = realloc(ptr, nsize);
    if (newptr == NULL) abort();
    return newptr;
}

SOLVER_API struct flint_solver_Solver *flint_solver_newsolver(flint_solver_Allocf *allocf, void *ud) {
    struct flint_solver_Solver *solver;
    if (allocf == NULL) allocf = flint_solver_default_allocf;
    if ((solver = (struct flint_solver_Solver*)allocf(ud, NULL, sizeof(struct flint_solver_Solver), 0)) == NULL)
        return NULL;
    memset(solver, 0, sizeof(*solver));
    solver->allocf = allocf;
    solver->ud     = ud;
    flint_solver_initrow(&solver->objective);
    flint_solver_inittable(&solver->vars, sizeof(flint_solver_VarEntry));
    flint_solver_inittable(&solver->constraints, sizeof(flint_solver_ConsEntry));
    flint_solver_inittable(&solver->rows, sizeof(flint_solver_Row));
    flint_solver_initpool(&solver->varpool, sizeof(struct flint_solver_Var));
    flint_solver_initpool(&solver->conspool, sizeof(struct flint_solver_Constraint));
    return solver;
}

SOLVER_API void flint_solver_delsolver(struct flint_solver_Solver *solver) {
    flint_solver_Iterator it = flint_solver_itertable(&solver->constraints);
    flint_solver_ConsEntry *ce;
    flint_solver_Row *row;
    while ((ce = (flint_solver_ConsEntry*)flint_solver_nextentry(&it)))
        flint_solver_freerow(solver, &ce->constraint->expression);
    it = flint_solver_itertable(&solver->rows);
    while ((row = (flint_solver_Row*)flint_solver_nextentry(&it)))
        flint_solver_freerow(solver, row);
    flint_solver_freerow(solver, &solver->objective);
    flint_solver_freetable(solver, &solver->vars);
    flint_solver_freetable(solver, &solver->constraints);
    flint_solver_freetable(solver, &solver->rows);
    flint_solver_freepool(solver, &solver->varpool);
    flint_solver_freepool(solver, &solver->conspool);
    solver->allocf(solver->ud, solver, 0, sizeof(*solver));
}

SOLVER_API void flint_solver_resetsolver(struct flint_solver_Solver *solver, int clear_constraints) {
    flint_solver_Iterator it = flint_solver_itertable(&solver->vars);
    if (!solver->auto_update) flint_solver_updatevars(solver);
    while (flint_solver_nextentry(&it)) {
        flint_solver_VarEntry *ve = (flint_solver_VarEntry*)it.entry;
        struct flint_solver_Constraint **cons = &ve->var->constraint;
        flint_solver_remove(*cons);
        *cons = NULL;
    }
    assert(flint_solver_nearzero(solver->objective.constant));
    assert(solver->infeasible_rows.id == 0);
    assert(solver->dirty_vars == NULL);
    if (!clear_constraints) return;
    flint_solver_resetrow(&solver->objective);
    it = flint_solver_itertable(&solver->constraints);
    while (flint_solver_nextentry(&it)) {
        struct flint_solver_Constraint *cons = ((flint_solver_ConsEntry*)it.entry)->constraint;
        if (cons->marker.id != 0)
            cons->marker = cons->other = flint_solver_null();
    }
    it = flint_solver_itertable(&solver->rows);
    while (flint_solver_nextentry(&it)) {
        flint_solver_delkey(&solver->rows, (flint_solver_Entry*)it.entry);
        flint_solver_freerow(solver, (flint_solver_Row*)it.entry);
    }
}

SOLVER_API void flint_solver_updatevars(struct flint_solver_Solver *solver) {
    struct flint_solver_Var *var, *dead_vars = NULL;
    while (solver->dirty_vars != NULL) {
        var = solver->dirty_vars;
        solver->dirty_vars = var->next;
        var->dirty = 0;
        if (var->refcount == 1)
            var->next = dead_vars, dead_vars = var;
        else {
            flint_solver_Row *row = (flint_solver_Row*)flint_solver_gettable(&solver->rows, var->sym);
            var->value = row ? row->constant : 0.0f;
            --var->refcount;
        }
    }
    while (dead_vars != NULL) {
        var = dead_vars, dead_vars = var->next;
        flint_solver_delvariable(var);
    }
}

SOLVER_API int flint_solver_add(struct flint_solver_Constraint *cons) {
    struct flint_solver_Solver *solver = cons ? cons->solver : NULL;
    int ret, oldsym = solver ? solver->symbol_count : 0;
    flint_solver_Row row;
    if (solver == NULL || cons->marker.id != 0) return FLINT_SOLVER_FAILED;
    row = flint_solver_makerow(solver, cons);
    if ((ret = flint_solver_try_addrow(solver, &row, cons)) != FLINT_SOLVER_OK) {
        flint_solver_remove_errors(solver, cons);
        solver->symbol_count = oldsym;
    } else {
        flint_solver_optimize(solver, &solver->objective);
        if (solver->auto_update) flint_solver_updatevars(solver);
    }
    assert(solver->infeasible_rows.id == 0);
    return ret;
}

SOLVER_API void flint_solver_remove(struct flint_solver_Constraint *cons) {
    struct flint_solver_Solver *solver;
    flint_solver_Symbol marker;
    flint_solver_Row tmp;
    if (cons == NULL || cons->marker.id == 0) return;
    solver = cons->solver, marker = cons->marker;
    flint_solver_remove_errors(solver, cons);
    if (flint_solver_takerow(solver, marker, &tmp) != FLINT_SOLVER_OK) {
        flint_solver_Symbol leave = flint_solver_get_leaving_row(solver, marker);
        assert(leave.id != 0);
        flint_solver_takerow(solver, leave, &tmp);
        flint_solver_solvefor(solver, &tmp, marker, leave);
        flint_solver_substitute_rows(solver, marker, &tmp);
    }
    flint_solver_freerow(solver, &tmp);
    flint_solver_optimize(solver, &solver->objective);
    if (solver->auto_update) flint_solver_updatevars(solver);
}

SOLVER_API int flint_solver_setstrength(struct flint_solver_Constraint *cons, flint_solver_Num strength) {
    if (cons == NULL) return FLINT_SOLVER_FAILED;
    strength = flint_solver_nearzero(strength) ? FLINT_SOLVER_REQUIRED : strength;
    if (cons->strength == strength) return FLINT_SOLVER_OK;
    if (cons->strength >= FLINT_SOLVER_REQUIRED || strength >= FLINT_SOLVER_REQUIRED)
    { flint_solver_remove(cons), cons->strength = strength; return flint_solver_add(cons); }
    if (cons->marker.id != 0) {
        struct flint_solver_Solver *solver = cons->solver;
        flint_solver_Num diff = strength - cons->strength;
        flint_solver_mergerow(solver, &solver->objective, cons->marker, diff);
        flint_solver_mergerow(solver, &solver->objective, cons->other,  diff);
        flint_solver_optimize(solver, &solver->objective);
        if (solver->auto_update) flint_solver_updatevars(solver);
    }
    cons->strength = strength;
    return FLINT_SOLVER_OK;
}

SOLVER_API int flint_solver_addedit(struct flint_solver_Var *var, flint_solver_Num strength) {
    struct flint_solver_Solver *solver = var ? var->solver : NULL;
    struct flint_solver_Constraint *cons;
    if (var == NULL) return FLINT_SOLVER_FAILED;
    if (strength >= FLINT_SOLVER_STRONG) strength = FLINT_SOLVER_STRONG;
    if (var->constraint) return flint_solver_setstrength(var->constraint, strength);
    assert(var->sym.id != 0);
    cons = flint_solver_newconstraint(solver, strength);
    flint_solver_setrelation(cons, FLINT_SOLVER_EQUAL);
    flint_solver_addterm(cons, var, 1.0f); /* var must have positive signture */
    flint_solver_addconstant(cons, -var->value);
    if (flint_solver_add(cons) != FLINT_SOLVER_OK) assert(0);
    var->constraint = cons;
    var->edit_value = var->value;
    return FLINT_SOLVER_OK;
}

SOLVER_API void flint_solver_deledit(struct flint_solver_Var *var) {
    if (var == NULL || var->constraint == NULL) return;
    flint_solver_delconstraint(var->constraint);
    var->constraint = NULL;
    var->edit_value = 0.0f;
}

SOLVER_API void flint_solver_suggest(struct flint_solver_Var *var, flint_solver_Num value) {
    struct flint_solver_Solver *solver = var ? var->solver : NULL;
    flint_solver_Num delta;
    if (var == NULL) return;
    if (var->constraint == NULL) {
        flint_solver_addedit(var, FLINT_SOLVER_MEDIUM);
        assert(var->constraint != NULL);
    }
    delta = value - var->edit_value;
    var->edit_value = value;
    flint_solver_delta_edit_constant(solver, delta, var->constraint);
    flint_solver_dual_optimize(solver);
    if (solver->auto_update) flint_solver_updatevars(solver);
}


SOLVER_NS_END
