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
#include "tdsutil.h"
#include "tds.h"
#include "sybfront.h"
#include "sybdb.h"
#include "dblib.h"
#ifdef NCBI_FTDS
#ifndef _WIN32
#include <unistd.h>
#endif
#endif

#ifdef TARGET_API_MAC_OS8
#include <stdlib.h>
#include "tdsconvert.h"
#endif

/*    was hard coded as 32768, but that made the local stack data size > 32K,
    which is not allowed on Mac OS 8/9. (mlilback, 11/7/01) */
#ifdef TARGET_API_MAC_OS8
#define ROWBUF_SIZE 31000
#else
#define ROWBUF_SIZE 32768
#endif

extern int (*g_dblib_msg_handler)();
extern int (*g_dblib_err_handler)();

extern const int g__numeric_bytes_per_prec[];

static char  software_version[]   = "$Id$";
static void *no_unused_var_warn[] = { software_version,
                                     no_unused_var_warn};

static RETCODE _bcp_start_copy_in(DBPROCESS *);
static RETCODE _bcp_build_bulk_insert_stmt(char *, BCP_COLINFO *, int);
static RETCODE _bcp_start_new_batch(DBPROCESS *);
static RETCODE _bcp_send_colmetadata(DBPROCESS *);
static int _bcp_rtrim_varchar(char *, int);
static int _bcp_err_handler(DBPROCESS * dbproc, int bcp_errno);

#ifdef NCBI_FTDS
/*
** The following little table is indexed by precision-1 and will
** tell us the number of bytes required to store the specified
** precision (with the sign).
** Support precision up to 77 digits
*/
static const int tds_numeric_bytes_per_prec[] = {
        -1, 2, 2, 3, 3, 4, 4, 4, 5, 5,
        6, 6, 6, 7, 7, 8, 8, 9, 9, 9,
        10, 10, 11, 11, 11, 12, 12, 13, 13, 14,
        14, 14, 15, 15, 16, 16, 16, 17, 17, 18,
        18, 19, 19, 19, 20, 20, 21, 21, 21, 22,
        22, 23, 23, 24, 24, 24, 25, 25, 26, 26,
        26, 27, 27, 28, 28, 28, 29, 29, 30, 30,
        31, 31, 31, 32, 32, 33, 33, 33
};
#endif

RETCODE bcp_init(DBPROCESS *dbproc, char *tblname, char *hfile, char *errfile, int direction)
{

	TDSSOCKET *tds = dbproc->tds_socket;
	BCP_COLINFO *bcpcol;
	TDSRESULTINFO *resinfo;
	int i;
	int rc;

    char query[256];

	/* free allocated storage in dbproc & initialise flags, etc. */

	_bcp_clear_storage(dbproc);

	/* check validity of parameters */

	if (hfile != (char *) NULL) {

		dbproc->bcp_hostfile = (char *) malloc(strlen(hfile) + 1);
		strcpy(dbproc->bcp_hostfile, hfile);

		if (errfile != (char *) NULL) {
			dbproc->bcp_errorfile = (char *) malloc(strlen(errfile) + 1);
			strcpy(dbproc->bcp_errorfile, errfile);
        }
        else {
			dbproc->bcp_errorfile = (char *) NULL;
		}
    }
    else {
		dbproc->bcp_hostfile = (char *) NULL;
		dbproc->bcp_errorfile = (char *) NULL;
		dbproc->sendrow_init = 0;
	}

	if (tblname == (char *) NULL) {
        _bcp_err_handler(dbproc, BCPEBCITBNM);
		return (FAIL);
	}

	if (strlen(tblname) > 92) {	/* 30.30.30 */
        _bcp_err_handler(dbproc, BCPEBCITBLEN);
		return (FAIL);
	}

	dbproc->bcp_tablename = (char *) malloc(strlen(tblname) + 1);
	strcpy(dbproc->bcp_tablename, tblname);

	if (direction == DB_IN || direction == DB_OUT)
		dbproc->bcp_direction = direction;
	else {
        _bcp_err_handler(dbproc, BCPEBDIO);
		return (FAIL);
	}

	if (dbproc->bcp_direction == DB_IN) {
        sprintf(query,"select * from %s where 0 = 1", dbproc->bcp_tablename);
        if(tds_submit_query(tds,query) == TDS_FAIL) {
			return FAIL;
		}

        while((rc= tds_process_result_tokens(tds)) == TDS_SUCCEED);
		if (rc != TDS_NO_MORE_RESULTS) {
			return FAIL;
		}

		if (!tds->res_info) {
			return FAIL;
		}

		resinfo = tds->res_info;

		dbproc->bcp_colcount = resinfo->num_cols;
		dbproc->bcp_columns = (BCP_COLINFO **) malloc(resinfo->num_cols * sizeof(BCP_COLINFO *));


		for (i = 0; i < dbproc->bcp_colcount; i++) {
			dbproc->bcp_columns[i] = (BCP_COLINFO *) malloc(sizeof(BCP_COLINFO));
			bcpcol = dbproc->bcp_columns[i];
			memset(bcpcol, '\0', sizeof(BCP_COLINFO));

			bcpcol->tab_colnum = i + 1;	/* turn offset into ordinal */
			bcpcol->db_type = resinfo->columns[i]->column_type;
			bcpcol->db_length = resinfo->columns[i]->column_size;
			bcpcol->db_nullable = resinfo->columns[i]->column_nullable;

			if (is_numeric_type(bcpcol->db_type)) {
				bcpcol->data = (BYTE *) malloc(sizeof(TDS_NUMERIC));
				((TDS_NUMERIC *) bcpcol->data)->precision = resinfo->columns[i]->column_prec;
				((TDS_NUMERIC *) bcpcol->data)->scale = resinfo->columns[i]->column_scale;
            }
            else if(is_blob_type(bcpcol->db_type)) {
                bcpcol->data= (BYTE*)malloc(16);
                memset(bcpcol->data, 0, 16);
            }
            else {
				bcpcol->data = (BYTE *) malloc(bcpcol->db_length);
				if (bcpcol->data == (BYTE *) NULL) {
					printf("could not allocate %d bytes of memory\n", bcpcol->db_length);
				}
			}


			bcpcol->data_size = 0;

			if (IS_TDS7_PLUS(tds)) {
				bcpcol->db_usertype = resinfo->columns[i]->column_usertype;
				bcpcol->db_flags = resinfo->columns[i]->column_flags;
				bcpcol->db_type_save = resinfo->columns[i]->column_type_save;
				bcpcol->db_prec = resinfo->columns[i]->column_prec;
				bcpcol->db_scale = resinfo->columns[i]->column_scale;
                memcpy(bcpcol->db_collate, resinfo->columns[i]->collation, 5);
				strcpy(bcpcol->db_name, resinfo->columns[i]->column_name);
				bcpcol->db_varint_size = resinfo->columns[i]->column_varint_size;
				bcpcol->db_unicodedata = resinfo->columns[i]->column_unicodedata;
			}
		}
        if(dbproc->bcp_hostfile == NULL) {
			dbproc->host_colcount = dbproc->bcp_colcount;
			dbproc->host_columns = (BCP_HOSTCOLINFO **) malloc(dbproc->host_colcount * sizeof(BCP_HOSTCOLINFO *));

			for (i = 0; i < dbproc->host_colcount; i++) {
                dbproc->host_columns[i] = (BCP_HOSTCOLINFO *) malloc(sizeof(BCP_HOSTCOLINFO));
				memset(dbproc->host_columns[i], '\0', sizeof(BCP_HOSTCOLINFO));
			}
		}
	}

	return SUCCEED;
}


RETCODE bcp_collen(DBPROCESS *dbproc, DBINT varlen, int table_column)
{
BCP_HOSTCOLINFO *hostcol;

	if (dbproc->bcp_direction == 0) {
        _bcp_err_handler(dbproc, BCPEBCPI);
		return FAIL;
	}

	if (dbproc->bcp_direction != DB_IN) {
        _bcp_err_handler(dbproc, BCPEBCPN);
		return FAIL;
	}

	if (table_column > dbproc->host_colcount)
		return FAIL;

	hostcol = dbproc->host_columns[table_column - 1];

	hostcol->column_len = varlen;

	return SUCCEED;
}


RETCODE bcp_columns(DBPROCESS *dbproc, int host_colcount)
{

int i;

	if (dbproc->bcp_direction == 0) {
        _bcp_err_handler(dbproc, BCPEBCPI);
		return FAIL;
	}
	if (dbproc->bcp_hostfile == (char *) NULL) {
        _bcp_err_handler(dbproc, BCPEBIVI);
		return FAIL;
	}

	if (host_colcount < 1) {
        _bcp_err_handler(dbproc, BCPEBCFO);
		return FAIL;
	}

	dbproc->host_colcount = host_colcount;
	dbproc->host_columns = (BCP_HOSTCOLINFO **) malloc(host_colcount * sizeof(BCP_HOSTCOLINFO *));

	for (i = 0; i < host_colcount; i++) {
		dbproc->host_columns[i] = (BCP_HOSTCOLINFO *) malloc(sizeof(BCP_HOSTCOLINFO));
		memset(dbproc->host_columns[i], '\0', sizeof(BCP_HOSTCOLINFO));
	}

	return SUCCEED;
}

RETCODE bcp_colfmt(DBPROCESS *dbproc, int host_colnum, int host_type, 
                   int host_prefixlen, DBINT host_collen, BYTE *host_term,
                   int host_termlen, int table_colnum)
{

BCP_HOSTCOLINFO *hostcol;
#ifdef MSDBLIB
	/* Microsoft specifies a "file_termlen" of zero if there's no terminator */
	if (host_termlen == 0)
		host_termlen = -1;
#endif
	if (dbproc->bcp_direction == 0) {
        _bcp_err_handler(dbproc, BCPEBCPI);
		return FAIL;
	}

	if (dbproc->bcp_hostfile == (char *) NULL) {
        _bcp_err_handler(dbproc, BCPEBIVI);
		return FAIL;
	}

	if (dbproc->host_colcount == 0) {
        _bcp_err_handler(dbproc, BCPEBCBC);
		return FAIL;
	}

	if (host_colnum < 1)
		return FAIL;

    if (host_prefixlen != 0 && host_prefixlen != 1 &&
        host_prefixlen != 2 && host_prefixlen != 4 &&
        host_prefixlen != -1 ) {
        _bcp_err_handler(dbproc, BCPEBCPREF);
		return FAIL;
	}

	if (table_colnum == 0 && host_type == 0) {
        _bcp_err_handler(dbproc, BCPEBCPCTYP);
		return FAIL;
	}

    if (host_prefixlen == 0 && host_collen == -1 && 
        host_termlen == -1 && !is_fixed_type(host_type)) {
        _bcp_err_handler(dbproc, BCPEVDPT);
		return FAIL;
	}

	if (host_collen < -1) {
        _bcp_err_handler(dbproc, BCPEBCHLEN);
		return FAIL;
	}

    if (is_fixed_type(host_type) && 
        (host_collen != -1 && host_collen != 0))
		return FAIL;

	/* 
	 * If there's a positive terminator length, we need a valid terminator pointer.
	 * If the terminator length is 0 or -1, then there's no terminator.
	 * FIXME: look up the correct error code for a bad terminator pointer or length and return that before arriving here.   
	 */
    /*assert ((host_termlen > 0)? (host_term != NULL) : 1);*/

	hostcol = dbproc->host_columns[host_colnum - 1];

	hostcol->host_column = host_colnum;
	hostcol->datatype = host_type;
	hostcol->prefix_len = host_prefixlen;
	hostcol->column_len = host_collen;
	if (host_term && host_termlen >= 0) { 
		hostcol->terminator = (BYTE *) malloc(host_termlen + 1);
		memcpy(hostcol->terminator, host_term, host_termlen);
	}
	hostcol->term_len = host_termlen;
	hostcol->tab_colnum = table_colnum;

	return SUCCEED;

}



