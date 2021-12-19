#ifndef GIFSICLE_CONFIG_H
#define GIFSICLE_CONFIG_H

#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Use the clean-failing malloc library in fmalloc.c. */
#define GIF_ALLOCATOR_DEFINED   1
#define Gif_Free free
/* Prototype strerror if we don't have it. */
#ifndef HAVE_STRERROR
char *strerror(int errno);
#endif
#ifdef __cplusplus
}
/* Get rid of a possible inline macro under C++. */
# define inline inline
#endif

#define PATHNAME_SEPARATOR '/'
#define VERSION "1"
#define RANDOM rand
/* Need _setmode under MS-DOS, to set stdin/stdout to binary mode */
/* Need _fsetmode under OS/2 for the same reason */
/* Windows has _isatty and _snprintf, not the normal versions */
#if defined(_MSDOS) || defined(_WIN32) || defined(__EMX__) || defined(__DJGPP__)
# include <fcntl.h>
# include <io.h>
# define isatty _isatty
# define snprintf _snprintf
#endif
#endif /* GIFSICLE_CONFIG_H */
