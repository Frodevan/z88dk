//-----------------------------------------------------------------------------
// die - check results and die on error
// Copyright (C) Paulo Custodio, 2011-2019
// License: http://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------
#pragma once

#include "fileutil.h"
#include "../z80asm/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#endif

#ifdef _WIN32
#include <unixem/glob.h>
#endif
#include <glob.h>
#include <dirent.h>

// check OS retval
extern int check_retval(int retval, const char *file, const char *source_file, int line_nr);
#define Check_retval(rv, file)	check_retval((rv), (file), __FILE__, __LINE__)

#define xremove(file)		Check_retval(remove(file), (file))

#ifdef _WIN32
#define xmkdir(dir)			Check_retval(_mkdir(path_os(dir)), (dir))
#define xrmdir(dir)			Check_retval(_rmdir(path_os(dir)), (dir))
#else
#define xmkdir(dir)			Check_retval(mkdir(path_os(dir), 0777), (dir))
#define xrmdir(dir)			Check_retval(rmdir(path_os(dir)), (dir))
#endif

int xglob(const char *pattern, int flags, int(*errfunc) (const char *epath, int eerrno),
	glob_t *pglob);