RETCODE bcp_colfmt_ps(DBPROCESS *dbproc, int host_colnum, int host_type, int host_prefixlen, DBINT host_collen, BYTE *host_term,int host_termlen, int table_colnum, DBTYPEINFO *typeinfo)
{
	if (dbproc->bcp_direction == 0) {
        _bcp_err_handler(dbproc, BCPEBCPI);
		return FAIL;
	}
	return SUCCEED;
}



RETCODE bcp_control(DBPROCESS *dbproc, int field, DBINT value)
{
	if (dbproc->bcp_direction == 0) {
        _bcp_err_handler(dbproc, BCPEBCPI);
		return FAIL;
	}

	switch (field) {

    case BCPMAXERRS: dbproc->maxerrs  = value; break;
    case BCPFIRST:   dbproc->firstrow = value; break;
    case BCPLAST:    dbproc->firstrow = value; break;
    case BCPBATCH:   dbproc->bcpbatch = value; break;

	default:
        _bcp_err_handler(dbproc, BCPEIFNB);
		return FAIL;
	}
	return SUCCEED;
}

RETCODE bcp_options(DBPROCESS *dbproc, int option, BYTE *value, int valuelen)
{
	int i;
	static const char *hints[] = 	{ "ORDER"
					, "ROWS_PER_BATCH"
					, "KILOBYTES_PER_BATCH"
					, "TABLOCK"
					, "CHECK_CONSTRAINTS"
					, NULL
					};
	if (!dbproc)
		return FAIL;

	switch (option) {
	case BCPLABELED:
		tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED bcp option: BCPLABELED\n");
		return FAIL;
	case BCPHINTS:
                if (!value || valuelen <= 0)
                        return FAIL;

                if (dbproc->bcp_hint != NULL) /* hint already set */
                        return FAIL;

		for(i=0; hints[i]; i++ ) { /* do we know about this hint? */
			if (strncasecmp((char*) value, hints[i], strlen(hints[i])) == 0)
				break;
		}
		if (!hints[i]) { /* no such hint */
			return FAIL;
		}

		/* 
		 * Store the bare hint, as passed from the application.  
		 * The process that constructs the "insert bulk" statement will incorporate the hint text.
		 */
		dbproc->bcp_hint = (char*) malloc(1+valuelen);
		memcpy(dbproc->bcp_hint, value, valuelen);
		dbproc->bcp_hint[valuelen] = '\0';	/* null terminate it */
		break;
	default:
		tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED bcp option: %u\n", option);
		return FAIL;	
	}

	return SUCCEED;
}

RETCODE
bcp_colptr(DBPROCESS * dbproc, BYTE * colptr, int table_column)
{
BCP_HOSTCOLINFO *hostcol;

	if (dbproc->bcp_direction == 0) {
        _bcp_err_handler(dbproc, BCPEBCPI);
		return FAIL;
	}
	if (dbproc->bcp_direction != DB_IN) {
        _bcp_err_handler(dbproc, BCPEBCPN);
		return FAIL;
	}

	if (table_column > dbproc->host_colcount)
		return FAIL;

	hostcol = dbproc->host_columns[table_column - 1];

	hostcol->host_var = colptr;

	return SUCCEED;
}


DBBOOL bcp_getl(LOGINREC *login)
{
TDSLOGIN *tdsl = (TDSLOGIN *) login->tds_login;

	return (tdsl->bulk_copy);
}

static RETCODE _bcp_exec_out(DBPROCESS *dbproc, DBINT *rows_copied)
{

FILE *hostfile;

    int  ret;
int i;
    int  j;
    int  k;
    char *xx;

    TDSSOCKET     *tds = dbproc->tds_socket;
TDSRESULTINFO *resinfo;
BCP_COLINFO *bcpcol;
TDSCOLINFO *curcol;
BCP_HOSTCOLINFO *hostcol;
BYTE *src;
int srctype;
long len;
int buflen;
int destlen;
BYTE *outbuf;
    char          query[256];

TDS_INT rowtype;
TDS_INT computeid;
TDS_INT result_type;

TDS_TINYINT ti;
TDS_SMALLINT si;
TDS_INT li;

TDSDATEREC when;

int row_of_query;
int rows_written;

	if (!(hostfile = fopen(dbproc->bcp_hostfile, "w"))) {
        _bcp_err_handler(dbproc, BCPEBCUO);
		return FAIL;
	}

    sprintf(query,"select * from %s", dbproc->bcp_tablename);

    tds_submit_query(tds, query);

    if (tds_process_result_tokens(tds) == TDS_FAIL) {
		fclose(hostfile);
		return FAIL;
	}
	if (!tds->res_info) {
		fclose(hostfile);
		return FAIL;
	}

	resinfo = tds->res_info;

	dbproc->bcp_colcount = resinfo->num_cols;
	dbproc->bcp_columns = (BCP_COLINFO **) malloc(resinfo->num_cols * sizeof(BCP_COLINFO *));

	for (i = 0; i < resinfo->num_cols; i++) {
		dbproc->bcp_columns[i] = (BCP_COLINFO *) malloc(sizeof(BCP_COLINFO));
		memset(dbproc->bcp_columns[i], '\0', sizeof(BCP_COLINFO));
		dbproc->bcp_columns[i]->tab_colnum = i + 1;	/* turn offset into ordinal */
		dbproc->bcp_columns[i]->db_type = resinfo->columns[i]->column_type;
		dbproc->bcp_columns[i]->db_length = resinfo->columns[i]->column_size;
		if (is_numeric_type(resinfo->columns[i]->column_type))
			dbproc->bcp_columns[i]->data = (BYTE *) malloc(sizeof(TDS_NUMERIC));
		else
			dbproc->bcp_columns[i]->data = (BYTE *) malloc(dbproc->bcp_columns[i]->db_length);
		dbproc->bcp_columns[i]->data_size = 0;
	}

	row_of_query = 0;
	rows_written = 0;

    while (tds_process_row_tokens(tds)==TDS_SUCCEED) {

		row_of_query++;

		if ((dbproc->firstrow == 0 && dbproc->lastrow == 0) ||
	   /** FIX this! */
            ((dbproc->firstrow > 0 && row_of_query >= dbproc->firstrow) &&
             (dbproc->lastrow > 0 && row_of_query <= dbproc->lastrow))) {
			for (i = 0; i < dbproc->bcp_colcount; i++) {

				bcpcol = dbproc->bcp_columns[i];
				curcol = resinfo->columns[bcpcol->tab_colnum - 1];

				src = &resinfo->current_row[curcol->column_offset];
				if (is_blob_type(curcol->column_type)) {
					/* FIX ME -- no converts supported */
					src = (BYTE*) ((TDSBLOBINFO *) src)->textvalue;
					len = curcol->cur_row_size;
				} else {

					srctype = tds_get_conversion_type(curcol->column_type, curcol->column_size);

					if (srctype != bcpcol->db_type) {
						bcpcol->db_type = srctype;
					}

					if (is_numeric_type(curcol->column_type))
						memcpy(bcpcol->data, src, sizeof(TDS_NUMERIC));
					else
						memcpy(bcpcol->data, src, curcol->column_size);

					/* null columns have zero output */
					if (tds_get_null(resinfo->current_row, bcpcol->tab_colnum - 1))
						bcpcol->data_size = 0;
					else
						bcpcol->data_size = curcol->cur_row_size;
				}
			}


			for (i = 0; i < dbproc->host_colcount; i++) {

				hostcol = dbproc->host_columns[i];
				if ((hostcol->tab_colnum < 1)
				    || (hostcol->tab_colnum > dbproc->bcp_colcount)) {
					continue;
				}

				if (hostcol->tab_colnum) {
					bcpcol = dbproc->bcp_columns[hostcol->tab_colnum - 1];
					if (bcpcol->tab_colnum != hostcol->tab_colnum) {
						return FAIL;
					}
				}
				/*assert(bcpcol);*/

				if (hostcol->datatype == 0)
					hostcol->datatype = bcpcol->db_type;

				/* work out how much space to allocate for output data */

				switch (hostcol->datatype) {

                case SYBINT1:        buflen = destlen = 1;  break;
                case SYBINT2:        buflen = destlen = 2;  break;
                case SYBINT4:        buflen = destlen = 4;  break;
                case SYBREAL:        buflen = destlen = 4;  break;
                case SYBFLT8:        buflen = destlen = 8;  break;
                case SYBDATETIME:    buflen = destlen = 8;  break;
                case SYBDATETIME4:   buflen = destlen = 4;  break;
                case SYBBIT:         buflen = destlen = 1;  break;
                case SYBBITN:        buflen = destlen = 1;  break;
                case SYBMONEY:       buflen = destlen = 8;  break;
                case SYBMONEY4:      buflen = destlen = 4;  break;
				case SYBCHAR:
				case SYBVARCHAR:
					switch (bcpcol->db_type) {
					case SYBVARCHAR:
						buflen = bcpcol->db_length + 1;
						destlen = -1;
						break;
					case SYBCHAR:
						buflen = bcpcol->db_length + 1;
						destlen = -2;
						break;
					case SYBINT1:
						buflen = 4 + 1;	/*  255         */
						destlen = -1;
						break;
					case SYBINT2:
						buflen = 6 + 1;	/* -32768       */
						destlen = -1;
						break;
					case SYBINT4:
						buflen = 11 + 1;	/* -2147483648  */
						destlen = -1;
						break;
					case SYBNUMERIC:
						buflen = 40 + 1;	/* 10 to the 38 */
						destlen = -1;
						break;
					case SYBDECIMAL:
						buflen = 40 + 1;	/* 10 to the 38 */
						destlen = -1;
						break;
					case SYBFLT8:
						buflen = 40 + 1;	/* 10 to the 38 */
						destlen = -1;
						break;
					case SYBDATETIME:
						buflen = 255 + 1;
						destlen = -1;
						break;
					default:
						buflen = 255 + 1;
						destlen = -1;
						break;
					}
					break;
				default:
					buflen = destlen = 255;
				}

				outbuf = (BYTE*) malloc(buflen);

				/* if we are converting datetime to string, need to override any 
				 * date time formats already established
				 */
				if ((bcpcol->db_type == SYBDATETIME || bcpcol->db_type == SYBDATETIME4)
				    && (hostcol->datatype == SYBCHAR || hostcol->datatype == SYBVARCHAR)) {
					memset(&when, 0, sizeof(when));
					tds_datecrack(bcpcol->db_type, bcpcol->data, &when);
					buflen = tds_strftime((char*)outbuf, buflen, "%Y-%m-%d %H:%M:%S.%z", &when);
				} else {
					/* 
					 * For null columns, the above work to determine the output buffer size is moot, 
					 * because bcpcol->data_size is zero, so dbconvert() won't write anything, and returns zero. 
					 */
					buflen = dbconvert(dbproc,
							   bcpcol->db_type,
							   bcpcol->data, bcpcol->data_size, hostcol->datatype, outbuf, destlen);
                    
                    if (buflen == -1) {
                        return (FAIL);
                    }
				}

				/* FIX ME -- does not handle prefix_len == -1 */
				/* The prefix */

				switch (hostcol->prefix_len) {
				case -1:
					if (!(is_fixed_type(hostcol->datatype))) {
						si = buflen;
						fwrite(&si, sizeof(si), 1, hostfile);
					}
					break;
				case 1:
					ti = buflen;
					fwrite(&ti, sizeof(ti), 1, hostfile);
					break;
				case 2:
					si = buflen;
					fwrite(&si, sizeof(si), 1, hostfile);
					break;
				case 4:
					li = buflen;
					fwrite(&li, sizeof(li), 1, hostfile);
					break;
				}

				/* The data */

				if (is_blob_type(curcol->column_type)) {
					fwrite(src, len, 1, hostfile);
				} else {
					if (hostcol->column_len != -1) {
						buflen = buflen > hostcol->column_len ? hostcol->column_len : buflen;
					}

					fwrite(outbuf, buflen, 1, hostfile);
				}

				free(outbuf);

				/* The terminator */
				if (hostcol->terminator && hostcol->term_len > 0) {
					fwrite(hostcol->terminator, hostcol->term_len, 1, hostfile);
				}
			}
			rows_written++;
		}
	}
	if (fclose(hostfile) != 0) {
        _bcp_err_handler(dbproc, BCPEBCUC);
		return (FAIL);
	}

    printf("firstrow = %d row_of_query = %d rows written %d\n",
           dbproc->firstrow, row_of_query, rows_written);
	if (dbproc->firstrow > 0 && row_of_query < dbproc->firstrow) {
		/* The table which bulk-copy is attempting to
		 * copy to a host-file is shorter than the
		 * number of rows which bulk-copy was instructed
		 * to skip.  
		 */

        _bcp_err_handler(dbproc, BCPETTS);
		return (FAIL);
	}

	*rows_copied = rows_written;
	return SUCCEED;
}



