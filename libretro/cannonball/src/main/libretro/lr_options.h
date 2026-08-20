/***************************************************************************
   Interface for notifying Libretro frontend of configuration
   value changes.

   See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include <libretro.h>

/*----------------------------------------------------------------------------- */
/* Function prototypes  */
/*----------------------------------------------------------------------------- */
extern void lr_options_init();
extern void lr_options_close();

/* Split by pointer type (was an overload) to keep the call sites free
 * of casts while remaining C89-compatible. */
extern void lr_options_set_frontend_variable_bool(const bool *config_var);
extern void lr_options_set_frontend_variable_int(const int *config_var);

#ifdef __cplusplus
}
#endif
