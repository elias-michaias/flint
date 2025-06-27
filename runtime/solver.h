#ifndef solver_h
#define solver_h

#ifndef SOLVER_NS_BEGIN
# ifdef __cplusplus
#   define SOLVER_NS_BEGIN extern "C" {
#   define SOLVER_NS_END   }
# else
#   define SOLVER_NS_BEGIN
#   define SOLVER_NS_END
# endif
#endif /* SOLVER_NS_BEGIN */

#ifndef SOLVER_STATIC
# ifdef __GNUC__
#   define SOLVER_STATIC static __attribute((unused))
# else
#   define SOLVER_STATIC static
# endif
#endif

#ifdef SOLVER_STATIC_API
# ifndef SOLVER_IMPLEMENTATION
#  define SOLVER_IMPLEMENTATION
# endif
# define SOLVER_API SOLVER_STATIC
#endif

#if !defined(SOLVER_API) && defined(_WIN32)
# ifdef SOLVER_IMPLEMENTATION
#  define SOLVER_API __declspec(dllexport)
# else
#  define SOLVER_API __declspec(dllimport)
# endif
#endif

#ifndef SOLVER_API
# define SOLVER_API extern
#endif

#define FLINT_SOLVER_OK           (0)
#define FLINT_SOLVER_FAILED       (-1)
#define FLINT_SOLVER_UNSATISFIED  (-2)
#define FLINT_SOLVER_UNBOUND      (-3)

#define FLINT_SOLVER_LESSEQUAL    (1)
#define FLINT_SOLVER_EQUAL        (2)
#define FLINT_SOLVER_GREATEQUAL   (3)

#define FLINT_SOLVER_REQUIRED     ((flint_solver_Num)1000000000)
#define FLINT_SOLVER_STRONG       ((flint_solver_Num)1000000)
#define FLINT_SOLVER_MEDIUM       ((flint_solver_Num)1000)
#define FLINT_SOLVER_WEAK         ((flint_solver_Num)1)

#include <stddef.h>

SOLVER_NS_BEGIN


#ifdef AM_USE_FLOAT
typedef float  flint_solver_Num;
#else
typedef double flint_solver_Num;
#endif
typedef struct flint_solver_Solver     flint_solver_Solver;
typedef struct flint_solver_Var        flint_solver_Var;
typedef struct flint_solver_Constraint flint_solver_Constraint;

typedef void *flint_solver_Allocf (void *ud, void *ptr, size_t nsize, size_t osize);

SOLVER_API flint_solver_Solver *flint_solver_newsolver   (flint_solver_Allocf *allocf, void *ud);
SOLVER_API void       flint_solver_resetsolver (flint_solver_Solver *solver, int clear_constraints);
SOLVER_API void       flint_solver_delsolver   (flint_solver_Solver *solver);

SOLVER_API void flint_solver_updatevars (flint_solver_Solver *solver);
SOLVER_API void flint_solver_autoupdate (flint_solver_Solver *solver, int auto_update);

SOLVER_API int  flint_solver_hasedit       (flint_solver_Var *var);
SOLVER_API int  flint_solver_hasconstraint (flint_solver_Constraint *cons);

SOLVER_API int  flint_solver_add    (flint_solver_Constraint *cons);
SOLVER_API void flint_solver_remove (flint_solver_Constraint *cons);

SOLVER_API int  flint_solver_addedit (flint_solver_Var *var, flint_solver_Num strength);
SOLVER_API void flint_solver_suggest (flint_solver_Var *var, flint_solver_Num value);
SOLVER_API void flint_solver_deledit (flint_solver_Var *var);

SOLVER_API flint_solver_Var *flint_solver_newvariable (flint_solver_Solver *solver);
SOLVER_API void    flint_solver_usevariable (flint_solver_Var *var);
SOLVER_API void    flint_solver_delvariable (flint_solver_Var *var);
SOLVER_API int     flint_solver_variableid  (flint_solver_Var *var);
SOLVER_API flint_solver_Num  flint_solver_value       (flint_solver_Var *var);

SOLVER_API flint_solver_Constraint *flint_solver_newconstraint   (flint_solver_Solver *solver, flint_solver_Num strength);
SOLVER_API flint_solver_Constraint *flint_solver_cloneconstraint (flint_solver_Constraint *other, flint_solver_Num strength);

SOLVER_API void flint_solver_resetconstraint (flint_solver_Constraint *cons);
SOLVER_API void flint_solver_delconstraint   (flint_solver_Constraint *cons);

SOLVER_API int flint_solver_addterm     (flint_solver_Constraint *cons, flint_solver_Var *var, flint_solver_Num multiplier);
SOLVER_API int flint_solver_setrelation (flint_solver_Constraint *cons, int relation);
SOLVER_API int flint_solver_addconstant (flint_solver_Constraint *cons, flint_solver_Num constant);
SOLVER_API int flint_solver_setstrength (flint_solver_Constraint *cons, flint_solver_Num strength);

SOLVER_API int flint_solver_mergeconstraint (flint_solver_Constraint *cons, const flint_solver_Constraint *other, flint_solver_Num multiplier);


SOLVER_NS_END

#endif /* solver_h */