RETCODE _bcp_read_hostfile(DBPROCESS *dbproc, FILE *hostfile)
{
BCP_COLINFO *bcpcol;
BCP_HOSTCOLINFO *hostcol;

int i;
TDS_TINYINT ti;
TDS_SMALLINT si;
TDS_INT li;
int collen;
int data_is_null;
int bytes_read;
int converted_data_size;
TDS_INT desttype;


	/* for each host file column defined by calls to bcp_colfmt */

	for (i = 0; i < dbproc->host_colcount; i++) {
		BYTE coldata[256];

		hostcol = dbproc->host_columns[i];

		data_is_null = 0;
		collen = 0;

		/* if a prefix length specified, read the correct */
		/* amount of data...                              */

		if (hostcol->prefix_len > 0) {

			switch (hostcol->prefix_len) {
			case 1:
				if (fread(&ti, 1, 1, hostfile) != 1) {
                    _bcp_err_handler(dbproc, BCPEBCRE);
					return (FAIL);
				}
				collen = ti;
				break;
			case 2:
				if (fread(&si, 2, 1, hostfile) != 1) {
                    _bcp_err_handler(dbproc, BCPEBCRE);
					return (FAIL);
				}
				collen = si;
				break;
			case 4:
				if (fread(&li, 4, 1, hostfile) != 1) {
                    _bcp_err_handler(dbproc, BCPEBCRE);
					return (FAIL);
				}
				collen = li;
				break;
			default:
             /*assert(hostcol->prefix_len <= 4);*/
				break;
			}
	
			if (collen == 0)
				data_is_null = 1;

		}

		/* if (Max) column length specified take that into */
		/* consideration...                                */

		if (!data_is_null && hostcol->column_len >= 0) {
			if (hostcol->column_len == 0)
				data_is_null = 1;
			else {
				if (collen)
					collen = (hostcol->column_len < collen) ? hostcol->column_len : collen;
				else
					collen = hostcol->column_len;
			}
		}

		/* Fixed Length data - this over-rides anything else specified */

		if (is_fixed_type(hostcol->datatype)) {
            collen = get_size_by_type(hostcol->datatype);
		}
#ifdef NCBI_FTDS
      else if(hostcol->prefix_len <= 0 && hostcol->term_len <= 0) {
          /* we need to know the size anyway */
          if (fread(&si, 2, 1, hostfile) != 1) {
              _bcp_err_handler(dbproc, BCPEBCRE);
              return (FAIL);
          }
          collen = si;
      }
#endif

		/* if this host file column contains table data,   */
		/* find the right element in the table/column list */

		if (hostcol->tab_colnum) {
			bcpcol = dbproc->bcp_columns[hostcol->tab_colnum - 1];
			if (bcpcol->tab_colnum != hostcol->tab_colnum) {
				return FAIL;
			}
		}

		/* a terminated field is specified - get the data into temporary space... */

		memset(coldata, '\0', sizeof(coldata));

		if (hostcol->term_len > 0) {
			bytes_read = _bcp_get_term_data(hostfile, hostcol, coldata);
			
			if (bytes_read == -1)
				return FAIL;

			if (collen)
				collen = (bytes_read < collen) ? bytes_read : collen;
			else
				collen = bytes_read;

			if (collen == 0)
				data_is_null = 1;

		} else {
			if (collen) {
				if (fread(coldata, collen, 1, hostfile) != 1) {
                    _bcp_err_handler(dbproc, BCPEBCRE);
					return (FAIL);
				}
			}
		}

		if (hostcol->tab_colnum) {
			if (data_is_null) {
				bcpcol->data_size = 0;
                }
                else {
				desttype = tds_get_conversion_type(bcpcol->db_type, bcpcol->db_length);
				
				converted_data_size =
					dbconvert(dbproc, hostcol->datatype, coldata, collen, desttype, 
						  bcpcol->data, bcpcol->db_length);
						  
				if (converted_data_size == -1) 
					return (FAIL);

				if (desttype == SYBVARCHAR) {
                        bcpcol->data_size = _bcp_rtrim_varchar(bcpcol->data, converted_data_size);
                    }
                    else {
					bcpcol->data_size = converted_data_size;
				}
			}
		}

	}
	return SUCCEED;
}


/* read a terminated variable from a hostfile */

RETCODE _bcp_get_term_data(FILE *hostfile, BCP_HOSTCOLINFO *hostcol, BYTE *coldata )
{

int j = 0;
int termfound = 1;
char x;
char *tester = NULL;
int bufpos = 0;
int found = 0;


	if (hostcol->term_len > 1)
		tester = (char *) malloc(hostcol->term_len);


	while (!found && (x = getc(hostfile)) != EOF) {
		if (x != *hostcol->terminator) {
			*(coldata + bufpos) = x;
			bufpos++;
        }
        else {
			if (hostcol->term_len == 1) {
				*(coldata + bufpos) = '\0';
				found = 1;
            }
            else {
				ungetc(x, hostfile);
				fread(tester, hostcol->term_len, 1, hostfile);
				termfound = 1;
				for (j = 0; j < hostcol->term_len; j++)
					if (*(tester + j) != *(hostcol->terminator + j))
						termfound = 0;
				if (termfound) {
					*(coldata + bufpos) = '\0';
					found = 1;
                }
                else {
					for (j = 0; j < hostcol->term_len; j++) {
						*(coldata + bufpos) = *(tester + j);
						bufpos++;
					}
				}
			}
		}
	}
	if (found) {
		return (bufpos);
    }
    else {
		return (-1);
	}

}

/*
** Add fixed size columns to the row
*/

static int _bcp_add_fixed_columns(DBPROCESS *dbproc, BYTE *rowbuffer, int start)
{
	TDS_NUMERIC *num;
	int row_pos = start;
	BCP_COLINFO *bcpcol;
	int cpbytes;

	int i;

	for (i = 0; i < dbproc->bcp_colcount; i++) {
		bcpcol = dbproc->bcp_columns[i];


        if (!is_nullable_type(bcpcol->db_type) &&
            !(bcpcol->db_nullable)) {

			if (!(bcpcol->db_nullable) && bcpcol->data_size == 0) {
                _bcp_err_handler(dbproc, BCPEBCNN);
				return FAIL;
			}

			if (is_numeric_type(bcpcol->db_type)) {
				num = (TDS_NUMERIC *) bcpcol->data;
                cpbytes = g__numeric_bytes_per_prec[num->precision];
				memcpy(&rowbuffer[row_pos], num->array, cpbytes);
            }
            else {
                cpbytes = bcpcol->data_size > bcpcol->db_length 
                    ? bcpcol->db_length : bcpcol->data_size;
				memcpy(&rowbuffer[row_pos], bcpcol->data, cpbytes);
			}

			row_pos += bcpcol->db_length;
		}
	}
	return row_pos;
}

/*
** Add variable size columns to the row
*/

static int _bcp_add_variable_columns(DBPROCESS *dbproc, BYTE *rowbuffer, int start)
{
	TDSSOCKET *tds = dbproc->tds_socket;
	BCP_COLINFO *bcpcol;
	TDS_NUMERIC *num;
	int row_pos = start;
	int cpbytes;
	int eod_ptr;
	BYTE offset_table[256];
	int offset_pos = 0;
	BYTE adjust_table[256];
	int adjust_pos = 0;
	int num_cols = 0;

	int i;

	for (i = 0; i < dbproc->bcp_colcount; i++) {
		bcpcol = dbproc->bcp_columns[i];

        if (is_nullable_type(bcpcol->db_type) || 
            bcpcol->db_nullable) {

			if (!(bcpcol->db_nullable) && bcpcol->data_size == 0) {
                _bcp_err_handler(dbproc, BCPEBCNN);
				return FAIL;
			}

			if (is_blob_type(bcpcol->db_type)) {

				/* no need to copy they are all zero bytes */

				cpbytes = 16;
				/* save for data write */
				bcpcol->txptr_offset = row_pos;
            } 
            else if (is_numeric_type(bcpcol->db_type)) {
				num = (TDS_NUMERIC *) bcpcol->data;
                cpbytes = g__numeric_bytes_per_prec[num->precision];
				memcpy(&rowbuffer[row_pos], num->array, cpbytes);
            }
            else {
				/* compute the length to copy to the row ** buffer */

                cpbytes = bcpcol->data_size > bcpcol->db_length 
                    ? bcpcol->db_length : bcpcol->data_size;
				memcpy(&rowbuffer[row_pos], bcpcol->data, cpbytes);

			}

			/* update offset and adjust tables (if necessary) */
			offset_table[offset_pos++] = row_pos % 256;
            if (row_pos > 255 && 
                (adjust_pos==0 || row_pos/256 != adjust_table[adjust_pos])) {
				adjust_table[adjust_pos++] = row_pos / 256;
			}

			num_cols++;
			row_pos += cpbytes;
		}
	}

	eod_ptr = row_pos;


	/* write the marker */
	if (IS_TDS50(tds))
		rowbuffer[row_pos++] = num_cols + 1;

	/* write the adjust table (right to left) */
	for (i = adjust_pos - 1; i >= 0; i--) {
		rowbuffer[row_pos++] = adjust_table[i];
	}

	/* the EOD (end of data) pointer */
	rowbuffer[row_pos++] = eod_ptr;

	/* write the offset table (right to left) */
	for (i = offset_pos - 1; i >= 0; i--) {
		rowbuffer[row_pos++] = offset_table[i];
	}

	return row_pos;
}

