/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <tds_config.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <limits.h>
#include <assert.h>
#include <ctype.h>
#ifdef __DGUX__
#include <paths.h>
#endif
#ifdef __FreeBSD__
#include <sys/time.h>
#endif
#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#define PATH_MAX 255
#endif
#ifndef _WIN32
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#define UTIL_C_PROCESSING 1
#include "tdsutil.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

#ifdef NCBI_FTDS
#   ifndef DBVERSION_46
#       define DBVERSION_46      1
#   endif
#   ifndef DBVERSION_100
#       define DBVERSION_100     2
#   endif
#   ifndef DBVERSION_42
#       define DBVERSION_42      3
#   endif
#   ifndef DBVERSION_70
#       define DBVERSION_70      4
#   endif
#   ifndef DBVERSION_80
#       define DBVERSION_80      6
#   endif
#endif

static char  software_version[]   = "$Id$";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

/* for now all messages go to the log */
int g_debug_lvl = 99;
int g_append_mode = 0;
static char *g_dump_filename;
static int   write_dump = 0;      /* is TDS stream debug log turned on? */
static FILE *dumpfile   = NULL;   /* file pointer for dump log          */

void
tds_set_parent(TDSSOCKET *tds, void *the_parent)
{
    if (tds)
            tds->parent = the_parent;
}

void *
tds_get_parent(TDSSOCKET *tds)
{
      return(tds->parent);
}
void
tds_ctx_set_parent(TDSCONTEXT *ctx, void* the_parent)
{
    if (ctx)
            ctx->parent = the_parent;
}
void *
tds_ctx_get_parent(TDSCONTEXT *ctx)
{
      return(ctx->parent);
}

int tds_swap_bytes(unsigned char *buf, int bytes)
{
  unsigned char tmp;

  int i;

    /* if (bytes % 2) { return 0 }; */
    for (i=0;i<bytes/2;i++) {
        tmp = buf[i];
        buf[i] = buf[bytes-i-1];
        buf[bytes-i-1]=tmp;
    }
    return bytes;
}

#ifdef NCBI_FTDS
void tds_swap_2bytes(unsigned char *buf)
{
  unsigned char t= buf[0];
  buf[0]= buf[1];
  buf[1]= t;
}

void tds_swap_4bytes(unsigned char *buf)
{
  unsigned char t= buf[0];
  buf[0]= buf[3];
  buf[3]= t;
  t= buf[1];
  buf[1]= buf[2];
  buf[2]= t;
}

void tds_swap_8bytes(unsigned char *buf)
{
  unsigned char t= buf[0];
  buf[0]= buf[7];
  buf[7]= t;

  t= buf[1];
  buf[1]= buf[6];
  buf[6]= t;

  t= buf[2];
  buf[2]= buf[5];
  buf[5]= t;

  t= buf[3];
  buf[3]= buf[4];
  buf[4]= t;

}
#endif
/* ============================== tdsdump_off() ==============================
 *
 * Def:  temporarily turn off logging.  Note-
 *
 * Ret:  void
 *
 * ===========================================================================
 */
void tdsdump_off()
{
   write_dump = 0;
} /* tdsdump_off()  */


/* ============================== tdsdump_on() ===============================
 *
 * Def:  turn logging back on.  Note-  You must call tdsdump_open() before
 *       calling this routine.
 *
 * Ret:  void
 *
 * ===========================================================================
 */
void tdsdump_on()
{
    write_dump = 1;
} /* tdsdump_on()  */


/* ============================= tdsdump_open() ==============================
 *
 * Def:  This creates and truncates a human readable dump file for the TDS
 *       traffic.  The name of the file is specified by the filename
 *       parameter.  If that is given as NULL, it opens a
 *       file named "tdsdump.out" in the current directory.
 *
 * Ret:  true iff the file was opened, false if it couldn't be opened.
 *
 * ===========================================================================
 */
int tdsdump_open(const char *filename)
{
int   result;   /* really should be a boolean, not an int */

   if (filename == NULL || filename[0]=='\0') {
      filename = "tdsdump.out";
   }
   if (g_append_mode) {
     g_dump_filename = strdup(filename);
      tdsdump_on();
      result = 1;
   } else if (!strcmp(filename,"stdout")) {
      dumpfile = stdout;
      result = 1;
   } else if (!strcmp(filename,"stderr")) {
      dumpfile = stderr;
      result = 1;
   } else if (NULL == (dumpfile = fopen(filename, "w"))) {
      tdsdump_off();
      result = 0;
   } else {
      tdsdump_on();
      result = 1;
   }
   return result;
} /* tdsdump_open()  */

int tdsdump_append()
{
int result;

    if (!g_dump_filename) {
        return 0;
    }

    if (!strcmp(g_dump_filename,"stdout")) {
        dumpfile = stdout;
        result = 1;
    } else if (!strcmp(g_dump_filename,"stderr")) {
        dumpfile = stderr;
        result = 1;
    } else if (NULL == (dumpfile = fopen(g_dump_filename, "a"))) {
        result = 0;
    } else {
        result = 1;
    }
    return result;
}


/* ============================= tdsdump_close() =============================
 *
 * Def:  Close the TDS dump log file.
 *
 * Ret:  void
 *
 * ===========================================================================
 */
void tdsdump_close()
{
   tdsdump_off();
   if (dumpfile!=NULL && dumpfile != stdout && dumpfile != stderr)
   {
      fclose(dumpfile);
   }
   if (g_dump_filename) {
      free(g_dump_filename);
   }
} /* tdsdump_close()  */


/* =========================== tdsdump_dump_buf() ============================
 *
 * Def:  Dump the contents of data into the log file in a human readable
 *       format.
 *
 * Ret:  void
 *
 * ===========================================================================
 */
