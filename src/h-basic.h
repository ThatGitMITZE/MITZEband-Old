/* File: h-basic.h */

#ifndef INCLUDED_H_BASIC_H
#define INCLUDED_H_BASIC_H

#include "msvc_warnings.h"

/*
 * The most basic "include" file.
 *
 * This file simply includes other low level header files.
 */

#ifdef HAVE_CONFIG_H
#include "autoconf.h"
#endif /* HAVE_CONFIG_H */

/**
 * Every system seems to use its own symbol as a path separator.
 *
 * Default to the standard Unix slash, but attempt to change this
 * for various other systems.  Note that any system that uses the
 * "period" as a separator (i.e. RISCOS) will have to pretend that
 * it uses the slash, and do its own mapping of period <-> slash.
 *
 * It is most definitely wrong to have such things here.  Platform-specific
 * code should handle shifting Angband filenames to platform ones. XXX
 */
#undef PATH_SEP
#define PATH_SEP "/"
#define PATH_SEPC '/'

#ifdef WINDOWS
# undef PATH_SEP
# undef PATH_SEPC
# define PATH_SEP "\\"
# define PATH_SEPC '\\'
#endif

/* System Configuration */
#include "h-config.h"

/* System includes/externs */
#include "h-system.h"

/* Basic types */
#include "h-type.h"

/* Basic constants and macros */
#include "h-define.h"

#endif