RETCODE bcp_sendrow(DBPROCESS *dbproc)
{

TDSSOCKET *tds = dbproc->tds_socket;
BCP_COLINFO *bcpcol;
    BCP_HOSTCOLINFO *hostcol;
    TDSCOLINFO      *curcol;

int i;
    int collen;
/* FIX ME -- calculate dynamically */
unsigned char rowbuffer[ROWBUF_SIZE];
int row_pos;
int row_sz_pos;
TDS_SMALLINT row_size;
    TDS_SMALLINT row_size_saved;

    int marker;
int blob_cols = 0;

unsigned char CHARBIN_NULL[] = { 0xff, 0xff };
unsigned char GEN_NULL = 0x00;

TDS_NUMERIC *num;
TDS_TINYINT numeric_size;

unsigned char row_token = 0xd1;

	if (dbproc->bcp_direction == 0) {
        _bcp_err_handler(dbproc, BCPEBCPI);
		return FAIL;
	}
	if (dbproc->bcp_hostfile != (char *) NULL) {
        _bcp_err_handler(dbproc, BCPEBCPB);
		return FAIL;
	}

	if (dbproc->bcp_direction != DB_IN) {
        _bcp_err_handler(dbproc, BCPEBCPN);
		return FAIL;
	}

	/* The first time sendrow is called after bcp_init,  */
	/* there is a certain amount of initialisation to be */
	/* done...                                           */

	if (!(dbproc->sendrow_init)) {

		/* first call the start_copy function, which will */
		/* retrieve details of the database table columns */

		if (_bcp_start_copy_in(dbproc) == FAIL)
			return (FAIL);

#ifndef NCBI_FTDS
        /* Next, allocate the storage to hold data about the */
        /* program variable data - these must be the same in */
        /* number as the table columns...                    */

        dbproc->host_colcount = dbproc->bcp_colcount;
        dbproc->host_columns  = (BCP_HOSTCOLINFO **) malloc(dbproc->host_colcount * sizeof(BCP_HOSTCOLINFO *));

        for ( i = 0; i < dbproc->host_colcount; i++ ) {
            dbproc->host_columns[i] = (BCP_HOSTCOLINFO *) malloc(sizeof(BCP_HOSTCOLINFO));
            memset(dbproc->host_columns[i], '\0', sizeof(BCP_HOSTCOLINFO));
        }
#endif

		/* set packet type to send bulk data */
		tds->out_flag = 0x07;

		if (IS_TDS7_PLUS(tds)) {
			_bcp_send_colmetadata(dbproc);
		}

		dbproc->sendrow_init = 1;

	}


	if (_bcp_get_prog_data(dbproc) == SUCCEED) {

		if (IS_TDS7_PLUS(tds)) {
			tds_put_byte(tds, row_token);	/* 0xd1 */

#ifdef NCBI_FTDS
            dbproc->curr_text_col= 0;
#endif
			for (i = 0; i < dbproc->bcp_colcount; i++) {
				bcpcol = dbproc->bcp_columns[i];
#ifdef NCBI_FTDS
                if(dbproc->host_columns[i]->datatype == 0) continue;

                if(is_blob_type(bcpcol->db_type)) continue;
#endif

                if (bcpcol->data_size == 0 ) /* Are we trying to insert a NULL ? */ {
                    if(bcpcol->db_nullable)   /* is the column nullable ? */ {
                        if (bcpcol->db_type_save == XSYBCHAR || 
                            bcpcol->db_type_save == XSYBVARCHAR ||
                            bcpcol->db_type_save == XSYBBINARY ||
                            bcpcol->db_type_save == XSYBVARBINARY|| 
                            bcpcol->db_type_save == XSYBNCHAR || 
                            bcpcol->db_type_save == XSYBNVARCHAR) {
						tds_put_n(tds, CHARBIN_NULL, 2);
                        }
                        else {
						tds_put_byte(tds, GEN_NULL);
					}
                    }
                    else {
                        _bcp_err_handler(dbproc, BCPEBCNN);
                        return FAIL;
                    }
                }
                else {   
					switch (bcpcol->db_varint_size) {
                    case 4: tds_put_int(tds, bcpcol->data_size);
						break;
                    case 2: tds_put_smallint(tds, bcpcol->data_size);
						break;
                    case 1: if (is_numeric_type(bcpcol->db_type)) {
                        numeric_size = g__numeric_bytes_per_prec[bcpcol->db_prec];
							tds_put_byte(tds, numeric_size);
                    }
                    else
							tds_put_byte(tds, bcpcol->data_size);
						break;
                    case 0: break;
					}

#if WORDS_BIGENDIAN
               if (tds->emul_little_endian) {
                    tds_swap_datatype(tds_get_conversion_type(bcpcol->db_type, bcpcol->db_length),
                                      bcpcol->data);
               }
#endif

					if (is_numeric_type(bcpcol->db_type)) {
						num = (TDS_NUMERIC *) bcpcol->data;
#ifdef NCBI_FTDS
                        tds_put_n(tds, num->array, g__numeric_bytes_per_prec[bcpcol->db_prec]);
#else
                        tds_put_n(tds, num->array, g__numeric_bytes_per_prec[num->precision]);
#endif
                    }
#if 0
                    else if(is_blob_type(bcpcol->db_type)) { //add blob data
                        int ss;
                        char blob_buff[256];
                        for(marker= 0; marker < 256;) {
                            blob_buff[marker++]= 'T';
                            blob_buff[marker++]= 'E';
                            blob_buff[marker++]= 'S';
                            blob_buff[marker++]= 't';
                        }
                        for(marker = bcpcol->data_size; marker > 0; marker-= ss) {
                            ss= (marker > 256)? 256 : marker;
                            tds_put_n(tds, blob_buff, ss);
                        }
                    }
#endif
                    else
						tds_put_n(tds, bcpcol->data, bcpcol->data_size);
				}
			}
			return SUCCEED;
        }
        else    /* Not TDS 7 or 8 */    {
			memset(rowbuffer, '\0', ROWBUF_SIZE);	/* zero the rowbuffer */

			/* offset 0 = number of var columns */
			/* offset 1 = row number.  zeroed (datasever assigns) */
			row_pos = 2;

			rowbuffer[0] = dbproc->var_cols;

            if (row_pos = _bcp_add_fixed_columns(dbproc, rowbuffer, row_pos) == FAIL)
				return (FAIL);


			row_sz_pos = row_pos;


			if (dbproc->var_cols) {

				row_pos += 2;	/* skip over row length */

				if ((row_pos = _bcp_add_variable_columns(dbproc, rowbuffer, row_pos)) == FAIL)
					return (FAIL);
			}

			row_size = row_pos;


			if (dbproc->var_cols)
				memcpy(&rowbuffer[row_sz_pos], &row_size, sizeof(row_size));

			tds_put_smallint(tds, row_size);
			tds_put_n(tds, rowbuffer, row_size);

			/* row is done, now handle any text/image data */

			blob_cols = 0;

			for (i = 0; i < dbproc->bcp_colcount; i++) {
				bcpcol = dbproc->bcp_columns[i];
				if (is_blob_type(bcpcol->db_type)) {
					/* unknown but zero */
					tds_put_smallint(tds, 0);
					tds_put_byte(tds, bcpcol->db_type);
					tds_put_byte(tds, 0xff - blob_cols);
					/* offset of txptr we stashed during variable
                    ** column processing */
					tds_put_smallint(tds, bcpcol->txptr_offset);
					tds_put_int(tds, bcpcol->data_size);
					tds_put_n(tds, bcpcol->data, bcpcol->data_size);
					blob_cols++;
				}
			}
			return SUCCEED;
		}
	}

	return FAIL;
}