void tdsdump_dump_buf(
   const void    *buf,     /* (I) buffer to dump                      */
   int            length)  /* (I) number of bytes in the buffer       */
{
   int                   i;
   int                   j;
   const int             bytesPerLine = 16;
   const unsigned char  *data         = buf;

   if (write_dump && dumpfile!=NULL)
   {
      for(i=0; i<length; i+=bytesPerLine)
      {
         /*
          * print the offset as a 4 digit hex number
          */
         fprintf(dumpfile, "%04x  ", i);

         /*
          * print each byte in hex
          */
         for(j=i; j<length && (j-i)<bytesPerLine; j++)
         {
            fprintf(dumpfile, "%02x ", data[j]);
            if (j-i == bytesPerLine/2) fprintf(dumpfile, " ");
         }

         /*
          * skip over to the ascii dump column
          */
         for(; 0!=(j % bytesPerLine); j++)
         {
            fprintf(dumpfile, "   ");
         }
         fprintf(dumpfile, "  |");

         /*
          * print each byte in ascii
          */
         for(j=i; j<length && (j-i)<bytesPerLine; j++)
         {
            fprintf(dumpfile, "%c", (isprint(data[j])) ? data[j] : '.');
            if (j-i == bytesPerLine/2) fprintf(dumpfile, " ");
         }
         fprintf(dumpfile, "|\n");
      }
      fprintf(dumpfile, "\n");
   }
} /* tdsdump_dump_buf()  */


/* ============================== tdsdump_log() ==============================
 *
 * Def:  This function write a message to the debug log.  fmt is a printf-like
 *       format string.  It recognizes the following format characters:
 *          d     The next argument is printed a decimal number
 *          p     The pid of the running process
 *          s     The next argument is printed as a character string
 *          L     This doesn't consume any arguments, it simply
 *                prints the current local time.
 *          D     This dumps a buffer in hexadecimal and ascii.
 *                The next argument is a pointer to the buffer
 *                and the argument after that is the number
 *                of bytes in the buffer.
 *
 * Ret:  void
 *
 * ===========================================================================
 */
void tdsdump_log(int debug_lvl, const char *fmt, ...)
{
#ifndef NDEBUG
   int ret = 0;

#ifdef NCBI_FTDS
   if (debug_lvl < g_debug_lvl)
#else
   if (debug_lvl>g_debug_lvl)
#endif
    return;

   if (g_append_mode) {
      ret = tdsdump_append();
   }
   if (write_dump && dumpfile!=NULL)
   {
      const char     *ptr;

      va_list   ap;
      va_start(ap, fmt);

     if (g_append_mode) {
          fprintf(dumpfile, "pid: %d:", (int)getpid() );
      }
      for(ptr = fmt; *ptr != '\0'; ptr++)
      {
         if (*ptr == '%')
         {
            ptr++;
            switch(*ptr)
            {
               case 's':
               {
                  char   *s = va_arg(ap, char *);
                  fputs(s, dumpfile);
                  break;
               }
               case 'd':
               {
                  int i = va_arg(ap, int);
                  fprintf(dumpfile, "%d", i);
                  break;
               }
               case 'x':
               {
                  int i = va_arg(ap, int);
                  fprintf(dumpfile, "%x", i);
                  break;
               }
               case 'D':
               {
                  char  *buf = va_arg(ap, char *);
                  int    len = va_arg(ap, int);
                  tdsdump_dump_buf(buf, len);
                  break;
               }
               case 'L': /* current local time */
               {
                  char        buf[128];

                  fputs(tds_timestamp_str(buf, 127), dumpfile);
               }
               default:
               {
                  break;
               }
            }
         }
         else
         {
            fputc(*ptr, dumpfile);
         }
      }
   }
   fflush(dumpfile);
   if (g_append_mode && ret) {
      fclose(dumpfile);
   }
#endif
} /* tdsdump_log()  */

/* Jeff's hack*** NEW CODE *** */
int tds_msleep(long usec)         /* returns 0 if ok, else -1 */
     {
#ifdef _WIN32
     Sleep(0);
     return 0;
#else
     /* try to select stdin for writing just to create a delay */
     /* fd_set fd_in; */
    struct timeval delay;          /* _select() timeout */

     /* FD_ZERO (&fd_in); */
     /* FD_SET (fileno(stdin), &fd_in); */

    delay.tv_sec = usec / 1000000L;
    delay.tv_usec = usec % 1000000L;

    return(select(0, (fd_set *)NULL, (fd_set *)NULL, (fd_set *)NULL, &delay));
#endif
    }
/* Jeff's hack ***NEW CODE END**** */

#ifdef NCBI_FTDS
void tds_block_sigpipe()
{
}
void tds_reset_sigpipe()
{
}
#endif

#ifdef NCBI_FTDS
void tds_setTDS_version(TDSLOGIN *tds_login, int version)
{
    switch(version)
    {
        case DBVERSION_42:
            tds_set_version(tds_login, 4, 2);
            break;
        case DBVERSION_46:
            tds_set_version(tds_login, 4, 6);
            break;
        case DBVERSION_70:
            tds_set_version(tds_login, 7, 0);
            break;
        case DBVERSION_80:
            tds_set_version(tds_login, 8, 0);
            break;
            // !!! DBVERSION_100 may not be used with MS SQL server. !!!
            // MS SQL server does not understand protocol version 5.0 (DBVERSION_100)
            // FreeTDS work with Sybase using protocol version 4.2. So, there
            // are no reasons at all to use protocol version 5.0
            // (DBVERSION_100) with FreeTDS.
//         case DBVERSION_100:
//             tds_set_version(tds_login, 5, 0);
//             break;
    }
}
#endif

