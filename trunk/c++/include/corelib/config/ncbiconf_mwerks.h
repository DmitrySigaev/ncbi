/* $Id$
 * By Jonathan Kans, NCBI (kans@ncbi.nlm.nih.gov)
 *
 * NOTE:  Unlike its UNIX counterpart, this configuration header
 *        is manually maintained in order to keep it in-sync with the
 *        "configure"-generated configuration headers.
 */

#define NCBI_CXX_TOOLKIT  1

#ifdef __MWERKS__
#  define NCBI_COMPILER_METROWERKS 1
#endif

#ifdef __MACH__
#  define NCBI_OS_DARWIN 1
#  define NCBI_OS_UNIX 1
#  define NCBI_OS      "MAC OSX"
#  define HOST         "PowerPC-Apple-MacOSX"
#  define HOST_OS      "MacOSX"
#  define _MT           1
   /* fix for /usr/include/ctype.h */
#  define _USE_CTYPE_INLINE_ 1
#else
#  define NCBI_OS_MAC  1
#  define NCBI_OS      "MAC"
#  define HOST         "PowerPC-Apple-MacOS"
#  define HOST_OS      "MacOS"
#endif

#ifdef NCBI_COMPILER_MW_MSL
#  undef NCBI_COMPILER_MW_MSL
#endif

#if defined(NCBI_OS_DARWIN) && defined(NCBI_COMPILER_METROWERKS)
#  if _MSL_USING_MW_C_HEADERS
#    define NCBI_COMPILER_MW_MSL
#  endif
#endif

#define HOST_CPU     "PowerPC"
#define HOST_VENDOR  "Apple"

#define NCBI_USE_THROW_SPEC 1
#define STACK_GROWS_DOWN    1

#define SIZEOF_CHAR         1
#define SIZEOF_DOUBLE       8
#define SIZEOF_INT          4
#define SIZEOF_LONG         4
#define SIZEOF_LONG_DOUBLE  8
#define SIZEOF_LONG_LONG    8
#define SIZEOF_SHORT        2
#define SIZEOF_SIZE_T       4
#define SIZEOF_VOIDP        4
#define NCBI_PLATFORM_BITS  32

#define STDC_HEADERS  1

#define HAVE_FSTREAM     1
#define HAVE_FSTREAM_H   1
#define HAVE_IOSTREAM    1
#define HAVE_IOSTREAM_H  1
#define HAVE_LIMITS      1
#define HAVE_STRING      1
#define HAVE_STRSTREA_H  1
#define HAVE_STRSTREAM   1
#define HAVE_UNISTD_H    1
#define HAVE_IOS_XALLOC  1
#define HAVE_DLFCN_H     1
#define HAVE_SCHED_YIELD 1