static RETCODE _bcp_exec_in(DBPROCESS *dbproc, DBINT *rows_copied)
{
	FILE *hostfile, *errfile;
	TDSSOCKET *tds = dbproc->tds_socket;
	BCP_COLINFO *bcpcol;

	/* FIX ME -- calculate dynamically */
	unsigned char rowbuffer[ROWBUF_SIZE];
	int row_pos;

	/* end of data pointer...the last byte of var data before the adjust table */

	int row_sz_pos;
	TDS_SMALLINT row_size;

	const unsigned char CHARBIN_NULL[] = { 0xff, 0xff };
	const unsigned char GEN_NULL = 0x00;
	const unsigned char row_token = 0xd1;

	TDS_NUMERIC *num;
	TDS_TINYINT numeric_size;

	int i;
	int marker;
	int blob_cols = 0;
	int row_of_hostfile;
	int rows_written_so_far;
	int rows_copied_this_batch = 0;
	int rows_copied_so_far = 0;

	if (!(hostfile = fopen(dbproc->bcp_hostfile, "r"))) {
        _bcp_err_handler(dbproc, BCPEBCUO);
		return FAIL;
	}

	if (dbproc->bcp_errorfile) {
		if (!(errfile = fopen(dbproc->bcp_errorfile, "w"))) {
			_bcp_err_handler(dbproc, SYBEBUOE);
			return FAIL;
		}
	}

	if (_bcp_start_copy_in(dbproc) == FAIL)
		return (FAIL);

	tds->out_flag = 0x07;

	if (IS_TDS7_PLUS(tds)) {
		_bcp_send_colmetadata(dbproc);
	}

	row_of_hostfile = 0;

	rows_written_so_far = 0;

	while (_bcp_read_hostfile(dbproc, hostfile/*, errfile*/) == SUCCEED) {

		row_of_hostfile++;

		if ((dbproc->firstrow == 0 && dbproc->lastrow == 0) ||
		    ((dbproc->firstrow > 0 && row_of_hostfile >= dbproc->firstrow)
		     && (dbproc->lastrow > 0 && row_of_hostfile <= dbproc->lastrow))
			) {

			if (IS_TDS7_PLUS(tds)) {
				tds_put_byte(tds, row_token);	/* 0xd1 */

				for (i = 0; i < dbproc->bcp_colcount; i++) {
					bcpcol = dbproc->bcp_columns[i];

					if (bcpcol->data_size == 0) {	/* Are we trying to insert a NULL ? */
						if (!bcpcol->db_nullable){	
							/* too bad if the column is not nullable */
							fprintf(errfile, "Column is not nullable. Row %d, Column %d\n", 
								row_of_hostfile, i);
							fwrite(bcpcol->data, bcpcol->data_size, 1, errfile);
							_bcp_err_handler(dbproc, BCPEBCNN);
							return FAIL;
						}
						
						switch (bcpcol->db_type_save) {
						case XSYBCHAR:
						case XSYBVARCHAR:
						case XSYBBINARY:
						case XSYBVARBINARY:
						case XSYBNCHAR:
						case XSYBNVARCHAR:
							tds_put_n(tds, CHARBIN_NULL, 2);
							break;
						default :
							tds_put_byte(tds, GEN_NULL);
							break;
						}
					} else {
						switch (bcpcol->db_varint_size) {
						case 4:
							tds_put_int(tds, bcpcol->data_size);
							break;
						case 2:
							tds_put_smallint(tds, bcpcol->data_size);
							break;
						case 1:
							if (is_numeric_type(bcpcol->db_type)) {
								numeric_size = tds_numeric_bytes_per_prec[bcpcol->db_prec];
								tds_put_byte(tds, numeric_size);
							} else
								tds_put_byte(tds, bcpcol->data_size);
							break;
						case 0:
							break;
						default:
                      /*assert(bcpcol->db_varint_size <= 4);*/
							break;
						}

#ifdef WORDS_BIGENDIAN
						tds_swap_datatype
							(tds_get_conversion_type(bcpcol->db_type, bcpcol->db_length), bcpcol->data);
#else
						if (is_numeric_type(bcpcol->db_type)) {
							tds_swap_datatype
								(tds_get_conversion_type
								 (bcpcol->db_type, bcpcol->db_length), bcpcol->data);
						}
#endif

						if (is_numeric_type(bcpcol->db_type)) {
							num = (TDS_NUMERIC *) bcpcol->data;
                            tds_put_n(tds, num->array, g__numeric_bytes_per_prec[num->precision]);
                        }
                        else
							tds_put_n(tds, bcpcol->data, bcpcol->data_size);
					}
				}
            }
            else    /* Not TDS 7 or 8 */    {
				memset(rowbuffer, '\0', ROWBUF_SIZE);	/* zero the rowbuffer */

				/* offset 0 = number of var columns */
				/* offset 1 = row number.  zeroed (datasever assigns) */

				row_pos = 2;

				rowbuffer[0] = dbproc->var_cols;

				if ((row_pos = _bcp_add_fixed_columns(dbproc, rowbuffer, row_pos)) == FAIL)
					return (FAIL);

				row_sz_pos = row_pos;

				if (dbproc->var_cols) {

					row_pos += 2;	/* skip over row length */

                    if ((row_pos = _bcp_add_variable_columns(dbproc, rowbuffer, row_pos)) == FAIL)
						return (FAIL);
				}

				row_size = row_pos;


				if (dbproc->var_cols)
					memcpy(&rowbuffer[row_sz_pos], &row_size, sizeof(row_size));

				tds_put_smallint(tds, row_size);
				tds_put_n(tds, rowbuffer, row_size);

				/* row is done, now handle any text/image data */

				blob_cols = 0;

				for (i = 0; i < dbproc->bcp_colcount; i++) {
					bcpcol = dbproc->bcp_columns[i];
					if (is_blob_type(bcpcol->db_type)) {
						/* unknown but zero */
						tds_put_smallint(tds, 0);
						tds_put_byte(tds, bcpcol->db_type);
						tds_put_byte(tds, 0xff - blob_cols);
						/* offset of txptr we stashed during variable
                        ** column processing */
						tds_put_smallint(tds, bcpcol->txptr_offset);
						tds_put_int(tds, bcpcol->data_size);
						tds_put_n(tds, bcpcol->data, bcpcol->data_size);
						blob_cols++;
					}
				}
			}	/* Not TDS  7.0 or 8.0 */

			rows_written_so_far++;

			if (dbproc->bcpbatch > 0 && rows_written_so_far == dbproc->bcpbatch) {
				rows_written_so_far = 0;

				tds_flush_packet(tds);

				do {
					marker = tds_get_byte(tds);
					if (marker == TDS_DONE_TOKEN) {
						tds_process_end(tds, marker, NULL,NULL);
						rows_copied_this_batch = tds->rows_affected;
						rows_copied_so_far += rows_copied_this_batch;
                    } 
                    else {
						tds_process_default_tokens(tds, marker);
					}
				} while (marker != TDS_DONE_TOKEN);

				_bcp_start_new_batch(dbproc);

			}
		}
	}

	if (fclose(hostfile) != 0) {
        _bcp_err_handler(dbproc, BCPEBCUC);
		return (FAIL);
	}

	tds_flush_packet(tds);

	do {
		marker = tds_get_byte(tds);
		if (marker == TDS_DONE_TOKEN) {
			tds_process_end(tds, marker, NULL,NULL);
			rows_copied_this_batch = tds->rows_affected;
			rows_copied_so_far += rows_copied_this_batch;
			*rows_copied = rows_copied_so_far;
        } 
        else {
			tds_process_default_tokens(tds, marker);
		}
	} while (marker != TDS_DONE_TOKEN);

	return SUCCEED;
}

RETCODE bcp_exec(DBPROCESS *dbproc, DBINT *rows_copied)
{

RETCODE ret;

	if (dbproc->bcp_direction == 0) {
        _bcp_err_handler(dbproc, BCPEBCPI);
		return FAIL;
	}
	if (dbproc->bcp_hostfile) {
		if (dbproc->bcp_direction == DB_OUT) {
			ret = _bcp_exec_out(dbproc, rows_copied);
        } 
        else if (dbproc->bcp_direction==DB_IN) {
			ret = _bcp_exec_in(dbproc, rows_copied);
		}
		_bcp_clear_storage(dbproc);
		return ret;
    }
    else {
        _bcp_err_handler(dbproc, BCPEBCVH );
		return FAIL;
	}
}

static RETCODE _bcp_start_copy_in(DBPROCESS *dbproc)
{

TDSSOCKET *tds = dbproc->tds_socket;
BCP_COLINFO *bcpcol;
    TDSCOLINFO      *curcol;
    TDSRESULTINFO   *resinfo;

int i;
int marker;
    char colstatus;

#ifdef NCBI_FTDS
    char query[2048];
    char colclause[2048];
#else
    char query[256];
    char colclause[256];
#endif
int firstcol;

	if (IS_TDS7_PLUS(tds)) {
		int erc;
		char *hint;
        firstcol = 1;
#ifdef NCBI_FTDS
        colclause[0]= '\0';
        for(i= 0; i < dbproc->bcp_colcount; i++) {
            if((!is_blob_type(dbproc->bcp_columns[i]->db_type)) &&
               (dbproc->host_columns[i]->datatype != 0)){
                _bcp_build_bulk_insert_stmt(colclause, dbproc->bcp_columns[i], firstcol);
                firstcol = 0;
            }
        }

        for(i= 0; i < dbproc->bcp_colcount; i++) {
            if(is_blob_type(dbproc->bcp_columns[i]->db_type) &&
               (dbproc->host_columns[i]->datatype != 0)) {
                _bcp_build_bulk_insert_stmt(colclause, dbproc->bcp_columns[i], firstcol);
                firstcol = 0;
            }
        }
#else
		strcpy(colclause, "");

		for (i = 0; i < dbproc->bcp_colcount; i++) {
			bcpcol = dbproc->bcp_columns[i];

			if (IS_TDS7_PLUS(tds)) {
				_bcp_build_bulk_insert_stmt(colclause, bcpcol, firstcol);
				firstcol = 0;
			}
		}
#endif
		if (dbproc->bcp_hint) {
			if (asprintf(&hint, " with (%s)", dbproc->bcp_hint) < 0) {
				return FAIL;
			}
		} else {
			hint = strdup("");
		}
		if (!hint) return FAIL;

		sprintf(query, "insert bulk %s (%s)%s", dbproc->bcp_tablename, colclause, hint);

		free(hint); 

	} 
   else {
       sprintf(query, "insert bulk %s", dbproc->bcp_tablename);
	}

	tds_submit_query(tds, query);

	/* save the statement for later... */

    dbproc->bcp_insert_stmt = (char *) malloc(strlen(query)+1);
    strcpy(dbproc->bcp_insert_stmt, query);

	/* In TDS 5 we get the column information as a */
	/* result set from the "insert bulk" command.  */
	/* we're going to ignore this....              */

	if (IS_TDS50(tds)) {
        if (tds_process_result_tokens(tds) == TDS_FAIL) {
			return FAIL;
		}
		if (!tds->res_info) {
			return FAIL;
		}

        while (tds_process_row_tokens(tds) == SUCCEED); 

    }
    else {
		marker = tds_get_byte(tds);
		tds_process_default_tokens(tds, marker);
		if (!is_end_token(marker)) {
			return FAIL;
		}
	}

	/* work out the number of "variable" columns -         */
	/* either varying length type e.g. varchar or nullable */

	dbproc->var_cols = 0;

	for (i = 0; i < dbproc->bcp_colcount; i++) {

		bcpcol = dbproc->bcp_columns[i];
        if (is_nullable_type(bcpcol->db_type) ||
            bcpcol->db_nullable) {
			dbproc->var_cols++;
		}
	}

	return SUCCEED;

}

static RETCODE _bcp_build_bulk_insert_stmt(char *clause, BCP_COLINFO *bcpcol, int first)
{
	char column_type[32];

	switch (bcpcol->db_type_save) {
    case SYBINT1:      strcpy(column_type,"tinyint");
		break;
    case SYBBIT:       strcpy(column_type,"bit");
		break;
    case SYBINT2:      strcpy(column_type,"smallint");
		break;
    case SYBINT4:      strcpy(column_type,"int");
		break;
    case SYBDATETIME:  strcpy(column_type,"datetime");
		break;
    case SYBDATETIME4: strcpy(column_type,"smalldatetime");
		break;
    case SYBREAL:      strcpy(column_type,"real");
		break;
    case SYBMONEY:     strcpy(column_type,"money");
		break;
    case SYBMONEY4:    strcpy(column_type,"smallmoney");
		break;
    case SYBFLT8:      strcpy(column_type,"float");
		break;
    case SYBINT8:      strcpy(column_type,"bigint");
		break;

    case SYBINTN:      switch (bcpcol->db_length) {
    case 1: strcpy(column_type,"tinyint");
			break;
    case 2: strcpy(column_type,"smallint");
			break;
    case 4: strcpy(column_type,"int");
			break;
    case 8: strcpy(column_type,"bigint");
			break;
		}
		break;

    case SYBBITN:      strcpy(column_type,"bit");
		break;
    case SYBFLTN:      switch (bcpcol->db_length) {
    case 4: strcpy(column_type,"real");
			break;
    case 8: strcpy(column_type,"float");
			break;
		}
		break;
    case SYBMONEYN:    switch (bcpcol->db_length) {
    case 4: strcpy(column_type,"smallmoney");
			break;
    case 8: strcpy(column_type,"money");
			break;
		}
		break;
    case SYBDATETIMN:  switch (bcpcol->db_length) {
    case 4: strcpy(column_type,"smalldatetime");
			break;
    case 8: strcpy(column_type,"datetime");
			break;
		}
		break;
    case SYBDECIMAL:   sprintf(column_type,"decimal(%d,%d)", bcpcol->db_prec, bcpcol->db_scale);
		break;
    case SYBNUMERIC:   sprintf(column_type,"numeric(%d,%d)", bcpcol->db_prec, bcpcol->db_scale);
		break;

    case XSYBVARBINARY: sprintf(column_type,"varbinary(%d)", bcpcol->db_length);
		break;
    case XSYBVARCHAR  : sprintf(column_type,"varchar(%d)", bcpcol->db_length);
		break;
    case XSYBBINARY   : sprintf(column_type,"binary(%d)", bcpcol->db_length);
		break;
    case XSYBCHAR     : sprintf(column_type,"char(%d)", bcpcol->db_length);
		break;
    case SYBTEXT      : sprintf(column_type,"text");
		break;
    case SYBIMAGE     : sprintf(column_type,"image");
		break;
    case XSYBNVARCHAR : sprintf(column_type,"nvarchar(%d)", bcpcol->db_length);
		break;
    case XSYBNCHAR    : sprintf(column_type,"nchar(%d)", bcpcol->db_length);
		break;
    case SYBNTEXT     : sprintf(column_type,"ntext");
		break;
	}

	if (!first)
		strcat(clause, ", ");

	strcat(clause, bcpcol->db_name);
	strcat(clause, " ");

	strcat(clause, column_type);

#ifdef NCBI_FTDS
	return SUCCEED;
#endif
}

static RETCODE _bcp_start_new_batch(DBPROCESS *dbproc)
{

TDSSOCKET *tds = dbproc->tds_socket;
int marker;

	if (IS_TDS50(tds)) {

		tds_submit_query(tds, dbproc->bcp_insert_stmt);

        if (tds_process_result_tokens(tds) == TDS_FAIL) {
			return FAIL;
		}
		if (!tds->res_info) {
			return FAIL;
		}

        while (tds_process_row_tokens(tds) == SUCCEED); 

    }   
    else {
		tds_submit_query(tds, dbproc->bcp_insert_stmt);

		marker = tds_get_byte(tds);
		tds_process_default_tokens(tds, marker);
		if (!is_end_token(marker)) {
			return FAIL;
		}
	}

	tds->out_flag = 0x07;

	if (IS_TDS7_PLUS(tds)) {
		_bcp_send_colmetadata(dbproc);
	}
	return SUCCEED;
}

static RETCODE _bcp_send_colmetadata(DBPROCESS *dbproc)
{

TDSSOCKET *tds = dbproc->tds_socket;
    int          marker;
unsigned char colmetadata_token = 0x81;
BCP_COLINFO *bcpcol;
char unicode_string[256];


int i;
#ifdef NCBI_FTDS
    int n;
#endif


	if (IS_TDS7_PLUS(tds)) {
		/* Deep joy! - for TDS 8 we have to send a 
           colmetadata message followed by row data 
		 */

		tds_put_byte(tds, colmetadata_token);	/* 0x81 */

#ifdef NCBI_FTDS
        for (i = 0, n= 0; i < dbproc->bcp_colcount; i++) {
            if(dbproc->host_columns[i]->datatype != 0) n++;
        }
        tds_put_smallint(tds, n);
#else
		tds_put_smallint(tds, dbproc->bcp_colcount);
#endif

#ifdef NCBI_FTDS
        for(n= 2; n--;)
#endif
        for (i = 0; i < dbproc->bcp_colcount; i++) {
#ifdef NCBI_FTDS
            if(dbproc->host_columns[i]->datatype == 0) continue;
            if(n) {
                if(is_blob_type(dbproc->bcp_columns[i]->db_type)) continue;
            }
            else
                if(!is_blob_type(dbproc->bcp_columns[i]->db_type)) continue;
#endif

			bcpcol = dbproc->bcp_columns[i];
			tds_put_smallint(tds, bcpcol->db_usertype);
			tds_put_smallint(tds, bcpcol->db_flags);
			tds_put_byte(tds, bcpcol->db_type_save);

			switch (bcpcol->db_varint_size) {
            case 4: tds_put_int(tds, bcpcol->db_length);
				break;
            case 2: tds_put_smallint(tds, bcpcol->db_length);
				break;
            case 1: tds_put_byte(tds, bcpcol->db_length);
				break;
            case 0: break;
			}

			if (is_numeric_type(bcpcol->db_type)) {
				tds_put_byte(tds, bcpcol->db_prec);
				tds_put_byte(tds, bcpcol->db_scale);
			}
#ifndef NCBI_FTDS
            if (IS_TDS80(tds) && is_collate_type(bcpcol->db_type_save)) {
#else
            if (IS_TDS80(tds) && 
                (((bcpcol->db_flags & 0x2) != 0) || is_collate_type(bcpcol->db_type_save))) {
#endif
				tds_put_n(tds, bcpcol->db_collate, 5);
			}
#ifdef NCBI_FTDS
            if(is_blob_type(bcpcol->db_type)) {
                tds_put_byte(tds, 0x0); tds_put_byte(tds, 0x0); /* don't know what it is */
            }
#endif
			tds_put_byte(tds, strlen(bcpcol->db_name));
			tds7_ascii2unicode(tds, bcpcol->db_name, unicode_string, 255);
			tds_put_n(tds, unicode_string, strlen(bcpcol->db_name) * 2);

		}
	}
	return SUCCEED;
}


RETCODE bcp_readfmt(DBPROCESS *dbproc, char *filename)
{
FILE *ffile;
char buffer[1024];

float lf_version = 0.0;
int li_numcols = 0;
int colinfo_count = 0;

    struct fflist {
	struct fflist *nextptr;
	BCP_HOSTCOLINFO colinfo;
};

struct fflist *topptr = (struct fflist *) NULL;
struct fflist *curptr = (struct fflist *) NULL;

BCP_HOSTCOLINFO *hostcol;

	if (dbproc->bcp_direction == 0) {
        _bcp_err_handler(dbproc, BCPEBCPI);
		return FAIL;
	}

	if ((ffile = fopen(filename, "r")) == (FILE *) NULL) {
        _bcp_err_handler(dbproc, BCPEBUOF);
		return (FAIL);
	}

	if ((fgets(buffer, sizeof(buffer), ffile)) != (char *) NULL) {
		buffer[strlen(buffer) - 1] = '\0';	/* discard newline */
		lf_version = atof(buffer);
	}

	if ((fgets(buffer, sizeof(buffer), ffile)) != (char *) NULL) {
		buffer[strlen(buffer) - 1] = '\0';	/* discard newline */
		li_numcols = atoi(buffer);
	}

	while ((fgets(buffer, sizeof(buffer), ffile)) != (char *) NULL) {

		buffer[strlen(buffer) - 1] = '\0';	/* discard newline */


		if (topptr == (struct fflist *) NULL) {	/* first time */
			if ((topptr = (struct fflist *) malloc(sizeof(struct fflist))) == (struct fflist *) NULL) {
				tds_error_log("out of memory");
				return (FAIL);
			}
			curptr = topptr;
			curptr->nextptr = NULL;
			if (_bcp_readfmt_colinfo(dbproc, buffer, &(curptr->colinfo)))
				colinfo_count++;
			else
				return (FAIL);
		} else {
			if ((curptr->nextptr = (struct fflist *) malloc(sizeof(struct fflist))) == (struct fflist *) NULL) {
				tds_error_log("out of memory");
				return (FAIL);
			}
			curptr = curptr->nextptr;
			curptr->nextptr = NULL;
			if (_bcp_readfmt_colinfo(dbproc, buffer, &(curptr->colinfo)))
				colinfo_count++;
			else
				return (FAIL);
		}

	}
	if (fclose(ffile) != 0) {
        _bcp_err_handler(dbproc, BCPEBUCF);
		return (FAIL);
	}

	if (colinfo_count != li_numcols)
		return (FAIL);

	if (bcp_columns(dbproc, li_numcols) == FAIL) {
		return (FAIL);
	}

	for (curptr = topptr; curptr->nextptr != (struct fflist *) NULL; curptr = curptr->nextptr) {
		hostcol = &(curptr->colinfo);
		if (bcp_colfmt(dbproc, hostcol->host_column, hostcol->datatype,
			       hostcol->prefix_len, hostcol->column_len,
                       hostcol->terminator,  hostcol->term_len ,
                       hostcol->tab_colnum ) == FAIL) {
			return (FAIL);
		}
	}
	hostcol = &(curptr->colinfo);
	if (bcp_colfmt(dbproc, hostcol->host_column, hostcol->datatype,
		       hostcol->prefix_len, hostcol->column_len,
                   hostcol->terminator,  hostcol->term_len ,
                   hostcol->tab_colnum ) == FAIL) {
		return (FAIL);
	}

	return (SUCCEED);

}

int _bcp_readfmt_colinfo(DBPROCESS *dbproc, char *buf, BCP_HOSTCOLINFO *ci)
{

	char *tok;
	int whichcol;
	char term[30];
	int i;

    enum nextcol {
		HOST_COLUMN,
		DATATYPE,
		PREFIX_LEN,
		COLUMN_LEN,
		TERMINATOR,
		TAB_COLNUM,
		NO_MORE_COLS
	};

	tok = strtok(buf, " \t");
	whichcol = HOST_COLUMN;

	while (tok != (char *) NULL && whichcol != NO_MORE_COLS) {
		switch (whichcol) {

		case HOST_COLUMN:
			ci->host_column = atoi(tok);

			if (ci->host_column < 1) {
                _bcp_err_handler(dbproc, BCPEBIHC);
				return (FALSE);
			}

			whichcol = DATATYPE;
			break;

		case DATATYPE:
			if (strcmp(tok, "SYBCHAR") == 0)
				ci->datatype = SYBCHAR;
			else if (strcmp(tok, "SYBTEXT") == 0)
				ci->datatype = SYBTEXT;
			else if (strcmp(tok, "SYBBINARY") == 0)
				ci->datatype = SYBBINARY;
			else if (strcmp(tok, "SYBIMAGE") == 0)
				ci->datatype = SYBIMAGE;
			else if (strcmp(tok, "SYBINT1") == 0)
				ci->datatype = SYBINT1;
			else if (strcmp(tok, "SYBINT2") == 0)
				ci->datatype = SYBINT2;
			else if (strcmp(tok, "SYBINT4") == 0)
				ci->datatype = SYBINT4;
			else if (strcmp(tok, "SYBFLT8") == 0)
				ci->datatype = SYBFLT8;
			else if (strcmp(tok, "SYBREAL") == 0)
				ci->datatype = SYBREAL;
			else if (strcmp(tok, "SYBBIT") == 0)
				ci->datatype = SYBBIT;
			else if (strcmp(tok, "SYBNUMERIC") == 0)
				ci->datatype = SYBNUMERIC;
			else if (strcmp(tok, "SYBDECIMAL") == 0)
				ci->datatype = SYBDECIMAL;
			else if (strcmp(tok, "SYBMONEY") == 0)
				ci->datatype = SYBMONEY;
			else if (strcmp(tok, "SYBDATETIME") == 0)
				ci->datatype = SYBDATETIME;
			else if (strcmp(tok, "SYBDATETIME4") == 0)
				ci->datatype = SYBDATETIME4;
			else {
                _bcp_err_handler(dbproc, BCPEBUDF);
				return (FALSE);
			}

			whichcol = PREFIX_LEN;
			break;

		case PREFIX_LEN:
			ci->prefix_len = atoi(tok);
			whichcol = COLUMN_LEN;
			break;
		case COLUMN_LEN:
			ci->column_len = atoi(tok);
			whichcol = TERMINATOR;
			break;
		case TERMINATOR:

			if (*tok++ != '\"')
				return (FALSE);

			for (i = 0; *tok != '\"' && i < 30; i++) {
				if (*tok == '\\') {
					tok++;
					switch (*tok) {
                    case 't':  term[i] = '\t'; break;
                    case 'n':  term[i] = '\n'; break;
                    case 'r':  term[i] = '\r'; break;
                    case '\\': term[i] = '\\'; break;
                    case '0':  term[i] = '\0'; break;
                    default:   return(FALSE);
					}
					tok++;
                }
                else
					term[i] = *tok++;
			}

			if (*tok != '\"')
				return (FALSE);

			term[i] = '\0';
			ci->terminator = (BYTE*) malloc(i + 1);
			strcpy((char*) ci->terminator, term);
			ci->term_len = strlen(term);

			whichcol = TAB_COLNUM;
			break;

		case TAB_COLNUM:
			ci->tab_colnum = atoi(tok);
			whichcol = NO_MORE_COLS;
			break;

		}
		tok = strtok((char *) NULL, " \t");
	}
	if (whichcol == NO_MORE_COLS)
		return (TRUE);
	else
		return (FALSE);
}

RETCODE bcp_writefmt(DBPROCESS *dbproc, char *filename)
{
	if (dbproc->bcp_direction == 0) {
        _bcp_err_handler(dbproc, BCPEBCPI);
		return FAIL;
	}
	return SUCCEED;
}

RETCODE bcp_moretext(DBPROCESS *dbproc, DBINT size, BYTE *text)
{
#ifdef NCBI_FTDS
    static unsigned char txtptr_and_timestamp[24]= {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };

    int i;
    DBINT s;
    BCP_COLINFO *bcpcol;
    TDSSOCKET *tds = dbproc->tds_socket;

    if (dbproc == NULL || text== NULL || tds == NULL) {
        return FAIL;
    }

    if ( dbproc->text_sent == 0 ) {
        for (i=dbproc->curr_text_col; i < dbproc->bcp_colcount; i++) {
            if(dbproc->host_columns[i]->datatype == 0) continue;

            bcpcol = dbproc->bcp_columns[i];
            if (bcpcol == NULL) {
                return FAIL;
            }
            if (is_blob_type (bcpcol->db_type) ) { 
                dbproc->curr_text_col = i;
                tds_put_byte (tds,0x10);
                tds_put_n(tds, txtptr_and_timestamp, 24);
                tds_put_int (tds,  bcpcol->data_size);
                break;
            }
        }
        if(i >= dbproc->bcp_colcount) return FAIL;
    }
    if(dbproc->bcp_columns[dbproc->curr_text_col]->data_size == 0 &&
       size == 0) { /* this is a NULL */
        dbproc->curr_text_col++;
        dbproc->text_sent = 0;
        return SUCCEED;
    }
    s= dbproc->bcp_columns[dbproc->curr_text_col]->data_size - dbproc->text_sent;
    if(s <= 0) return FAIL;
    if(size > s) size= s;
    tds_put_n (tds, text, size);
    dbproc->text_sent += size;
    if (dbproc->text_sent >= dbproc->bcp_columns[dbproc->curr_text_col]->data_size) {
        dbproc->text_sent = 0;
        dbproc->curr_text_col++;
    }
#else
	if (dbproc->bcp_direction == 0) {
        _bcp_err_handler(dbproc, BCPEBCPI);
		return FAIL;
	}
#endif
	return SUCCEED;
}

RETCODE bcp_batch(DBPROCESS *dbproc)
{
TDSSOCKET *tds = dbproc->tds_socket;
int marker;
int rows_copied;

	if (dbproc->bcp_direction == 0) {
        _bcp_err_handler(dbproc, BCPEBCPI);
        /* return FAIL; */
        return -1;
	}

	tds_flush_packet(tds);

	do {

		marker = tds_get_byte(tds);
        if ( marker == TDS_DONE_TOKEN ) 
            rows_copied = tds_process_end(tds,marker,NULL,NULL);
        else 
			tds_process_default_tokens(tds, marker);

	} while (marker != TDS_DONE_TOKEN);


	_bcp_start_new_batch(dbproc);


    return rows_copied;
}

/* end the transfer of data from program variables */

RETCODE bcp_done(DBPROCESS *dbproc)
{

TDSSOCKET *tds = dbproc->tds_socket;
int marker;
int rows_copied = -1;

	if (dbproc->bcp_direction == 0) {
        _bcp_err_handler(dbproc, BCPEBCPI);
        /* return FAIL; */
        return -1;
	}
	tds_flush_packet(tds);

	do {

		marker = tds_get_byte(tds);
        if ( marker == TDS_DONE_TOKEN ) 
            rows_copied = tds_process_end(tds,marker,NULL,NULL);
        else 
			tds_process_default_tokens(tds, marker);

	} while (marker != TDS_DONE_TOKEN);

	_bcp_clear_storage(dbproc);

	return (rows_copied);

}

RETCODE bcp_cancel(DBPROCESS *dbproc)
{

TDSSOCKET *tds = dbproc->tds_socket;
int marker;
int rows_copied = -1;

	if (dbproc->bcp_direction == 0) {
        _bcp_err_handler(dbproc, BCPEBCPI);
        /* return FAIL; */
        return -1;
	}
    tds_send_cancel(tds);
    tds_process_cancel(tds);

    _bcp_clear_storage(dbproc);

	return SUCCEED;

}

/* bind a program host variable to a database column */

RETCODE bcp_bind(DBPROCESS *dbproc, BYTE *varaddr, int prefixlen, DBINT varlen,
	 BYTE * terminator, int termlen, int type, int table_column)
{

BCP_HOSTCOLINFO *hostcol;

	if (dbproc->bcp_direction == 0) {
        _bcp_err_handler(dbproc, BCPEBCPI);
		return FAIL;
	}

	if (dbproc->bcp_hostfile != (char *) NULL) {
        _bcp_err_handler(dbproc, BCPEBCPB);
		return FAIL;
	}

	if (dbproc->bcp_direction != DB_IN) {
        _bcp_err_handler(dbproc, BCPEBCPN);
		return FAIL;
	}

	if (varlen < -1) {
        _bcp_err_handler(dbproc, BCPEBCVLEN);
		return FAIL;
	}

    if (prefixlen != 0 && prefixlen != 1 &&
        prefixlen != 2 && prefixlen != 4 ) {
        _bcp_err_handler(dbproc, BCPEBCBPREF);
		return FAIL;
	}

    if (prefixlen == 0 && varlen == -1 && 
        termlen == -1 && !is_fixed_type(type)) {
		return FAIL;
	}

    if (is_fixed_type(type) && 
        (varlen != -1 && varlen != 0)) {
		return FAIL;
	}

	if (table_column > dbproc->host_colcount) {
		return FAIL;
	}

#ifndef NCBI_FTDS
    if (varaddr == (BYTE *)NULL && 
        (prefixlen != 0 || termlen != 0)) {
        _bcp_err_handler(dbproc, BCPEBCBNPR);
		return FAIL;
	}
#endif

	hostcol = dbproc->host_columns[table_column - 1];

	hostcol->host_var = varaddr;
	hostcol->datatype = type;
	hostcol->prefix_len = prefixlen;
	hostcol->column_len = varlen;
    if(termlen > 0) {
	hostcol->terminator = (BYTE *) malloc(termlen + 1);
	memcpy(hostcol->terminator, terminator, termlen);
    }
    else hostcol->terminator= NULL;
	hostcol->term_len = termlen;
	hostcol->tab_colnum = table_column;

	return SUCCEED;
}

/* for a bcp in from program variables, collate all the data */
/* into the column arrays held in dbproc...                  */

RETCODE _bcp_get_prog_data(DBPROCESS *dbproc)
{
    BCP_COLINFO *bcpcol;
    BCP_HOSTCOLINFO *hostcol;

    int i;
    TDS_TINYINT ti;
    TDS_SMALLINT si;
    TDS_INT li;
    TDS_INT desttype;
    int collen;
    int data_is_null;
    int bytes_read;
    int converted_data_size;

    BYTE *dataptr;

    /* for each host file column defined by calls to bcp_colfmt */

    for (i = 0; i < dbproc->host_colcount; i++) {
        BYTE coldata[8192];

        hostcol = dbproc->host_columns[i];
#ifdef NCBI_FTDS
        if(hostcol->datatype == 0) continue;
#endif
        dataptr = hostcol->host_var;

        data_is_null = 0;
        collen = 0;

        /* if a prefix length specified, read the correct */
        /* amount of data...                              */

        if (hostcol->prefix_len > 0) {

            switch (hostcol->prefix_len) {
            case 1:
                memcpy(&ti, dataptr, 1);
                dataptr += 1;
                collen = ti;
                break;
            case 2:
                memcpy(&si, dataptr, 2);
                dataptr += 2;
                collen = si;
                break;
            case 4:
                memcpy(&li, dataptr, 4);
                dataptr += 4;
                collen = li;
                break;
            }
            if (collen == 0)
                data_is_null = 1;

        }

        /* if (Max) column length specified take that into */
        /* consideration...                                */

        if (!data_is_null && hostcol->column_len >= 0) {
            if (hostcol->column_len == 0)
                data_is_null = 1;
            else {
                if (collen)
                    collen = (hostcol->column_len < collen) ? hostcol->column_len : collen;
                else
                    collen = hostcol->column_len;
            }
        }

        /* Fixed Length data - this over-rides anything else specified */

        if (is_fixed_type(hostcol->datatype)) {
            collen = get_size_by_type(hostcol->datatype);
        }
#ifdef NCBI_FTDS
        else if(is_numeric_type(hostcol->datatype)) {
            collen= sizeof(DBNUMERIC);
        }
#endif


        /* if this host file column contains table data,   */
        /* find the right element in the table/column list */

        if (hostcol->tab_colnum) {
            bcpcol = dbproc->bcp_columns[hostcol->tab_colnum - 1];
            if (bcpcol->tab_colnum != hostcol->tab_colnum) {
                return FAIL;
            }
        }
#ifdef NCBI_FTDS
        if(dataptr == NULL) {
            bcpcol->data_size= collen;
            continue;
        }
#endif


        /* a terminated field is specified - get the data into temporary space... */

        memset(coldata, '\0', sizeof(coldata));

        if (hostcol->term_len > 0) {
            bytes_read = _bcp_get_term_var(dataptr, hostcol->terminator, hostcol->term_len, coldata, sizeof(coldata));

            if (bytes_read == -1)
                return FAIL;


            if (collen)
                collen = (bytes_read < collen) ? bytes_read : collen;
            else
                collen = bytes_read;

            if (collen == 0)
                data_is_null = 1;

            if (hostcol->tab_colnum) {
                if (data_is_null) {
                    bcpcol->data_size = 0;
                }
                else {
                    desttype = tds_get_conversion_type(bcpcol->db_type, bcpcol->db_length);

                    if ((converted_data_size = dbconvert(dbproc, hostcol->datatype, 
                                                         (BYTE *) coldata, collen,
                                                         desttype, bcpcol->data, 
                                                         bcpcol->db_length)) == -1) {
                        return (FAIL);
                    }

                    bcpcol->data_size = converted_data_size;
                }
            }
        }
        else {
            if (collen) {
                if (collen > sizeof(coldata)) {
                    return (FAIL);
                }
                memcpy(coldata, dataptr, collen);
            }

            if (hostcol->tab_colnum) {
                if (data_is_null) {
                    bcpcol->data_size = 0;
                }
                else {
                    desttype = tds_get_conversion_type(bcpcol->db_type, bcpcol->db_length);

                    if ((converted_data_size = dbconvert(dbproc, hostcol->datatype, 
                                                         (BYTE *) coldata, collen,
                                                         desttype, bcpcol->data, 
                                                         bcpcol->db_length)) == -1) {
                        return (FAIL);
                    }

                    bcpcol->data_size = converted_data_size;
                }
            }
        }

    }
    return SUCCEED;
}

/* for bcp in from program variables, where the program data */
/* has been identified as character terminated, get the data */

RETCODE _bcp_get_term_var(BYTE *dataptr, BYTE *term, int term_len, BYTE *coldata, int coldata_size )
{

int bufpos = 0;
int found = 0;
BYTE *tptr;


	for (tptr = dataptr; !found; tptr++) {
		if (memcmp(tptr, term, term_len) != 0) {
            if (bufpos == coldata_size) {
                return (-1);
            }
			*(coldata + bufpos) = *tptr;
			bufpos++;
        }
        else
			found = 1;
	}
	if (found) {
		return (bufpos);
    }
    else {
		return (-1);
	}

}

static int _bcp_rtrim_varchar(char *istr, int ilen)
{
	char *t;
	int olen = ilen;

	for (t = istr + (ilen - 1); *t == ' '; t--) {
		*t = '\0';
		olen--;
	}
	return olen;
}

RETCODE _bcp_clear_storage(DBPROCESS *dbproc)
{

int i;

	if (dbproc->bcp_hostfile) {
		free(dbproc->bcp_hostfile);
		dbproc->bcp_hostfile = (char *) NULL;
	}

	if (dbproc->bcp_errorfile) {
		free(dbproc->bcp_errorfile);
		dbproc->bcp_errorfile = (char *) NULL;
	}

	if (dbproc->bcp_tablename) {
		free(dbproc->bcp_tablename);
		dbproc->bcp_tablename = (char *) NULL;
	}

	if (dbproc->bcp_insert_stmt) {
		free(dbproc->bcp_insert_stmt);
		dbproc->bcp_insert_stmt = (char *) NULL;
	}

	dbproc->bcp_direction = 0;

	if (dbproc->bcp_columns) {
		for (i = 0; i < dbproc->bcp_colcount; i++) {
			if (dbproc->bcp_columns[i]->data) {
				free(dbproc->bcp_columns[i]->data);
				dbproc->bcp_columns[i]->data = NULL;
			}
			free(dbproc->bcp_columns[i]);
			dbproc->bcp_columns[i] = NULL;
		}
		free(dbproc->bcp_columns);
		dbproc->bcp_columns = NULL;
	}

	dbproc->bcp_colcount = 0;
#ifdef NCBI_FTDS
    dbproc->curr_text_col= 0;
#endif

	/* free up storage that holds details of hostfile columns */

	if (dbproc->host_columns) {
		for (i = 0; i < dbproc->host_colcount; i++) {
			if (dbproc->host_columns[i]->terminator) {
				free(dbproc->host_columns[i]->terminator);
				dbproc->host_columns[i]->terminator = NULL;
			}
			free(dbproc->host_columns[i]);
			dbproc->host_columns[i] = NULL;
		}
		free(dbproc->host_columns);
		dbproc->host_columns = NULL;
	}
#ifdef NCBI_FTDS
   if(dbproc->bcp_hint) {
       free(dbproc->bcp_hint);
       dbproc->bcp_hint= NULL;
   }
#endif
	dbproc->host_colcount = 0;

	dbproc->var_cols = 0;
	dbproc->sendrow_init = 0;

	return (SUCCEED);

}


RETCODE _bcp_err_handler(DBPROCESS *dbproc, int bcp_errno)
{
	const char *errmsg;
	char *p;
	int severity;
	int erc;

	switch (bcp_errno) {

	case SYBETTS:
		errmsg = "The table which bulk-copy is attempting to copy to a "
			"host-file is shorter than the number of rows which bulk-copy " "was instructed to skip.";
		severity = EXUSER;
		break;

	case SYBEBDIO:
		errmsg = "Bad bulk-copy direction. Must be either IN or OUT.";
		severity = EXPROGRAM;
		break;

	case SYBEBCVH:
		errmsg = "bcp_exec() may be called only after bcp_init() has " "been passed a valid host file.";
		severity = EXPROGRAM;
		break;

	case SYBEBIVI:
		errmsg = "bcp_columns(), bcp_colfmt() and * bcp_colfmt_ps() may be used "
			"only after bcp_init() has been passed a valid input file.";
		severity = EXPROGRAM;
		break;

	case SYBEBCBC:
		errmsg = "bcp_columns() must be called before bcp_colfmt().";
		severity = EXPROGRAM;
		break;

	case SYBEBCFO:
		errmsg = "Bcp host-files must contain at least one column.";
		severity = EXUSER;
		break;

	case SYBEBCPB:
		errmsg = "bcp_bind(), bcp_moretext() and bcp_sendrow() * may NOT be used "
			"after bcp_init() has been passed a non-NULL input file name.";
		severity = EXPROGRAM;
		break;

	case SYBEBCPN:
		errmsg = "bcp_bind(), bcp_collen(), bcp_colptr(), bcp_moretext() and "
			"bcp_sendrow() may be used only after bcp_init() has been called " "with the copy direction set to DB_IN.";
		severity = EXPROGRAM;
		break;

	case SYBEBCPI:
		errmsg = "bcp_init() must be called before any other bcp routines.";
		severity = EXPROGRAM;
		break;

	case SYBEBCITBNM:
		errmsg = "bcp_init(): tblname parameter cannot be NULL.";
		severity = EXPROGRAM;
		break;

	case SYBEBCITBLEN:
		errmsg = "bcp_init(): tblname parameter is too long.";
		severity = EXPROGRAM;
		break;

	case SYBEBCBNPR:
		errmsg = "bcp_bind(): if varaddr is NULL, prefixlen must be 0 and no " "terminator should be ** specified.";
		severity = EXPROGRAM;
		break;

	case SYBEBCBPREF:
		errmsg = "Illegal prefix length. Legal values are 0, 1, 2 or 4.";
		severity = EXPROGRAM;
		break;

	case SYBEVDPT:
		errmsg = "For bulk copy, all variable-length data * must have either " "a length-prefix or a terminator specified.";
		severity = EXUSER;
		break;

	case SYBEBCPCTYP:
		errmsg = "bcp_colfmt(): If table_colnum is 0, ** host_type cannot be 0.";
		severity = EXPROGRAM;
		break;

	case SYBEBCHLEN:
		errmsg = "host_collen should be greater than or equal to -1.";
		severity = EXPROGRAM;
		break;

	case SYBEBCPREF:
		errmsg = "Illegal prefix length. Legal values are -1, 0, 1, 2 or 4.";
		severity = EXPROGRAM;
		break;

	case SYBEBCVLEN:
		errmsg = "varlen should be greater than or equal to -1.";
		severity = EXPROGRAM;
		break;

	case SYBEBCUO:
		errmsg = "Unable to open host data-file.";
		severity = EXRESOURCE;
		break;

	case SYBEBUOE:
		erc = asprintf( &p, "Unable to open bcp error file \"%s\".", dbproc->bcp_errorfile );
		if (p && erc != -1) {
          if(g_dblib_err_handler) {
              g_dblib_err_handler(dbproc, EXRESOURCE, bcp_errno, 0, errmsg, p);
          }
          free (p);
          return SUCCEED;
		}
		/* try to print silly message if unable to allocate memory */
		errmsg = "Unable to open error file.";
		severity = EXRESOURCE;
		break;

	case SYBEBUOF:
		errmsg = "Unable to open format-file.";
		severity = EXPROGRAM;
		break;

	case SYBEBUDF:
		errmsg = "Unrecognized datatype found in format-file.";
		severity = EXPROGRAM;
		break;

	case SYBEBIHC:
		errmsg = "Incorrect host-column number found in bcp format-file.";
		severity = EXPROGRAM;
		break;

	case SYBEBCUC:
		errmsg = "Unable to close host data-file.";
		severity = EXRESOURCE;
		break;

	case SYBEBUCE:
		errmsg = "Unable to close error file.";
		severity = EXPROGRAM;
		break;

	case SYBEBUCF:
		errmsg = "Unable to close format-file.";
		severity = EXPROGRAM;
		break;

	case SYBEIFNB:
		errmsg = "Illegal field number passed to bcp_control().";
		severity = EXPROGRAM;
		break;

	case SYBEBCRE:
		errmsg = "I/O error while reading bcp data-file.";
		severity = EXNONFATAL;
		break;

	case SYBEBCNN:
		errmsg = "Attempt to bulk-copy a NULL value into Server column which " "does not accept NULL values.";
		severity = EXUSER;
		break;

	case SYBEBBCI:
		errmsg = "Batch successfully bulk-copied to SQL Server.";
		severity = EXINFO;
		break;

	default:
		errmsg = "Unknown bcp error";
		severity = EXNONFATAL;
		break;
	}

    if(g_dblib_err_handler) {
        g_dblib_err_handler(dbproc, severity, bcp_errno, 0, errmsg, "");
        return (SUCCEED);
    }
}
