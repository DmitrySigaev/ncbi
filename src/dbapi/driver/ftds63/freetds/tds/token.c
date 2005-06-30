/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004  Brian Bruns
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include <assert.h>

#include "tds.h"
#include "tdsconvert.h"
#include "tdsiconv.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id$";
static void *no_unused_var_warn[] = { software_version,
	no_unused_var_warn
};

static int tds_process_msg(TDSSOCKET * tds, int marker);
static int tds_process_compute_result(TDSSOCKET * tds);
static int tds_process_compute_names(TDSSOCKET * tds);
static int tds7_process_compute_result(TDSSOCKET * tds);
static int tds_process_result(TDSSOCKET * tds);
static int tds_process_col_name(TDSSOCKET * tds);
static int tds_process_col_fmt(TDSSOCKET * tds);
static int tds_process_colinfo(TDSSOCKET * tds);
static int tds_process_compute(TDSSOCKET * tds, TDS_INT * computeid);
static int tds_process_cursor_tokens(TDSSOCKET * tds);
static int tds_process_row(TDSSOCKET * tds);
static int tds_process_param_result(TDSSOCKET * tds, TDSPARAMINFO ** info);
static int tds7_process_result(TDSSOCKET * tds);
static TDSDYNAMIC *tds_process_dynamic(TDSSOCKET * tds);
static int tds_process_auth(TDSSOCKET * tds);
static int tds_get_data(TDSSOCKET * tds, TDSCOLUMN * curcol, unsigned char *current_row, int i);
static int tds_get_data_info(TDSSOCKET * tds, TDSCOLUMN * curcol, int is_param);
static int tds_process_env_chg(TDSSOCKET * tds);
static const char *_tds_token_name(unsigned char marker);
static int tds_process_param_result_tokens(TDSSOCKET * tds);
static int tds_process_params_result_token(TDSSOCKET * tds);
static int tds_process_dyn_result(TDSSOCKET * tds);
static int tds5_get_varint_size(int datatype);
static int tds5_process_result(TDSSOCKET * tds);
static int tds5_process_dyn_result2(TDSSOCKET * tds);
static void adjust_character_column_size(const TDSSOCKET * tds, TDSCOLUMN * curcol);
static int determine_adjusted_size(const TDSICONV * char_conv, int size);
static int tds_process_default_tokens(TDSSOCKET * tds, int marker);
static TDS_INT tds_process_end(TDSSOCKET * tds, int marker, int *flags_parm);
static int _tds_process_row_tokens(TDSSOCKET * tds, TDS_INT * rowtype, TDS_INT * computeid, TDS_INT read_end_token);
static const char *tds_pr_op(int op);


/**
 * \ingroup libtds
 * \defgroup token Results processing
 * Handle tokens in packets. Many PDU (packets data unit) contain tokens.
 * (like result description, rows, data, errors and many other).
 */


/** \addtogroup token
 *  \@{ 
 */

/**
 * tds_process_default_tokens() is a catch all function that is called to
 * process tokens not known to other tds_process_* routines
 */
static int
tds_process_default_tokens(TDSSOCKET * tds, int marker)
{
	int tok_size;
	int done_flags;
	TDS_INT ret_status;

	tdsdump_log(TDS_DBG_FUNC, "tds_process_default_tokens() marker is %x(%s)\n", marker, _tds_token_name(marker));

	if (IS_TDSDEAD(tds)) {
		tdsdump_log(TDS_DBG_FUNC, "leaving tds_process_default_tokens() connection dead\n");
		tds->state = TDS_DEAD;
		return TDS_FAIL;
	}

	switch (marker) {
	case TDS_AUTH_TOKEN:
		return tds_process_auth(tds);
		break;
	case TDS_ENVCHANGE_TOKEN:
		return tds_process_env_chg(tds);
		break;
	case TDS_DONE_TOKEN:
	case TDS_DONEPROC_TOKEN:
	case TDS_DONEINPROC_TOKEN:
		return tds_process_end(tds, marker, &done_flags);
		break;
	case TDS_PROCID_TOKEN:
		tds_get_n(tds, NULL, 8);
		break;
	case TDS_RETURNSTATUS_TOKEN:
		ret_status = tds_get_int(tds);
		marker = tds_peek(tds);
		if (marker != TDS_PARAM_TOKEN && marker != TDS_DONEPROC_TOKEN && marker != TDS_DONE_TOKEN)
			break;
		tds->has_status = 1;
		tds->ret_status = ret_status;
		tdsdump_log(TDS_DBG_FUNC, "tds_process_default_tokens: return status is %d\n", tds->ret_status);
		break;
	case TDS_ERROR_TOKEN:
	case TDS_INFO_TOKEN:
	case TDS_EED_TOKEN:
		return tds_process_msg(tds, marker);
		break;
	case TDS_CAPABILITY_TOKEN:
		/* TODO split two part of capability and use it */
		tok_size = tds_get_smallint(tds);
		/* vicm */
		/* Sybase 11.0 servers return the wrong length in the capability packet, causing use to read
		 * past the done packet.
		 */
		if (!TDS_IS_MSSQL(tds) && tds->product_version < TDS_SYB_VER(12, 0, 0)) {
			unsigned char type, size, *p, *pend;

			p = tds->capabilities;
			pend = tds->capabilities + TDS_MAX_CAPABILITY;

			do {
				type = tds_get_byte(tds);
				size = tds_get_byte(tds);
				if ((p + 2) > pend)
					break;
				*p++ = type;
				*p++ = size;
				if ((p + size) > pend)
					break;
				if (tds_get_n(tds, p, size) == NULL)
					return TDS_FAIL;
			} while (type != 2);
		} else {
			if (tds_get_n(tds, tds->capabilities, tok_size > TDS_MAX_CAPABILITY ? TDS_MAX_CAPABILITY : tok_size) ==
			    NULL)
				return TDS_FAIL;
		}
		break;
		/* PARAM_TOKEN can be returned inserting text in db, to return new timestamp */
	case TDS_PARAM_TOKEN:
		tds_unget_byte(tds);
		return tds_process_param_result_tokens(tds);
		break;
	case TDS7_RESULT_TOKEN:
		return tds7_process_result(tds);
		break;
	case TDS_OPTIONCMD_TOKEN:
		tdsdump_log(TDS_DBG_FUNC, "option command token encountered\n");
		break;
	case TDS_RESULT_TOKEN:
		return tds_process_result(tds);
		break;
	case TDS_ROWFMT2_TOKEN:
		return tds5_process_result(tds);
		break;
	case TDS_COLNAME_TOKEN:
		return tds_process_col_name(tds);
		break;
	case TDS_COLFMT_TOKEN:
		return tds_process_col_fmt(tds);
		break;
	case TDS_ROW_TOKEN:
		return tds_process_row(tds);
		break;
	case TDS5_PARAMFMT_TOKEN:
		/* store discarded parameters in param_info, not in old dynamic */
		tds->cur_dyn = NULL;
		return tds_process_dyn_result(tds);
		break;
	case TDS5_PARAMFMT2_TOKEN:
		tds->cur_dyn = NULL;
		return tds5_process_dyn_result2(tds);
		break;
	case TDS5_PARAMS_TOKEN:
		/* save params */
		return tds_process_params_result_token(tds);
		break;
	case TDS_CURINFO_TOKEN:
		return tds_process_cursor_tokens(tds);
		break;
	case TDS5_DYNAMIC_TOKEN:
	case TDS_LOGINACK_TOKEN:
	case TDS_ORDERBY_TOKEN:
	case TDS_CONTROL_TOKEN:
	case TDS_TABNAME_TOKEN:	/* used for FOR BROWSE query */
		tdsdump_log(TDS_DBG_WARN, "%s:%d: Eating %s token\n", __FILE__, __LINE__, _tds_token_name(marker));
		tds_get_n(tds, NULL, tds_get_smallint(tds));
		break;
	case TDS_COLINFO_TOKEN:
		return tds_process_colinfo(tds);
		break;
	case TDS_ORDERBY2_TOKEN:
		tdsdump_log(TDS_DBG_WARN, "%s:%d: Eating %s token\n", __FILE__, __LINE__, _tds_token_name(marker));
		tds_get_n(tds, NULL, tds_get_int(tds));
		break;
	default:
		tds_client_msg(tds->tds_ctx, tds, 20020, 9, 0, 0, "Unknown marker");
		if (IS_TDSDEAD(tds))
			tds->state = TDS_DEAD;
		else
			tds_close_socket(tds);
		tdsdump_log(TDS_DBG_ERROR, "Unknown marker: %d(%x)!!\n", marker, (unsigned char) marker);
		return TDS_FAIL;
	}
	return TDS_SUCCEED;
}

static int
tds_set_spid(TDSSOCKET * tds)
{
	TDS_INT result_type;
	TDS_INT done_flags;
	TDS_INT row_type;
	TDS_INT compute_id;
	TDS_INT rc;
	TDSCOLUMN *curcol;

	if (tds_submit_query(tds, "select @@spid") != TDS_SUCCEED) {
		return TDS_FAIL;
	}

	while ((rc = tds_process_result_tokens(tds, &result_type, &done_flags)) == TDS_SUCCEED) {

		switch (result_type) {

			case TDS_ROWFMT_RESULT:
				if (tds->res_info->num_cols != 1) 
					return TDS_FAIL;
				break;

			case TDS_ROW_RESULT:
				while ((rc = tds_process_row_tokens(tds, &row_type, &compute_id)) == TDS_SUCCEED);

				if (rc != TDS_NO_MORE_ROWS)
					return TDS_FAIL;

				curcol = tds->res_info->columns[0];
				if (curcol->column_type == SYBINT2 || (curcol->column_type == SYBINTN && curcol->column_size == 2)) {
					tds->spid = *((TDS_USMALLINT *) (tds->res_info->current_row + curcol->column_offset));
				} else if (curcol->column_type == SYBINT4 || (curcol->column_type == SYBINTN && curcol->column_size == 4)) {
					tds->spid = *((TDS_UINT *) (tds->res_info->current_row + curcol->column_offset));
				} else
					return TDS_FAIL;
				break;

			case TDS_DONE_RESULT:
				if ((done_flags & TDS_DONE_ERROR) != 0)
					return TDS_FAIL;
				break;

			default:
				break;
		}
	}
	if (rc != TDS_NO_MORE_RESULTS) 
		return TDS_FAIL;

	return TDS_SUCCEED;
}

/**
 * tds_process_login_tokens() is called after sending the login packet 
 * to the server.  It returns the success or failure of the login 
 * dependent on the protocol version. 4.2 sends an ACK token only when
 * successful, TDS 5.0 sends it always with a success byte within
 */
int
tds_process_login_tokens(TDSSOCKET * tds)
{
	int succeed = TDS_FAIL;
	int marker;
	int len;
	int memrc = 0;
	unsigned char major_ver, minor_ver;
	unsigned char ack;
	TDS_UINT product_version;

	tdsdump_log(TDS_DBG_FUNC, "tds_process_login_tokens()\n");
	/* get_incoming(tds->s); */
	do {
		marker = tds_get_byte(tds);
		tdsdump_log(TDS_DBG_FUNC, "looking for login token, got  %x(%s)\n", marker, _tds_token_name(marker));

		switch (marker) {
		case TDS_AUTH_TOKEN:
			tds_process_auth(tds);
			break;
		case TDS_LOGINACK_TOKEN:
			/* TODO function */
			len = tds_get_smallint(tds);
			ack = tds_get_byte(tds);
			major_ver = tds_get_byte(tds);
			minor_ver = tds_get_byte(tds);
			tds_get_n(tds, NULL, 2);
			/* ignore product name length, see below */
			tds_get_byte(tds);
			product_version = 0;
			/* get server product name */
			/* compute length from packet, some version seem to fill this information wrongly */
			len -= 10;
			if (tds->product_name)
				free(tds->product_name);
			if (major_ver >= 7) {
				product_version = 0x80000000u;
				memrc += tds_alloc_get_string(tds, &tds->product_name, len / 2);
			} else if (major_ver >= 5) {
				memrc += tds_alloc_get_string(tds, &tds->product_name, len);
			} else {
				memrc += tds_alloc_get_string(tds, &tds->product_name, len);
				if (tds->product_name != NULL && strstr(tds->product_name, "Microsoft"))
					product_version = 0x80000000u;
			}
			product_version |= ((TDS_UINT) tds_get_byte(tds)) << 24;
			product_version |= ((TDS_UINT) tds_get_byte(tds)) << 16;
			product_version |= ((TDS_UINT) tds_get_byte(tds)) << 8;
			product_version |= tds_get_byte(tds);
			/*
			 * MSSQL 6.5 and 7.0 seem to return strange values for this
			 * using TDS 4.2, something like 5F 06 32 FF for 6.50
			 */
			if (major_ver == 4 && minor_ver == 2 && (product_version & 0xff0000ffu) == 0x5f0000ffu)
				product_version = ((product_version & 0xffff00u) | 0x800000u) << 8;
			tds->product_version = product_version;
#ifdef WORDS_BIGENDIAN
			/* TODO do a best check */
/*
				
				if (major_ver==7) {
					tds->broken_dates=1;
				}
*/
#endif
			/*
			 * TDS 5.0 reports 5 on success 6 on failure
			 * TDS 4.2 reports 1 on success and is not
			 * present on failure
			 */
			if (ack == 5 || ack == 1)
				succeed = TDS_SUCCEED;
			break;
		default:
			if (tds_process_default_tokens(tds, marker) == TDS_FAIL)
				return TDS_FAIL;
			break;
		}
	} while (marker != TDS_DONE_TOKEN);
	/* TODO why ?? */
	tds->spid = tds->rows_affected;
	if (tds->spid == 0) {
		if (tds_set_spid(tds) != TDS_SUCCEED) {
			tdsdump_log(TDS_DBG_ERROR, "tds_set_spid() failed\n");
			succeed = TDS_FAIL;
		}
	}
	tdsdump_log(TDS_DBG_FUNC, "leaving tds_process_login_tokens() returning %d\n", succeed);
	if (memrc != 0)
		succeed = TDS_FAIL;
	return succeed;
}

static int
tds_process_auth(TDSSOCKET * tds)
{
	int pdu_size;
	unsigned char nonce[8];

/* char domain[30]; */
	int where;

#if ENABLE_EXTRA_CHECKS
	if (!IS_TDS7_PLUS(tds))
		tdsdump_log(TDS_DBG_ERROR, "Called auth on TDS version < 7\n");
#endif

	pdu_size = tds_get_smallint(tds);
	tdsdump_log(TDS_DBG_INFO1, "TDS_AUTH_TOKEN PDU size %d\n", pdu_size);
	
	/* at least 32 bytes (till context) */
	if (pdu_size < 32)
		return TDS_FAIL;

	/* TODO check first 2 values */
	tds_get_n(tds, NULL, 8);	/* NTLMSSP\0 */
	tds_get_int(tds);	/* sequence -> 2 */
	tds_get_n(tds, NULL, 4);	/* domain len (2 time) */
	tds_get_int(tds);	/* domain offset */
	/* TODO use them */
	tds_get_n(tds, NULL, 4);	/* flags */
	tds_get_n(tds, nonce, 8);
	tdsdump_dump_buf(TDS_DBG_INFO1, "TDS_AUTH_TOKEN nonce", nonce, 8);
	where = 32;

	/*
	 * tds_get_string(tds, domain, domain_len); 
	 * tdsdump_log(TDS_DBG_INFO1, "TDS_AUTH_TOKEN domain %s\n", domain);
	 * where += strlen(domain);
	 */

	if (pdu_size < where)
		return TDS_FAIL;
	/* discard context, target and data informations */
	tds_get_n(tds, NULL, pdu_size - where);
	tdsdump_log(TDS_DBG_INFO1, "Draining %d bytes\n", pdu_size - where);

	tds7_send_auth(tds, nonce);

	return TDS_SUCCEED;
}

/**
 * process TDS result-type message streams.
 * tds_process_result_tokens() is called after submitting a query with
 * tds_submit_query() and is responsible for calling the routines to
 * populate tds->res_info if appropriate (some query have no result sets)
 * @param tds A pointer to the TDSSOCKET structure managing a client/server operation.
 * @param result_type A pointer to an integer variable which 
 *        tds_process_result_tokens sets to indicate the current type of result.
 *  @par
 *  <b>Values that indicate command status</b>
 *  <table>
 *   <tr><td>TDS_DONE_RESULT</td><td>The results of a command have been completely processed. This command return no rows.</td></tr>
 *   <tr><td>TDS_DONEPROC_RESULT</td><td>The results of a  command have been completely processed. This command return rows.</td></tr>
 *   <tr><td>TDS_DONEINPROC_RESULT</td><td>The results of a  command have been completely processed. This command return rows.</td></tr>
 *  </table>
 *  <b>Values that indicate results information is available</b>
 *  <table><tr>
 *    <td>TDS_ROWFMT_RESULT</td><td>Regular Data format information</td>
 *    <td>tds->res_info now contains the result details ; tds->current_results now points to that data</td>
 *   </tr><tr>
 *    <td>TDS_COMPUTEFMT_ RESULT</td><td>Compute data format information</td>
 *    <td>tds->comp_info now contains the result data; tds->current_results now points to that data</td>
 *   </tr><tr>
 *    <td>TDS_DESCRIBE_RESULT</td><td></td>
 *    <td></td>
 *  </tr></table>
 *  <b>Values that indicate data is available</b>
 *  <table><tr>
 *   <td><b>Value</b></td><td><b>Meaning</b></td><td><b>Information returned</b></td>
 *   </tr><tr>
 *    <td>TDS_ROW_RESULT</td><td>Regular row results</td>
 *    <td>1 or more rows of regular data can now be retrieved</td>
 *   </tr><tr>
 *    <td>TDS_COMPUTE_RESULT</td><td>Compute row results</td>
 *    <td>A single row of compute data can now be retrieved</td>
 *   </tr><tr>
 *    <td>TDS_PARAM_RESULT</td><td>Return parameter results</td>
 *    <td>param_info or cur_dyn->params contain returned parameters</td>
 *   </tr><tr>
 *    <td>TDS_STATUS_RESULT</td><td>Stored procedure status results</td>
 *    <td>tds->ret_status contain the returned code</td>
 *  </tr></table>
 * @todo Complete TDS_DESCRIBE_RESULT description
 * @retval TDS_SUCCEED if a result set is available for processing.
 * @retval TDS_NO_MORE_RESULTS if all results have been completely processed.
 * @par Examples
 * The following code is cut from ct_results(), the ct-library function
 * @include token1.c
 */
int
tds_process_result_tokens(TDSSOCKET * tds, TDS_INT * result_type, int *done_flags)
{
	int marker;
	TDSPARAMINFO *pinfo = (TDSPARAMINFO *)NULL;
	TDSCOLUMN   *curcol;
	int rc;
	int saved_rows_affected = tds->rows_affected;
	TDS_INT ret_status;

	if (tds->state == TDS_IDLE) {
		tdsdump_log(TDS_DBG_FUNC, "tds_process_result_tokens() state is COMPLETED\n");
		*result_type = TDS_DONE_RESULT;
		return TDS_NO_MORE_RESULTS;
	}

	for (;;) {

		marker = tds_get_byte(tds);
		tdsdump_log(TDS_DBG_INFO1, "processing result tokens.  marker is  %x(%s)\n", marker, _tds_token_name(marker));

		switch (marker) {
		case TDS7_RESULT_TOKEN:
			rc = tds7_process_result(tds);

			/*
			 * If we're processing the results of a cursor fetch
			 * from sql server we don't want to pass back the
			 * TDS_ROWFMT_RESULT to the calling API
			 */

			if (tds->internal_sp_called == TDS_SP_CURSORFETCH) {
				marker = tds_get_byte(tds);
				if (marker != TDS_TABNAME_TOKEN) {
					tds_unget_byte(tds);
				} else {
					if (tds_process_default_tokens(tds, marker) == TDS_FAIL)
						return TDS_FAIL;
					marker = tds_get_byte(tds);
					if (marker != TDS_COLINFO_TOKEN) {
						tds_unget_byte(tds);
					} else {
						tds_process_colinfo(tds);
					}
				}
			} else {
				*result_type = TDS_ROWFMT_RESULT;
				/*
				 * handle browse information (if presents)
				 * TODO copied from below, function or put in results process 
				 */
				marker = tds_get_byte(tds);
				if (marker != TDS_TABNAME_TOKEN) {
					tds_unget_byte(tds);
					return TDS_SUCCEED;
				}
				if (tds_process_default_tokens(tds, marker) == TDS_FAIL)
					return TDS_FAIL;
				marker = tds_get_byte(tds);
				if (marker != TDS_COLINFO_TOKEN) {
					tds_unget_byte(tds);
					return TDS_SUCCEED;
				}
				if (rc == TDS_FAIL)
					return TDS_FAIL;
				else {
					tds_process_colinfo(tds);
					return TDS_SUCCEED;
				}
			}
			break;
		case TDS_RESULT_TOKEN:
			*result_type = TDS_ROWFMT_RESULT;
			return tds_process_result(tds);
			break;
		case TDS_ROWFMT2_TOKEN:
			*result_type = TDS_ROWFMT_RESULT;
			return tds5_process_result(tds);
			break;
		case TDS_COLNAME_TOKEN:
			tds_process_col_name(tds);
			break;
		case TDS_COLFMT_TOKEN:
			rc = tds_process_col_fmt(tds);
			*result_type = TDS_ROWFMT_RESULT;
			/* handle browse information (if presents) */
			marker = tds_get_byte(tds);
			if (marker != TDS_TABNAME_TOKEN) {
				tds_unget_byte(tds);
				return rc;
			}
			if (tds_process_default_tokens(tds, marker) == TDS_FAIL)
				return TDS_FAIL;
			marker = tds_get_byte(tds);
			if (marker != TDS_COLINFO_TOKEN) {
				tds_unget_byte(tds);
				return rc;
			}
			if (rc == TDS_FAIL)
				return TDS_FAIL;
			else {
				tds_process_colinfo(tds);
				return TDS_SUCCEED;
			}
			break;
		case TDS_PARAM_TOKEN:
			tds_unget_byte(tds);
			if (tds->internal_sp_called) {
				tdsdump_log(TDS_DBG_FUNC, "processing parameters for sp %d\n", tds->internal_sp_called);
				while ((marker = tds_get_byte(tds)) == TDS_PARAM_TOKEN) {
					tdsdump_log(TDS_DBG_INFO1, "calling tds_process_param_result\n");
					tds_process_param_result(tds, &pinfo);
				}
				tds_unget_byte(tds);
				tdsdump_log(TDS_DBG_FUNC, "no of hidden return parameters %d\n", pinfo->num_cols);
				if(tds->internal_sp_called == TDS_SP_CURSOROPEN) {
					curcol = pinfo->columns[0];
					if (tds->client_cursor_id) {
						TDSCURSOR  *cursor;

						tdsdump_log(TDS_DBG_FUNC, "locating cursor_id %d\n", tds->client_cursor_id);
						cursor = tds->cursor; 
						while (cursor && cursor->client_cursor_id != tds->client_cursor_id)
							cursor = cursor->next;
					
						if (!cursor) {
							tdsdump_log(TDS_DBG_FUNC, "tds_process_result_tokens() : cannot find cursor_id %d\n", tds->client_cursor_id);
							return TDS_FAIL;
						}
						cursor->cursor_id = *(TDS_INT *) &(pinfo->current_row[curcol->column_offset]);
						tdsdump_log(TDS_DBG_FUNC, "stored internal cursor id %d in client cursor id %d \n", 
							    cursor->cursor_id, cursor->client_cursor_id);
					} 
				}
				if(tds->internal_sp_called == TDS_SP_PREPARE) {
					curcol = pinfo->columns[0];
					if (tds->cur_dyn && tds->cur_dyn->num_id == 0 && !tds_get_null(pinfo->current_row, 0)) {
						tds->cur_dyn->num_id = *(TDS_INT *) &(pinfo->current_row[curcol->column_offset]);
					}
				}
				tds_free_param_results(pinfo);
			} else {
				tds_process_param_result_tokens(tds);
				*result_type = TDS_PARAM_RESULT;
				return TDS_SUCCEED;
			}
			break;
		case TDS_COMPUTE_NAMES_TOKEN:
			return tds_process_compute_names(tds);
			break;
		case TDS_COMPUTE_RESULT_TOKEN:
			*result_type = TDS_COMPUTEFMT_RESULT;
			return tds_process_compute_result(tds);
			break;
		case TDS7_COMPUTE_RESULT_TOKEN:
			tds7_process_compute_result(tds);
			*result_type = TDS_COMPUTEFMT_RESULT;
			return TDS_SUCCEED;
			break;
		case TDS_ROW_TOKEN:
			/* overstepped the mark... */
			*result_type = TDS_ROW_RESULT;
			if (tds->client_cursor_id) {
				TDSCURSOR  *cursor;

				cursor = tds->cursor; 
				while (cursor && cursor->client_cursor_id != tds->client_cursor_id)
					cursor = cursor->next;
			
				if (!cursor) {
					tdsdump_log(TDS_DBG_FUNC, "tds_process_result_tokens() : cannot find cursor_id %d\n", tds->client_cursor_id);
					return TDS_FAIL;
				}
				tds->current_results = cursor->res_info;
				tdsdump_log(TDS_DBG_INFO1, "tds_process_result_tokens(). set current_results to cursor->res_info\n");
			} else {
				if (tds->res_info)
					tds->current_results = tds->res_info;
			}
			tds->current_results->rows_exist = 1;
			tds_unget_byte(tds);
			return TDS_SUCCEED;
			break;
		case TDS_CMP_ROW_TOKEN:
			*result_type = TDS_COMPUTE_RESULT;
			tds->res_info->rows_exist = 1;
			tds_unget_byte(tds);
			return TDS_SUCCEED;
			break;
		case TDS_RETURNSTATUS_TOKEN:
			ret_status = tds_get_int(tds);
			marker = tds_peek(tds);
			if (marker != TDS_PARAM_TOKEN && marker != TDS_DONEPROC_TOKEN && marker != TDS_DONE_TOKEN)
				break;
			if (tds->internal_sp_called) {
				/* TODO perhaps we should use ret_status ?? */
			} else {
				tds->has_status = 1;
				tds->ret_status = ret_status;
				*result_type = TDS_STATUS_RESULT;
				tdsdump_log(TDS_DBG_FUNC, "tds_process_result_tokens: return status is %d\n", tds->ret_status);
				return TDS_SUCCEED;
			}
			break;
		case TDS5_DYNAMIC_TOKEN:
			/* process acknowledge dynamic */
			tds->cur_dyn = tds_process_dynamic(tds);
			/* special case, prepared statement cannot be prepared */
			if (!tds->cur_dyn || tds->cur_dyn->emulated)
				break;
			marker = tds_get_byte(tds);
			if (marker != TDS_EED_TOKEN) {
				tds_unget_byte(tds);
				break;
			}
			tds_process_msg(tds, marker);
			if (!tds->cur_dyn || !tds->cur_dyn->emulated)
				break;
			marker = tds_get_byte(tds);
			if (marker != TDS_DONE_TOKEN) {
				tds_unget_byte(tds);
				break;
			}
			tds_process_end(tds, marker, done_flags);
			if (done_flags)
				*done_flags &= ~TDS_DONE_ERROR;
			*result_type = TDS_DONE_RESULT;
			return TDS_SUCCEED;
			break;
		case TDS5_PARAMFMT_TOKEN:
			tds_process_dyn_result(tds);
			*result_type = TDS_DESCRIBE_RESULT;
			return TDS_SUCCEED;
			break;
		case TDS5_PARAMFMT2_TOKEN:
			tds5_process_dyn_result2(tds);
			*result_type = TDS_DESCRIBE_RESULT;
			return TDS_SUCCEED;
			break;
		case TDS5_PARAMS_TOKEN:
			tds_process_params_result_token(tds);
			*result_type = TDS_PARAM_RESULT;
			return TDS_SUCCEED;
			break;
		case TDS_CURINFO_TOKEN:
			tds_process_cursor_tokens(tds);
			break;
		case TDS_DONE_TOKEN:
			tds_process_end(tds, marker, done_flags);
			*result_type = TDS_DONE_RESULT;
			return TDS_SUCCEED;
		case TDS_DONEPROC_TOKEN:
			tds_process_end(tds, marker, done_flags);
			switch (tds->internal_sp_called ) {
				case 0: 
				case TDS_SP_PREPARE: 
					*result_type = TDS_DONEPROC_RESULT;
					return TDS_SUCCEED;
					break;
				case TDS_SP_CURSOROPEN: 
					*result_type       = TDS_DONE_RESULT;
					tds->rows_affected = saved_rows_affected;
					return TDS_SUCCEED;
					break;
				default:
					*result_type = TDS_NO_MORE_RESULTS;
					return TDS_NO_MORE_RESULTS;
					break;
			}
			break;
		case TDS_DONEINPROC_TOKEN:
			tds_process_end(tds, marker, done_flags);
			if (tds->internal_sp_called == TDS_SP_CURSOROPEN  ||
				tds->internal_sp_called == TDS_SP_CURSORFETCH ||
				tds->internal_sp_called == TDS_SP_CURSORCLOSE ) {
				if (tds->rows_affected != TDS_NO_COUNT) {
					saved_rows_affected = tds->rows_affected;
				}
			} else {
				*result_type = TDS_DONEINPROC_RESULT;
				return TDS_SUCCEED;
			}
			break;
		default:
			if (tds_process_default_tokens(tds, marker) == TDS_FAIL) {
				return TDS_FAIL;
			}
			break;
		}
	}
}

/**
 * process TDS row-type message streams.
 * tds_process_row_tokens() is called once a result set has been obtained
 * with tds_process_result_tokens(). It calls tds_process_row() to copy
 * data into the row buffer.
 * @param tds A pointer to the TDSSOCKET structure managing a 
 *    client/server operation.
 * @param rowtype A pointer to an integer variable which 
 *    tds_process_row_tokens sets to indicate the current type of row
 * @param computeid A pointer to an integer variable which 
 *    tds_process_row_tokens sets to identify the compute_id of the row 
 *    being returned. A compute row is a row that is generated by a 
 *    compute clause. The compute_id matches the number of the compute row 
 *    that was read; the first compute row is 1, the second is 2, and so forth.
 * @par Possible values of *rowtype
 *        @li @c TDS_REG_ROW      A regular data row
 *        @li @c TDS_COMP_ROW     A row of compute data
 *        @li @c TDS_NO_MORE_ROWS There are no more rows of data in this result set
 * @retval TDS_SUCCEED A row of data is available for processing.
 * @retval TDS_NO_MORE_ROWS All rows have been completely processed.
 * @retval TDS_FAIL An unexpected error occurred
 * @par Examples
 * The following code is cut from dbnextrow(), the db-library function
 * @include token2.c
 */
int
tds_process_row_tokens(TDSSOCKET * tds, TDS_INT * rowtype, TDS_INT * computeid)
{
	/*
	 * call internal function, with last parameter 1
	 * meaning read & process the end token
	 */

	return _tds_process_row_tokens(tds, rowtype, computeid, 1);
}
int
tds_process_row_tokens_ct(TDSSOCKET * tds, TDS_INT * rowtype, TDS_INT * computeid)
{
	/*
	 * call internal function, with last parameter 0
	 * meaning DON'T read & process the end token
	 */

	return _tds_process_row_tokens(tds, rowtype, computeid, 0);
}

static int
_tds_process_row_tokens(TDSSOCKET * tds, TDS_INT * rowtype, TDS_INT * computeid, TDS_INT read_end_token)
{
	int marker;
	TDSCURSOR *cursor;

	if (IS_TDSDEAD(tds))
		return TDS_FAIL;
	if (tds->state == TDS_IDLE) {
		*rowtype = TDS_NO_MORE_ROWS;
		tdsdump_log(TDS_DBG_FUNC, "tds_process_row_tokens() state is COMPLETED\n");
		return TDS_NO_MORE_ROWS;
	}

	while (1) {

		marker = tds_get_byte(tds);
		tdsdump_log(TDS_DBG_INFO1, "processing row tokens.  marker is  %x(%s)\n", marker, _tds_token_name(marker));

		switch (marker) {
		case TDS_RESULT_TOKEN:
		case TDS_ROWFMT2_TOKEN:
		case TDS7_RESULT_TOKEN:

			tds_unget_byte(tds);
			*rowtype = TDS_NO_MORE_ROWS;
			return TDS_NO_MORE_ROWS;

		case TDS_ROW_TOKEN:
			if (tds->client_cursor_id) {
				cursor = tds->cursor; 
				while (cursor && cursor->client_cursor_id != tds->client_cursor_id)
					cursor = cursor->next;
			
				if (!cursor) {
					tdsdump_log(TDS_DBG_FUNC, "tds_process_row_tokens() : cannot find cursor_id %d\n", tds->client_cursor_id);
					return TDS_FAIL;
				}
				tds->current_results = cursor->res_info;
				tdsdump_log(TDS_DBG_INFO1, "tds_process_row_tokens(). set current_results to cursor->res_info\n");
			} else {
				if (tds->res_info)
					tds->current_results = tds->res_info;
			}

			if (tds_process_row(tds) == TDS_FAIL)
				return TDS_FAIL;

			*rowtype = TDS_REG_ROW;

			return TDS_SUCCEED;

		case TDS_CMP_ROW_TOKEN:

			*rowtype = TDS_COMP_ROW;
			return tds_process_compute(tds, computeid);

		case TDS_DONE_TOKEN:
		case TDS_DONEPROC_TOKEN:
		case TDS_DONEINPROC_TOKEN:
			if (read_end_token) {
				if (tds_process_end(tds, marker, NULL) == TDS_FAIL)
					return TDS_FAIL;
			} else {
				tds_unget_byte(tds);
			}
			*rowtype = TDS_NO_MORE_ROWS;
			return TDS_NO_MORE_ROWS;

		default:
			if (tds_process_default_tokens(tds, marker) == TDS_FAIL)
				return TDS_FAIL;
			break;
		}
	}
	return TDS_SUCCEED;
}

/**
 * tds_process_trailing_tokens() is called to discard messages that may
 * be left unprocessed at the end of a result "batch". In dblibrary, it is 
 * valid to process all the data rows that a command may have returned but 
 * to leave end tokens etc. unprocessed (at least explicitly)
 * This function is called to discard such tokens. If it comes across a token
 * that does not fall into the category of valid "trailing" tokens, it will 
 * return TDS_FAIL, allowing the calling dblibrary function to return a 
 * "results pending" message. 
 * The valid "trailing" tokens are :
 *
 * TDS_DONE_TOKEN
 * TDS_DONEPROC_TOKEN
 * TDS_DONEINPROC_TOKEN
 * TDS_RETURNSTATUS_TOKEN
 * TDS_PARAM_TOKEN
 * TDS5_PARAMFMT_TOKEN
 * TDS5_PARAMS_TOKEN
 */
int
tds_process_trailing_tokens(TDSSOCKET * tds)
{
	int marker;
	int done_flags;

	tdsdump_log(TDS_DBG_FUNC, "tds_process_trailing_tokens() \n");

	while (tds->state != TDS_IDLE) {

		marker = tds_get_byte(tds);
		tdsdump_log(TDS_DBG_INFO1, "processing trailing tokens.  marker is  %x(%s)\n", marker, _tds_token_name(marker));
		switch (marker) {
		case TDS_DONE_TOKEN:
		case TDS_DONEPROC_TOKEN:
		case TDS_DONEINPROC_TOKEN:
			tds_process_end(tds, marker, &done_flags);
			break;
		case TDS_RETURNSTATUS_TOKEN:
			tds->has_status = 1;
			tds->ret_status = tds_get_int(tds);
			break;
		case TDS_PARAM_TOKEN:
			tds_unget_byte(tds);
			tds_process_param_result_tokens(tds);
			break;
		case TDS5_PARAMFMT_TOKEN:
			tds_process_dyn_result(tds);
			break;
		case TDS5_PARAMFMT2_TOKEN:
			tds5_process_dyn_result2(tds);
			break;
		case TDS5_PARAMS_TOKEN:
			tds_process_params_result_token(tds);
			break;
		default:
			tds_unget_byte(tds);
			return TDS_FAIL;

		}
	}
	return TDS_SUCCEED;
}

/**
 * Process results for simple query as "SET TEXTSIZE" or "USE dbname"
 * If the statement returns results, beware they are discarded.
 *
 * This function was written to avoid direct calls to tds_process_default_tokens
 * (which caused problems such as ignoring query errors).
 * Results are read until idle state or severe failure (do not stop for 
 * statement failure).
 * @return see tds_process_result_tokens for results (TDS_NO_MORE_RESULTS is never returned)
 */
int
tds_process_simple_query(TDSSOCKET * tds)
{
	TDS_INT res_type;
	TDS_INT done_flags;
	TDS_INT row_type;
	int     rc;

	while ((rc = tds_process_result_tokens(tds, &res_type, &done_flags)) == TDS_SUCCEED) {
		switch (res_type) {

			case TDS_ROW_RESULT:
			case TDS_COMPUTE_RESULT:

				/* discard all this information */
				while ((rc = tds_process_row_tokens_ct(tds, &row_type, NULL)) == TDS_SUCCEED);

				if (rc != TDS_NO_MORE_ROWS)
					return TDS_FAIL;

				break;

			case TDS_DONE_RESULT:
			case TDS_DONEPROC_RESULT:
			case TDS_DONEINPROC_RESULT:
				if ((done_flags & TDS_DONE_ERROR) != 0) 
					return TDS_FAIL;
				break;

			default:
				break;
		}
	}
	if (rc != TDS_NO_MORE_RESULTS) {
		return TDS_FAIL;
	}

	return TDS_SUCCEED;
}

/** 
 * simple flush function.  maybe be superseded soon.
 */
int
tds_do_until_done(TDSSOCKET * tds)
{
	int marker, rows_affected = 0;

	do {
		marker = tds_get_byte(tds);
		if (marker == TDS_DONE_TOKEN) {
			tds_process_end(tds, marker, NULL);
			rows_affected = tds->rows_affected;
		} else {
			if (tds_process_default_tokens(tds, marker) == TDS_FAIL)
				return TDS_FAIL;
		}
	} while (marker != TDS_DONE_TOKEN);

	return rows_affected;
}

/**
 * tds_process_col_name() is one half of the result set under TDS 4.2
 * it contains all the column names, a TDS_COLFMT_TOKEN should 
 * immediately follow this token with the datatype/size information
 * This is a 4.2 only function
 */
static int
tds_process_col_name(TDSSOCKET * tds)
{
	int hdrsize, len = 0;
	int memrc = 0;
	int col, num_cols = 0;
	struct tmp_col_struct
	{
		char *column_name;
		int column_namelen;
		struct tmp_col_struct *next;
	};
	struct tmp_col_struct *head = NULL, *cur = NULL, *prev;
	TDSCOLUMN *curcol;
	TDSRESULTINFO *info;

	hdrsize = tds_get_smallint(tds);

	/*
	 * this is a little messy...TDS 5.0 gives the number of columns
	 * upfront, while in TDS 4.2, you're expected to figure it out
	 * by the size of the message. So, I use a link list to get the
	 * colum names and then allocate the result structure, copy
	 * and delete the linked list
	 */
	/*
	 * TODO reallocate columns
	 * TODO code similar below, function to reuse
	 */
	while (len < hdrsize) {
		prev = cur;
		cur = (struct tmp_col_struct *)
			malloc(sizeof(struct tmp_col_struct));

		if (!cur) {
			memrc = -1;
			break;
		}

		if (prev)
			prev->next = cur;
		if (!head)
			head = cur;

		cur->column_namelen = tds_get_byte(tds);
		memrc += tds_alloc_get_string(tds, &cur->column_name, cur->column_namelen);
		cur->next = NULL;

		len += cur->column_namelen + 1;
		num_cols++;
	}

	/* free results/computes/params etc... */
	tds_free_all_results(tds);
	tds->rows_affected = TDS_NO_COUNT;

	if ((info = tds_alloc_results(num_cols)) == NULL)
		memrc = -1;
	tds->current_results = tds->res_info = info;

	/* tell the upper layers we are processing results */
	tds->state = TDS_PENDING;
	cur = head;

	if (memrc != 0) {
		while (cur != NULL) {
			prev = cur;
			cur = cur->next;
			free(prev->column_name);
			free(prev);
		}
		return TDS_FAIL;
	} else {
		for (col = 0; col < info->num_cols; col++) {
			curcol = info->columns[col];
			strncpy(curcol->column_name, cur->column_name, sizeof(curcol->column_name));
			curcol->column_name[sizeof(curcol->column_name) - 1] = 0;
			curcol->column_namelen = strlen(curcol->column_name);
			prev = cur;
			cur = cur->next;
			free(prev->column_name);
			free(prev);
		}
		return TDS_SUCCEED;
	}
}

/**
 * Add a column size to result info row size and calc offset into row
 * @param info   result where to add column
 * @param curcol column to add
 */
void
tds_add_row_column_size(TDSRESULTINFO * info, TDSCOLUMN * curcol)
{
	/*
	 * the column_offset is the offset into the row buffer
	 * where this column begins, text types are no longer
	 * stored in the row buffer because the max size can
	 * be too large (2gig) to allocate 
	 */
	curcol->column_offset = info->row_size;
	if (is_numeric_type(curcol->column_type)) {
		info->row_size += sizeof(TDS_NUMERIC);
	} else if (is_blob_type(curcol->column_type)) {
		info->row_size += sizeof(TDSBLOB);
	} else {
		info->row_size += curcol->column_size;
	}
	info->row_size += (TDS_ALIGN_SIZE - 1);
	info->row_size -= info->row_size % TDS_ALIGN_SIZE;
}

/**
 * tds_process_col_fmt() is the other half of result set processing
 * under TDS 4.2. It follows tds_process_col_name(). It contains all the 
 * column type and size information.
 * This is a 4.2 only function
 */
static int
tds_process_col_fmt(TDSSOCKET * tds)
{
	int col, hdrsize;
	TDSCOLUMN *curcol;
	TDSRESULTINFO *info;
	TDS_SMALLINT tabnamesize;
	int bytes_read = 0;
	int rest;
	TDS_SMALLINT flags;

	hdrsize = tds_get_smallint(tds);

	/* TODO use current_results instead of res_info ?? */
	info = tds->res_info;
	for (col = 0; col < info->num_cols; col++) {
		curcol = info->columns[col];
		/* In Sybase all 4 byte are used for usertype, while mssql place 2 byte as usertype and 2 byte as flags */
		if (TDS_IS_MSSQL(tds)) {
			curcol->column_usertype = tds_get_smallint(tds);
			flags = tds_get_smallint(tds);
			curcol->column_nullable = flags & 0x01;
			curcol->column_writeable = (flags & 0x08) > 0;
			curcol->column_identity = (flags & 0x10) > 0;
		} else {
			curcol->column_usertype = tds_get_int(tds);
		}
		/* on with our regularly scheduled code (mlilback, 11/7/01) */
		tds_set_column_type(curcol, tds_get_byte(tds));

		tdsdump_log(TDS_DBG_INFO1, "processing result. type = %d(%s), varint_size %d\n",
			    curcol->column_type, tds_prtype(curcol->column_type), curcol->column_varint_size);

		switch (curcol->column_varint_size) {
		case 4:
			curcol->column_size = tds_get_int(tds);
			/* junk the table name -- for now */
			tabnamesize = tds_get_smallint(tds);
			tds_get_n(tds, NULL, tabnamesize);
			bytes_read += 5 + 4 + 2 + tabnamesize;
			break;
		case 1:
			curcol->column_size = tds_get_byte(tds);
			bytes_read += 5 + 1;
			break;
		case 0:
			bytes_read += 5 + 0;
			break;
		}

		/* Adjust column size according to client's encoding */
		curcol->on_server.column_size = curcol->column_size;
		adjust_character_column_size(tds, curcol);

		tds_add_row_column_size(info, curcol);
	}

	/* get the rest of the bytes */
	rest = hdrsize - bytes_read;
	if (rest > 0) {
		tdsdump_log(TDS_DBG_INFO1, "NOTE:tds_process_col_fmt: draining %d bytes\n", rest);
		tds_get_n(tds, NULL, rest);
	}

	if ((info->current_row = tds_alloc_row(info)) != NULL)
		return TDS_SUCCEED;
	else
		return TDS_FAIL;
}

static int
tds_process_colinfo(TDSSOCKET * tds)
{
	int hdrsize;
	TDSCOLUMN *curcol;
	TDSRESULTINFO *info;
	int bytes_read = 0;
	unsigned char col_info[3], l;

	hdrsize = tds_get_smallint(tds);

	info = tds->current_results;

	while (bytes_read < hdrsize) {

		tds_get_n(tds, col_info, 3);
		bytes_read += 3;
		if (info && col_info[0] > 0 && col_info[0] <= info->num_cols) {
			curcol = info->columns[col_info[0] - 1];
			curcol->column_writeable = (col_info[2] & 0x4) == 0;
			curcol->column_key = (col_info[2] & 0x8) > 0;
			curcol->column_hidden = (col_info[2] & 0x10) > 0;
		}
		/* skip real name */
		/* TODO keep it */
		if (col_info[2] & 0x20) {
			l = tds_get_byte(tds);
			if (IS_TDS7_PLUS(tds))
				l *= 2;
			tds_get_n(tds, NULL, l);
			bytes_read += l + 1;
		}
	}

	return TDS_SUCCEED;
}

/**
 * tds_process_param_result() processes output parameters of a stored 
 * procedure. This differs from regular row/compute results in that there
 * is no total number of parameters given, they just show up singly.
 */
static int
tds_process_param_result(TDSSOCKET * tds, TDSPARAMINFO ** pinfo)
{
	int hdrsize;
	TDSCOLUMN *curparam;
	TDSPARAMINFO *info;
	int i;

	/* TODO check if current_results is a param result */

	/* limited to 64K but possible types are always smaller (not TEXT/IMAGE) */
	hdrsize = tds_get_smallint(tds);
	if ((info = tds_alloc_param_result(*pinfo)) == NULL)
		return TDS_FAIL;

	*pinfo = info;
	curparam = info->columns[info->num_cols - 1];

	/* FIXME check support for tds7+ (seem to use same format of tds5 for data...)
	 * perhaps varint_size can be 2 or collation can be specified ?? */
	tds_get_data_info(tds, curparam, 1);

	curparam->column_cur_size = curparam->column_size;	/* needed ?? */

	if (tds_alloc_param_row(info, curparam) == NULL)
		return TDS_FAIL;

	i = tds_get_data(tds, curparam, info->current_row, info->num_cols - 1);

	return i;
}

static int
tds_process_param_result_tokens(TDSSOCKET * tds)
{
	int marker;
	TDSPARAMINFO **pinfo;

	if (tds->cur_dyn)
		pinfo = &(tds->cur_dyn->res_info);
	else
		pinfo = &(tds->param_info);

	while ((marker = tds_get_byte(tds)) == TDS_PARAM_TOKEN) {
		tds_process_param_result(tds, pinfo);
	}
	tds->current_results = *pinfo;
	tds_unget_byte(tds);
	return TDS_SUCCEED;
}

/**
 * tds_process_params_result_token() processes params on TDS5.
 */
static int
tds_process_params_result_token(TDSSOCKET * tds)
{
	int i;
	TDSCOLUMN *curcol;
	TDSPARAMINFO *info;

	/* TODO check if current_results is a param result */
	info = tds->current_results;
	if (!info)
		return TDS_FAIL;

	for (i = 0; i < info->num_cols; i++) {
		curcol = info->columns[i];
		if (tds_get_data(tds, curcol, info->current_row, i) != TDS_SUCCEED)
			return TDS_FAIL;
	}
	return TDS_SUCCEED;
}

/**
 * tds_process_compute_result() processes compute result sets.  These functions
 * need work but since they get little use, nobody has complained!
 * It is very similar to normal result sets.
 */
static int
tds_process_compute_result(TDSSOCKET * tds)
{
	int hdrsize;
	int col, num_cols;
	TDS_TINYINT by_cols = 0;
	TDS_TINYINT *cur_by_col;
	TDS_SMALLINT compute_id = 0;
	TDSCOLUMN *curcol;
	TDSCOMPUTEINFO *info;
	int i;


	hdrsize = tds_get_smallint(tds);

	/*
	 * compute statement id which this relates
	 * to. You can have more than one compute
	 * statement in a SQL statement
	 */

	compute_id = tds_get_smallint(tds);

	tdsdump_log(TDS_DBG_INFO1, "processing tds7 compute result. compute_id = %d\n", compute_id);

	/*
	 * number of compute columns returned - so
	 * COMPUTE SUM(x), AVG(x)... would return
	 * num_cols = 2
	 */

	num_cols = tds_get_byte(tds);

	for (i = 0;; ++i) {
		if (i >= tds->num_comp_info)
			return TDS_FAIL;
		info = tds->comp_info[i];
		tdsdump_log(TDS_DBG_FUNC, "in dbaltcolid() found computeid = %d\n", info->computeid);
		if (info->computeid == compute_id)
			break;
	}

	tdsdump_log(TDS_DBG_INFO1, "processing tds7 compute result. num_cols = %d\n", num_cols);

	for (col = 0; col < num_cols; col++) {
		tdsdump_log(TDS_DBG_INFO1, "processing tds7 compute result. point 2\n");
		curcol = info->columns[col];

		curcol->column_operator = tds_get_byte(tds);
		curcol->column_operand = tds_get_byte(tds);

		/*
		 * if no name has been defined for the compute column,
		 * put in "max", "avg" etc.
		 */

		if (curcol->column_namelen == 0) {
			strcpy(curcol->column_name, tds_pr_op(curcol->column_operator));
			curcol->column_namelen = strlen(curcol->column_name);
		}

		/*  User defined data type of the column */
		curcol->column_usertype = tds_get_int(tds);

		tds_set_column_type(curcol, tds_get_byte(tds));

		switch (curcol->column_varint_size) {
		case 4:
			curcol->column_size = tds_get_int(tds);
			break;
		case 2:
			curcol->column_size = tds_get_smallint(tds);
			break;
		case 1:
			curcol->column_size = tds_get_byte(tds);
			break;
		case 0:
			break;
		}
		tdsdump_log(TDS_DBG_INFO1, "processing result. column_size %d\n", curcol->column_size);

		/* Adjust column size according to client's encoding */
		curcol->on_server.column_size = curcol->column_size;
		/* TODO check if this column can have collation information associated */
		adjust_character_column_size(tds, curcol);

		/* skip locale */
		if (!IS_TDS42(tds))
			tds_get_n(tds, NULL, tds_get_byte(tds));

		tds_add_row_column_size(info, curcol);
	}

	by_cols = tds_get_byte(tds);

	tdsdump_log(TDS_DBG_INFO1, "processing tds compute result. by_cols = %d\n", by_cols);

	if (by_cols) {
		if ((info->bycolumns = (TDS_TINYINT *) malloc(by_cols)) == NULL)
			return TDS_FAIL;

		memset(info->bycolumns, '\0', by_cols);
	}
	info->by_cols = by_cols;

	cur_by_col = info->bycolumns;
	for (col = 0; col < by_cols; col++) {
		*cur_by_col = tds_get_byte(tds);
		cur_by_col++;
	}

	if ((info->current_row = tds_alloc_compute_row(info)) != NULL)
		return TDS_SUCCEED;
	else
		return TDS_FAIL;
}

/**
 * Read data information from wire
 * @param curcol column where to store information
 */
static int
tds7_get_data_info(TDSSOCKET * tds, TDSCOLUMN * curcol)
{
	int colnamelen;

	/*  User defined data type of the column */
	curcol->column_usertype = tds_get_smallint(tds);

	curcol->column_flags = tds_get_smallint(tds);	/*  Flags */

	curcol->column_nullable = curcol->column_flags & 0x01;
	curcol->column_writeable = (curcol->column_flags & 0x08) > 0;
	curcol->column_identity = (curcol->column_flags & 0x10) > 0;

	tds_set_column_type(curcol, tds_get_byte(tds));	/* sets "cardinal" type */

	curcol->column_timestamp = (curcol->column_type == SYBBINARY && curcol->column_usertype == TDS_UT_TIMESTAMP);

	switch (curcol->column_varint_size) {
	case 4:
		curcol->column_size = tds_get_int(tds);
		break;
	case 2:
		curcol->column_size = tds_get_smallint(tds);
		break;
	case 1:
		curcol->column_size = tds_get_byte(tds);
		break;
	case 0:
		break;
	}

	/* Adjust column size according to client's encoding */
	curcol->on_server.column_size = curcol->column_size;

	/* numeric and decimal have extra info */
	if (is_numeric_type(curcol->column_type)) {
		curcol->column_prec = tds_get_byte(tds);	/* precision */
		curcol->column_scale = tds_get_byte(tds);	/* scale */
	}

	if (IS_TDS80(tds) && is_collate_type(curcol->on_server.column_type)) {
		/* based on true type as sent by server */
		/*
		 * first 2 bytes are windows code (such as 0x409 for english)
		 * other 2 bytes ???
		 * last bytes is id in syscharsets
		 */
		tds_get_n(tds, curcol->column_collation, 5);
		curcol->char_conv =
			tds_iconv_from_collate(tds, curcol->column_collation[4],
					       curcol->column_collation[1] * 256 + curcol->column_collation[0]);
	}

	/* NOTE adjustements must be done after curcol->char_conv initialization */
	adjust_character_column_size(tds, curcol);

	if (is_blob_type(curcol->column_type)) {
		curcol->table_namelen =
			tds_get_string(tds, tds_get_smallint(tds), curcol->table_name, sizeof(curcol->table_name) - 1);
	}

	/*
	 * under 7.0 lengths are number of characters not
	 * number of bytes...tds_get_string handles this
	 */
	/*
	 * column_name can store up to 512 bytes however column_namelen can hold
	 * that size so limit column name, fixed in 0.64 -- freddy77
	 */
	colnamelen = tds_get_string(tds, tds_get_byte(tds), curcol->column_name, 255);
	curcol->column_name[colnamelen] = 0;
	curcol->column_namelen = colnamelen;

	tdsdump_log(TDS_DBG_INFO1, "tds7_get_data_info:%d: \n"
		    "\tcolname = %s (%d bytes)\n"
		    "\ttype = %d (%s)\n"
		    "\tserver's type = %d (%s)\n"
		    "\tcolumn_varint_size = %d\n"
		    "\tcolumn_size = %d (%d on server)\n",
		    __LINE__, 
		    curcol->column_name, curcol->column_namelen, 
		    curcol->column_type, tds_prtype(curcol->column_type), 
		    curcol->on_server.column_type, tds_prtype(curcol->on_server.column_type), 
		    curcol->column_varint_size,
		    curcol->column_size, curcol->on_server.column_size);

	return TDS_SUCCEED;
}

/**
 * tds7_process_result() is the TDS 7.0 result set processing routine.  It 
 * is responsible for populating the tds->res_info structure.
 * This is a TDS 7.0 only function
 */
static int
tds7_process_result(TDSSOCKET * tds)
{
	int col, num_cols;
	TDSCOLUMN *curcol;
	TDSRESULTINFO *info;
	TDSCURSOR *cursor;

	/* read number of columns and allocate the columns structure */

	num_cols = tds_get_smallint(tds);

	/* This can be a DUMMY results token from a cursor fetch */

	if (num_cols == -1) {
		tdsdump_log(TDS_DBG_INFO1, "processing TDS7 result. no meta data\n");
		return TDS_SUCCEED;
	}

	tds_free_all_results(tds);
	tds->rows_affected = TDS_NO_COUNT;

	if (tds->client_cursor_id) {
		cursor = tds->cursor; 
		while (cursor && cursor->client_cursor_id != tds->client_cursor_id)
			cursor = cursor->next;
	
		if (!cursor) {
			tdsdump_log(TDS_DBG_FUNC, "tds7_process_result() : cannot find cursor_id %d\n", tds->client_cursor_id);
			return TDS_FAIL;
		}
		if ((cursor->res_info = tds_alloc_results(num_cols)) == NULL)
			return TDS_FAIL;
		info = cursor->res_info;
		tds->current_results = cursor->res_info;
		tdsdump_log(TDS_DBG_INFO1, "processing TDS7 result. set current_results to cursor->res_info\n");
	} else {
		if ((tds->res_info = tds_alloc_results(num_cols)) == NULL)
			return TDS_FAIL;
		info = tds->res_info;
		tds->current_results = tds->res_info;
		tdsdump_log(TDS_DBG_INFO1, "processing TDS7 result. set current_results to tds->res_info\n");
	}

	/* tell the upper layers we are processing results */
	tds->state = TDS_PENDING;

	/* loop through the columns populating COLINFO struct from
	 * server response */
	for (col = 0; col < num_cols; col++) {

		curcol = info->columns[col];

		tds7_get_data_info(tds, curcol);

		tds_add_row_column_size(info, curcol);
	}

	/* all done now allocate a row for tds_process_row to use */
	if ((info->current_row = tds_alloc_row(info)) != NULL)
		return TDS_SUCCEED;
	else
		return TDS_FAIL;
}

/**
 * Read data information from wire
 * @param curcol column where to store information
 */
static int
tds_get_data_info(TDSSOCKET * tds, TDSCOLUMN * curcol, int is_param)
{
	/*
	 * column_name can store up to 512 bytes however column_namelen can hold
	 * that size so limit column name, fixed in 0.64 -- freddy77
	 */
	curcol->column_namelen = tds_get_string(tds, tds_get_byte(tds), curcol->column_name, 255);
	curcol->column_name[curcol->column_namelen] = '\0';

	curcol->column_flags = tds_get_byte(tds);	/*  Flags */
	if (!is_param) {
		/* TODO check if all flags are the same for all TDS versions */
		if (IS_TDS50(tds))
			curcol->column_hidden = curcol->column_flags & 0x1;
		curcol->column_key = (curcol->column_flags & 0x2) > 1;
		curcol->column_writeable = (curcol->column_flags & 0x10) > 1;
		curcol->column_nullable = (curcol->column_flags & 0x20) > 1;
		curcol->column_identity = (curcol->column_flags & 0x40) > 1;
	}

	curcol->column_usertype = tds_get_int(tds);
	tds_set_column_type(curcol, tds_get_byte(tds));

	tdsdump_log(TDS_DBG_INFO1, "processing result. type = %d(%s), varint_size %d\n",
		    curcol->column_type, tds_prtype(curcol->column_type), curcol->column_varint_size);
	switch (curcol->column_varint_size) {
	case 4:
		curcol->column_size = tds_get_int(tds);
		/* Only read table_name for blob columns (eg. not for SYBLONGBINARY) */
		if (is_blob_type (curcol->column_type)) {
			curcol->table_namelen =
				tds_get_string(tds, tds_get_smallint(tds), curcol->table_name, sizeof(curcol->table_name) - 1);
		}
		break;
	case 2:
		curcol->column_size = tds_get_smallint(tds);
		break;
	case 1:
		curcol->column_size = tds_get_byte(tds);
		break;
	case 0:
		break;
	}
	tdsdump_log(TDS_DBG_INFO1, "processing result. column_size %d\n", curcol->column_size);

	/* numeric and decimal have extra info */
	if (is_numeric_type(curcol->column_type)) {
		curcol->column_prec = tds_get_byte(tds);	/* precision */
		curcol->column_scale = tds_get_byte(tds);	/* scale */
	}

	/* read sql collation info */
	/* TODO: we should use it ! */
	if (IS_TDS80(tds) && is_collate_type(curcol->on_server.column_type)) {
		tds_get_n(tds, curcol->column_collation, 5);
		curcol->char_conv =
			tds_iconv_from_collate(tds, curcol->column_collation[4],
					       curcol->column_collation[1] * 256 + curcol->column_collation[0]);
	}

	/* Adjust column size according to client's encoding */
	curcol->on_server.column_size = curcol->column_size;
	adjust_character_column_size(tds, curcol);

	return TDS_SUCCEED;
}

/**
 * tds_process_result() is the TDS 5.0 result set processing routine.  It 
 * is responsible for populating the tds->res_info structure.
 * This is a TDS 5.0 only function
 */
static int
tds_process_result(TDSSOCKET * tds)
{
	int hdrsize;
	int col, num_cols;
	TDSCOLUMN *curcol;
	TDSRESULTINFO *info;
	TDSCURSOR *cursor;

	tds_free_all_results(tds);
	tds->rows_affected = TDS_NO_COUNT;

	hdrsize = tds_get_smallint(tds);

	/* read number of columns and allocate the columns structure */
	num_cols = tds_get_smallint(tds);

	if (tds->client_cursor_id) {
		cursor = tds->cursor; 
		while (cursor && cursor->client_cursor_id != tds->client_cursor_id)
			cursor = cursor->next;
	
		if (!cursor) {
			tdsdump_log(TDS_DBG_FUNC, "tds7_process_result() : cannot find cursor_id %d\n", tds->client_cursor_id);
			return TDS_FAIL;
		}
		if ((cursor->res_info = tds_alloc_results(num_cols)) == NULL)
			return TDS_FAIL;
		info = cursor->res_info;
		tds->current_results = cursor->res_info;
	} else {
		if ((tds->res_info = tds_alloc_results(num_cols)) == NULL)
			return TDS_FAIL;
	
		info = tds->res_info;
		tds->current_results = tds->res_info;
	}

	/* tell the upper layers we are processing results */
	tds->state = TDS_PENDING;

	/*
	 * loop through the columns populating COLINFO struct from
	 * server response
	 */
	for (col = 0; col < info->num_cols; col++) {
		curcol = info->columns[col];

		tds_get_data_info(tds, curcol, 0);

		/* skip locale information */
		/* NOTE do not put into tds_get_data_info, param do not have locale information */
		tds_get_n(tds, NULL, tds_get_byte(tds));

		tds_add_row_column_size(info, curcol);
	}
	if ((info->current_row = tds_alloc_row(info)) != NULL)
		return TDS_SUCCEED;
	else
		return TDS_FAIL;
}

/**
 * tds5_process_result() is the new TDS 5.0 result set processing routine.  
 * It is responsible for populating the tds->res_info structure.
 * This is a TDS 5.0 only function
 */
static int
tds5_process_result(TDSSOCKET * tds)
{
	int hdrsize;

	/* int colnamelen; */
	int col, num_cols;
	TDSCOLUMN *curcol;
	TDSRESULTINFO *info;
	TDSCURSOR *cursor;

	tdsdump_log(TDS_DBG_INFO1, "tds5_process_result\n");

	/*
	 * free previous resultset
	 */
	tds_free_all_results(tds);
	tds->rows_affected = TDS_NO_COUNT;

	/*
	 * read length of packet (4 bytes)
	 */
	hdrsize = tds_get_int(tds);

	/* read number of columns and allocate the columns structure */
	num_cols = tds_get_smallint(tds);

	if (tds->client_cursor_id) {
		cursor = tds->cursor; 
		while (cursor && cursor->client_cursor_id != tds->client_cursor_id)
			cursor = cursor->next;
	
		if (!cursor) {
			tdsdump_log(TDS_DBG_FUNC, "tds7_process_result() : cannot find cursor_id %d\n", tds->client_cursor_id);
			return TDS_FAIL;
		}
		if ((cursor->res_info = tds_alloc_results(num_cols)) == NULL)
			return TDS_FAIL;
		info = cursor->res_info;
	} else {
		if ((tds->res_info = tds_alloc_results(num_cols)) == NULL)
			return TDS_FAIL;
		info = tds->res_info;
	}
	tds->current_results = info;

	tdsdump_log(TDS_DBG_INFO1, "num_cols=%d\n", num_cols);

	/* tell the upper layers we are processing results */
	tds->state = TDS_PENDING;

	/* TODO reuse some code... */
	/* loop through the columns populating COLINFO struct from
	 * server response */
	for (col = 0; col < info->num_cols; col++) {
		curcol = info->columns[col];

		/* label */
		/*
		 * column_name can store up to 512 bytes however column_namelen can hold
		 * that size so limit column name, fixed in 0.64 -- freddy77
		 */
		curcol->column_namelen =
			tds_get_string(tds, tds_get_byte(tds), curcol->column_name, 255);
		curcol->column_name[curcol->column_namelen] = '\0';

		/* TODO add these field again */
		/* database */
		/*
		 * colnamelen = tds_get_byte(tds);
		 * tds_get_n(tds, curcol->catalog_name, colnamelen);
		 * curcol->catalog_name[colnamelen] = '\0';
		 */

		/* owner */
		/*
		 * colnamelen = tds_get_byte(tds);
		 * tds_get_n(tds, curcol->schema_name, colnamelen);
		 * curcol->schema_name[colnamelen] = '\0';
		 */

		/* table */
		/*
		 * colnamelen = tds_get_byte(tds);
		 * tds_get_n(tds, curcol->table_name, colnamelen);
		 * curcol->table_name[colnamelen] = '\0';
		 */

		/* column name */
		/*
		 * colnamelen = tds_get_byte(tds);
		 * tds_get_n(tds, curcol->column_colname, colnamelen);
		 * curcol->column_colname[colnamelen] = '\0';
		 */

		/* if label is empty, use the column name */
		/*
		 * if (colnamelen > 0 && curcol->column_name[0] == '\0')
		 * strcpy(curcol->column_name, curcol->column_colname);
		 */

		/* flags (4 bytes) */
		curcol->column_flags = tds_get_int(tds);
		curcol->column_hidden = curcol->column_flags & 0x1;
		curcol->column_key = (curcol->column_flags & 0x2) > 1;
		curcol->column_writeable = (curcol->column_flags & 0x10) > 1;
		curcol->column_nullable = (curcol->column_flags & 0x20) > 1;
		curcol->column_identity = (curcol->column_flags & 0x40) > 1;

		curcol->column_usertype = tds_get_int(tds);

		curcol->column_type = tds_get_byte(tds);

		curcol->column_varint_size = tds5_get_varint_size(curcol->column_type);

		switch (curcol->column_varint_size) {
		case 4:
			if (curcol->column_type == SYBTEXT || curcol->column_type == SYBIMAGE) {
				int namelen;

				curcol->column_size = tds_get_int(tds);

				/* skip name */
				namelen = tds_get_smallint(tds);
				if (namelen)
					tds_get_n(tds, NULL, namelen);

			} else
				tdsdump_log(TDS_DBG_INFO1, "UNHANDLED TYPE %x\n", curcol->column_type);
			break;
		case 5:
			curcol->column_size = tds_get_int(tds);
			break;
		case 2:
			curcol->column_size = tds_get_smallint(tds);
			break;
		case 1:
			curcol->column_size = tds_get_byte(tds);
			break;
		case 0:
			curcol->column_size = tds_get_size_by_type(curcol->column_type);
			break;
		}

		/* Adjust column size according to client's encoding */
		curcol->on_server.column_size = curcol->column_size;
		adjust_character_column_size(tds, curcol);

		/* numeric and decimal have extra info */
		if (is_numeric_type(curcol->column_type)) {
			curcol->column_prec = tds_get_byte(tds);	/* precision */
			curcol->column_scale = tds_get_byte(tds);	/* scale */
		}

		/* discard Locale */
		tds_get_n(tds, NULL, tds_get_byte(tds));

		tds_add_row_column_size(info, curcol);

		/* 
		 *  Dump all information on this column
		 */
		tdsdump_log(TDS_DBG_INFO1, "col %d:\n", col);
		tdsdump_log(TDS_DBG_INFO1, "tcolumn_label=[%s]\n", curcol->column_name);
/*		tdsdump_log(TDS_DBG_INFO1, "\tcolumn_name=[%s]\n", curcol->column_colname);
		tdsdump_log(TDS_DBG_INFO1, "\tcatalog=[%s] schema=[%s] table=[%s]\n",
			    curcol->catalog_name, curcol->schema_name, curcol->table_name, curcol->column_colname);
*/
		tdsdump_log(TDS_DBG_INFO1, "\tflags=%x utype=%d type=%d varint=%d\n",
			    curcol->column_flags, curcol->column_usertype, curcol->column_type, curcol->column_varint_size);

		tdsdump_log(TDS_DBG_INFO1, "\tcolsize=%d prec=%d scale=%d\n",
			    curcol->column_size, curcol->column_prec, curcol->column_scale);
	}
	if ((info->current_row = tds_alloc_row(info)) != NULL)
		return TDS_SUCCEED;
	else
		return TDS_FAIL;
}

/**
 * tds_process_compute() processes compute rows and places them in the row
 * buffer.  
 */
static int
tds_process_compute(TDSSOCKET * tds, TDS_INT * computeid)
{
	int i;
	TDSCOLUMN *curcol;
	TDSCOMPUTEINFO *info;
	TDS_INT compute_id;

	compute_id = tds_get_smallint(tds);

	for (i = 0;; ++i) {
		if (i >= tds->num_comp_info)
			return TDS_FAIL;
		info = tds->comp_info[i];
		if (info->computeid == compute_id)
			break;
	}
	tds->current_results = info;

	for (i = 0; i < info->num_cols; i++) {
		curcol = info->columns[i];
		if (tds_get_data(tds, curcol, info->current_row, i) != TDS_SUCCEED)
			return TDS_FAIL;
	}
	if (computeid)
		*computeid = compute_id;
	return TDS_SUCCEED;
}


/**
 * Read a data from wire
 * @param curcol column where store column information
 * @param current_row pointer to row data to store information
 * @param i column position in current_row
 * @return TDS_FAIL on error or TDS_SUCCEED
 */
static int
tds_get_data(TDSSOCKET * tds, TDSCOLUMN * curcol, unsigned char *current_row, int i)
{
	unsigned char *dest;
	int len, colsize;
	int fillchar;
	TDSBLOB *blob = NULL;

	tdsdump_log(TDS_DBG_INFO1, "processing row.  column is %d varint size = %d\n", i, curcol->column_varint_size);
	switch (curcol->column_varint_size) {
	case 4:
		/*
		 * TODO finish 
		 * This strange type has following structure 
		 * 0 len (int32) -- NULL 
		 * len (int32), type (int8), data -- ints, date, etc
		 * len (int32), type (int8), 7 (int8), collation, column size (int16) -- [n]char, [n]varchar, binary, varbinary 
		 * BLOBS (text/image) not supported
		 */
		if (curcol->column_type == SYBVARIANT) {
			colsize = tds_get_int(tds);
			tds_get_n(tds, NULL, colsize);
			tds_set_null(current_row, i);
			return TDS_SUCCEED;
		}
		
		/*
		 * LONGBINARY
		 * This type just stores a 4-byte length
		 */
		if (curcol->column_type == SYBLONGBINARY) {
			colsize = tds_get_int(tds);
			break;
		}
		
		/* It's a BLOB... */
		len = tds_get_byte(tds);
		blob = (TDSBLOB *) & (current_row[curcol->column_offset]);
		if (len == 16) {	/*  Jeff's hack */
			tds_get_n(tds, blob->textptr, 16);
			tds_get_n(tds, blob->timestamp, 8);
			colsize = tds_get_int(tds);
		} else {
			colsize = 0;
		}
		break;
	case 2:
		colsize = tds_get_smallint(tds);
		/* handle empty no-NULL string */
		if (colsize == 0) {
			tds_clr_null(current_row, i);
			curcol->column_cur_size = 0;
			return TDS_SUCCEED;
		}
		if (colsize == -1)
			colsize = 0;
		break;
	case 1:
		colsize = tds_get_byte(tds);
		break;
	case 0:
		/* TODO this should be column_size */
		colsize = tds_get_size_by_type(curcol->column_type);
		break;
	default:
		colsize = 0;
		break;
	}
	if (IS_TDSDEAD(tds))
		return TDS_FAIL;

	tdsdump_log(TDS_DBG_INFO1, "processing row.  column size is %d \n", colsize);
	/* set NULL flag in the row buffer */
	if (colsize == 0) {
		tds_set_null(current_row, i);
		return TDS_SUCCEED;
	}

	tds_clr_null(current_row, i);
	
	/* 
	 * We're now set to read the data from the wire.  For varying types (e.g. char/varchar)
	 * make sure that curcol->column_cur_size reflects the size of the read data, 
	 * after any charset conversion.  tds_get_char_data() does that for you, 
	 * but of course tds_get_n() doesn't.  
	 *
	 * colsize == wire_size, bytes to read
	 * curcol->column_cur_size == sizeof destination buffer, room to write
	 */
	dest = &(current_row[curcol->column_offset]);
	if (is_numeric_type(curcol->column_type)) {
		/* 
		 * Handling NUMERIC datatypes: 
		 * Since these can be passed around independent
		 * of the original column they came from, we embed the TDS_NUMERIC datatype in the row buffer
		 * instead of using the wire representation, even though it uses a few more bytes.  
		 */
		TDS_NUMERIC *num = (TDS_NUMERIC *) dest;
		memset(num, '\0', sizeof(TDS_NUMERIC));
		num->precision = curcol->column_prec;
		num->scale = curcol->column_scale;

		/* server is going to crash freetds ?? */
		if (colsize > sizeof(num->array))
			return TDS_FAIL;
		tds_get_n(tds, num->array, colsize);

		/* corrected colsize for column_cur_size */
		colsize = sizeof(TDS_NUMERIC);
		if (IS_TDS7_PLUS(tds)) {
			tdsdump_log(TDS_DBG_INFO1, "swapping numeric data...\n");
			tds_swap_datatype(tds_get_conversion_type(curcol->column_type, colsize), (unsigned char *) num);
		}
		curcol->column_cur_size = colsize;
	} else if (is_blob_type(curcol->column_type)) {
		TDS_CHAR *p;
		int new_blob_size;
		assert(blob == (TDSBLOB *) dest); 	/* cf. column_varint_size case 4, above */
		
		/* 
		 * Blobs don't use a column's fixed buffer because the official maximum size is 2 GB.
		 * Instead, they're reallocated as necessary, based on the data's size.  
		 * Here we allocate memory, if need be.  
		 */
		/* TODO this can lead to a big waste of memory */
		new_blob_size = determine_adjusted_size(curcol->char_conv, colsize);
		
		/* NOTE we use an extra pointer (p) to avoid lose of memory in the case realloc fails */
		p = blob->textvalue;
		if (!p) {
			p = (TDS_CHAR *) malloc(new_blob_size);
		} else {
			/* TODO perhaps we should store allocated bytes too ? */
			if (new_blob_size > curcol->column_cur_size ||  (curcol->column_cur_size - new_blob_size) > 10240) {
				p = (TDS_CHAR *) realloc(p, new_blob_size);
			}
		}
		
		if (!p)
			return TDS_FAIL;
		blob->textvalue = p;
		curcol->column_cur_size = new_blob_size;
		
		/* read the data */
		if (is_char_type(curcol->column_type)) {
			if (tds_get_char_data(tds, (char *) blob, colsize, curcol) == TDS_FAIL)
				return TDS_FAIL;
		} else {
			assert(colsize == new_blob_size);
			tds_get_n(tds, blob->textvalue, colsize);
		}
	} else {		/* non-numeric and non-blob */
		if (curcol->char_conv) {
			if (tds_get_char_data(tds, (char *) dest, colsize, curcol) == TDS_FAIL)
				return TDS_FAIL;
		} else {	
			/*
			 * special case, some servers seem to return more data in some conditions 
			 * (ASA 7 returning 4 byte nullable integer)
			 */
			int discard_len = 0;
			if (colsize > curcol->column_size) {
				discard_len = colsize - curcol->column_size;
				colsize = curcol->column_size;
			}
			if (tds_get_n(tds, dest, colsize) == NULL)
				return TDS_FAIL;
			if (discard_len > 0)
				tds_get_n(tds, NULL, discard_len);
			curcol->column_cur_size = colsize;
		}

		/* pad (UNI)CHAR and BINARY types */
		fillchar = 0;
		switch (curcol->column_type) {
		/* extra handling for SYBLONGBINARY */
		case SYBLONGBINARY:
			if (curcol->column_usertype != USER_UNICHAR_TYPE)
				break;
		case SYBCHAR:
		case XSYBCHAR:
			if (curcol->column_size != curcol->on_server.column_size)
				break;
			/* FIXME use client charset */
			fillchar = ' ';
		case SYBBINARY:
		case XSYBBINARY:
			if (colsize < curcol->column_size)
				memset(dest + colsize, fillchar, curcol->column_size - colsize);
			colsize = curcol->column_size;
			break;
		}

		if (curcol->column_type == SYBDATETIME4) {
			tdsdump_log(TDS_DBG_INFO1, "datetime4 %d %d %d %d\n", dest[0], dest[1], dest[2], dest[3]);
		}
	}

#ifdef WORDS_BIGENDIAN
	/*
	 * MS SQL Server 7.0 has broken date types from big endian
	 * machines, this swaps the low and high halves of the
	 * affected datatypes
	 *
	 * Thought - this might be because we don't have the
	 * right flags set on login.  -mjs
	 *
	 * Nope its an actual MS SQL bug -bsb
	 */
	if (tds->broken_dates &&
	    (curcol->column_type == SYBDATETIME ||
	     curcol->column_type == SYBDATETIME4 ||
	     curcol->column_type == SYBDATETIMN ||
	     curcol->column_type == SYBMONEY ||
	     curcol->column_type == SYBMONEY4 || (curcol->column_type == SYBMONEYN && curcol->column_size > 4)))
		/*
		 * above line changed -- don't want this for 4 byte SYBMONEYN
		 * values (mlilback, 11/7/01)
		 */
	{
		unsigned char temp_buf[8];

		memcpy(temp_buf, dest, colsize / 2);
		memcpy(dest, &dest[colsize / 2], colsize / 2);
		memcpy(&dest[colsize / 2], temp_buf, colsize / 2);
	}
	if (tds->emul_little_endian && !is_numeric_type(curcol->column_type)) {
		tdsdump_log(TDS_DBG_INFO1, "swapping coltype %d\n", tds_get_conversion_type(curcol->column_type, colsize));
		tds_swap_datatype(tds_get_conversion_type(curcol->column_type, colsize), dest);
	}
#endif
	return TDS_SUCCEED;
}

/**
 * tds_process_row() processes rows and places them in the row buffer.
 */
static int
tds_process_row(TDSSOCKET * tds)
{
	int i;
	TDSCOLUMN *curcol;
	TDSRESULTINFO *info;

	info = tds->current_results;
	if (!info)
		return TDS_FAIL;

	info->row_count++;
	for (i = 0; i < info->num_cols; i++) {
		curcol = info->columns[i];
		if (tds_get_data(tds, curcol, info->current_row, i) != TDS_SUCCEED)
			return TDS_FAIL;
	}
	return TDS_SUCCEED;
}

/**
 * tds_process_end() processes any of the DONE, DONEPROC, or DONEINPROC
 * tokens.
 * @param marker     TDS token number
 * @param flags_parm filled with bit flags (see TDS_DONE_ constants). 
 *        Is NULL nothing is returned
 */
static TDS_INT
tds_process_end(TDSSOCKET * tds, int marker, int *flags_parm)
{
	int more_results, was_cancelled, error, done_count_valid;
	int tmp, state;

	tmp = tds_get_smallint(tds);

	state = tds_get_smallint(tds);

	more_results = (tmp & TDS_DONE_MORE_RESULTS) != 0;
	was_cancelled = (tmp & TDS_DONE_CANCELLED) != 0;
	error = (tmp & TDS_DONE_ERROR) != 0;
	done_count_valid = (tmp & TDS_DONE_COUNT) != 0;


	tdsdump_log(TDS_DBG_FUNC, "tds_process_end: more_results = %d\n"
		    "\t\twas_cancelled = %d\n"
		    "\t\terror = %d\n"
		    "\t\tdone_count_valid = %d\n", more_results, was_cancelled, error, done_count_valid);

	if (tds->res_info) {
		tds->res_info->more_results = more_results;
		if (tds->current_results == NULL)
			tds->current_results = tds->res_info;

	}

	if (flags_parm)
		*flags_parm = tmp;

	if (was_cancelled || !(more_results)) {
		tdsdump_log(TDS_DBG_FUNC, "tds_process_end() state set to TDS_IDLE\n");
		tds->state = TDS_IDLE;
	}

	if (IS_TDSDEAD(tds))
		return TDS_FAIL;

	/*
	 * rows affected is in the tds struct because a query may affect rows but
	 * have no result set.
	 */

	if (done_count_valid) {
		tds->rows_affected = tds_get_int(tds);
		tdsdump_log(TDS_DBG_FUNC, "                rows_affected = %d\n", tds->rows_affected);
	} else {
		tmp = tds_get_int(tds);	/* throw it away */
		tds->rows_affected = TDS_NO_COUNT;
	}

	if (IS_TDSDEAD(tds))
		return TDS_FAIL;

	return TDS_SUCCEED;
}



/**
 * tds_client_msg() sends a message to the client application from the CLI or
 * TDS layer. A client message is one that is generated from with the library
 * and not from the server.  The message is sent to the CLI (the 
 * err_handler) so that it may forward it to the client application or
 * discard it if no msg handler has been by the application. tds->parent
 * contains a void pointer to the parent of the tds socket. This can be cast
 * back into DBPROCESS or CS_CONNECTION by the CLI and used to determine the
 * proper recipient function for this message.
 * \todo This procedure is deprecated, because the client libraries use differing messages and message numbers.
 * 	The general approach is to emit ct-lib error information and let db-lib and ODBC map that to their number and text.  
 */
int
tds_client_msg(TDSCONTEXT * tds_ctx, TDSSOCKET * tds, int msgnum, int level, int state, int line, const char *msg_text)
{
	int ret;
	TDSMESSAGE msg;

	if (tds_ctx->err_handler) {
		memset(&msg, 0, sizeof(TDSMESSAGE));
		msg.msg_number = msgnum;
		msg.msg_level = level;	/* severity? */
		msg.msg_state = state;
		/* TODO is possible to avoid copy of strings ? */
		msg.server = strdup("OpenClient");
		msg.line_number = line;
		msg.message = strdup(msg_text);
		if (msg.sql_state == NULL)
			msg.sql_state = tds_alloc_client_sqlstate(msg.msg_number);
		ret = tds_ctx->err_handler(tds_ctx, tds, &msg);
		tds_free_msg(&msg);
#if 1
		/*
		 * error handler may return: 
		 * INT_EXIT -- Print an error message, and exit application, . returning an error to the OS.  
		 * INT_CANCEL -- Return FAIL to the db-lib function that caused the error. 
		 * For SQLETIME errors only, call dbcancel() to try to cancel the current command batch 
		 * 	and flush any pending results. Break the connection if dbcancel() times out, 
		 * INT_CONTINUE -- For SQLETIME, wait for one additional time-out period, then call the error handler again. 
		 *  	Else treat as INT_CANCEL. 
		 */
#else
		/*
		 * This was bogus afaict.  
		 * Definitely, it's a mistake to set the state to TDS_DEAD for information messages when the handler  
		 * returns INT_CANCEL, at least according to Microsoft's documentation.  
		 * --jkl
		 */  
		/*
		 * message handler returned FAIL/CS_FAIL
		 * mark socket as dead
		 */
		if (ret && tds) {
			/* TODO close socket too ?? */
			tds->state = TDS_DEAD;
		}
#endif
	}

	tdsdump_log(TDS_DBG_FUNC, "tds_client_msg: #%d: \"%s\".  Connection state is now %d.  \n", msgnum, msg_text, tds ? (int)tds->state : -1);

	return 0;
}

/**
 * tds_process_env_chg() 
 * when ever certain things change on the server, such as database, character
 * set, language, or block size.  A environment change message is generated
 * There is no action taken currently, but certain functions at the CLI level
 * that return the name of the current database will need to use this.
 */
static int
tds_process_env_chg(TDSSOCKET * tds)
{
	int size, type;
	char *oldval = NULL;
	char *newval = NULL;
	char **dest;
	int new_block_size;
	int lcid;
	int memrc = 0;

	size = tds_get_smallint(tds);
	/*
	 * this came in a patch, apparently someone saw an env message
	 * that was different from what we are handling? -- brian
	 * changed back because it won't handle multibyte chars -- 7.0
	 */
	/* tds_get_n(tds,NULL,size); */

	type = tds_get_byte(tds);

	/*
	 * handle collate default change (if you change db or during login)
	 * this environment is not a string so need different handles
	 */
	if (type == TDS_ENV_SQLCOLLATION) {
		/* save new collation */
		size = tds_get_byte(tds);
		memset(tds->collation, 0, 5);
		if (size < 5) {
			tds_get_n(tds, tds->collation, size);
		} else {
			tds_get_n(tds, tds->collation, 5);
			tds_get_n(tds, NULL, size - 5);
			lcid = (tds->collation[0] + ((int) tds->collation[1] << 8) + ((int) tds->collation[2] << 16)) & 0xffffflu;
			tds7_srv_charset_changed(tds, tds->collation[4], lcid);
		}
		/* discard old one */
		tds_get_n(tds, NULL, tds_get_byte(tds));
		return TDS_SUCCEED;
	}

	/* fetch the new value */
	memrc += tds_alloc_get_string(tds, &newval, tds_get_byte(tds));

	/* fetch the old value */
	memrc += tds_alloc_get_string(tds, &oldval, tds_get_byte(tds));

	if (memrc != 0) {
		if (newval != NULL)
			free(newval);
		if (oldval != NULL)
			free(oldval);
		return TDS_FAIL;
	}

	dest = NULL;
	switch (type) {
	case TDS_ENV_PACKSIZE:
		new_block_size = atoi(newval);
		if (new_block_size > tds->env->block_size) {
			tdsdump_log(TDS_DBG_INFO1, "increasing block size from %s to %d\n", oldval, new_block_size);
			/* 
			 * I'm not aware of any way to shrink the 
			 * block size but if it is possible, we don't 
			 * handle it.
			 */
			/* Reallocate buffer if impossible (strange values from server or out of memory) use older buffer */
			tds_realloc_socket(tds, new_block_size);
		}
		break;
	case TDS_ENV_DATABASE:
		dest = &tds->env->database;
		break;
	case TDS_ENV_LANG:
		dest = &tds->env->language;
		break;
	case TDS_ENV_CHARSET:
		dest = &tds->env->charset;
		tds_srv_charset_changed(tds, newval);
		break;
	}
	if (tds->env_chg_func) {
		(*(tds->env_chg_func)) (tds, type, oldval, newval);
	}

	if (oldval)
		free(oldval);
	if (newval) {
		if (dest) {
			if (*dest)
				free(*dest);
			*dest = newval;
		} else
			free(newval);
	}

	return TDS_SUCCEED;
}

/**
 * tds_process_msg() is called for MSG, ERR, or EED tokens and is responsible
 * for calling the CLI's message handling routine
 * returns TDS_SUCCEED if informational, TDS_ERROR if error.
 */
static int
tds_process_msg(TDSSOCKET * tds, int marker)
{
	int rc;
	int len;
	int len_sqlstate;
	int has_eed = 0;
	TDSMESSAGE msg;

	/* make sure message has been freed */
	memset(&msg, 0, sizeof(TDSMESSAGE));

	/* packet length */
	len = tds_get_smallint(tds);

	/* message number */
	rc = tds_get_int(tds);
	msg.msg_number = rc;

	/* msg state */
	msg.msg_state = tds_get_byte(tds);

	/* msg level */
	msg.msg_level = tds_get_byte(tds);

	/* determine if msg or error */
	switch (marker) {
	case TDS_EED_TOKEN:
		if (msg.msg_level <= 10)
			msg.priv_msg_type = 0;
		else
			msg.priv_msg_type = 1;

		/* read SQL state */
		len_sqlstate = tds_get_byte(tds);
		msg.sql_state = (char *) malloc(len_sqlstate + 1);
		if (!msg.sql_state) {
			tds_free_msg(&msg);
			return TDS_FAIL;
		}

		tds_get_n(tds, msg.sql_state, len_sqlstate);
		msg.sql_state[len_sqlstate] = '\0';

		/* do a better mapping using native errors */
		if (strcmp(msg.sql_state, "ZZZZZ") == 0)
			TDS_ZERO_FREE(msg.sql_state);

		/* if has_eed = 1, extended error data follows */
		has_eed = tds_get_byte(tds);

		/* junk status and transaction state */
		tds_get_smallint(tds);
		break;
	case TDS_INFO_TOKEN:
		msg.priv_msg_type = 0;
		break;
	case TDS_ERROR_TOKEN:
		msg.priv_msg_type = 1;
		break;
	default:
		tdsdump_log(TDS_DBG_ERROR, "__FILE__:__LINE__: tds_process_msg() called with unknown marker '%d'!\n", (int) marker);
		tds_free_msg(&msg);
		return TDS_FAIL;
	}

	rc = 0;
	/* the message */
	rc += tds_alloc_get_string(tds, &msg.message, tds_get_smallint(tds));

	/* server name */
	rc += tds_alloc_get_string(tds, &msg.server, tds_get_byte(tds));

	/* stored proc name if available */
	rc += tds_alloc_get_string(tds, &msg.proc_name, tds_get_byte(tds));

	/* line number in the sql statement where the problem occured */
	msg.line_number = tds_get_smallint(tds);

	/*
	 * If the server doesen't provide an sqlstate, map one via server native errors
	 * I'm assuming there is not a protocol I'm missing to fetch these from the server?
	 * I know sybase has an sqlstate column in it's sysmessages table, mssql doesn't and
	 * TDS_EED_TOKEN is not being called for me.
	 */
	if (msg.sql_state == NULL)
		msg.sql_state = tds_alloc_lookup_sqlstate(tds, msg.msg_number);


	/* In case extended error data is sent, we just try to discard it */
	if (has_eed == 1) {
		int next_marker;
		for (;;) {
			switch (next_marker = tds_get_byte(tds)) {
			case TDS5_PARAMFMT_TOKEN:
			case TDS5_PARAMFMT2_TOKEN:
			case TDS5_PARAMS_TOKEN:
				if (tds_process_default_tokens(tds, next_marker) != TDS_SUCCEED)
					++rc;
				continue;
			}
			break;
		}
		tds_unget_byte(tds);
	}

	/*
	 * call the msg_handler that was set by an upper layer
	 * (dblib, ctlib or some other one).  Call it with the pointer to
	 * the "parent" structure.
	 */

	if (rc != 0) {
		tds_free_msg(&msg);
		return TDS_ERROR;
	}
	
	/* special case, */
	if (marker == TDS_EED_TOKEN && tds->cur_dyn && !TDS_IS_MSSQL(tds) && msg.msg_number == 2782) {
		/* we must emulate prepare */
		tds->cur_dyn->emulated = 1;
	} else {
		/* EED can be followed to PARAMFMT/PARAMS, do not store it in dynamic */
		tds->cur_dyn = NULL;

		if (tds->tds_ctx->msg_handler) {
			tds->tds_ctx->msg_handler(tds->tds_ctx, tds, &msg);
		} else if (msg.msg_number) {
			tdsdump_log(TDS_DBG_WARN,
				    "Msg %d, Level %d, State %d, Server %s, Line %d\n%s\n",
				    msg.msg_number,
				    msg.msg_level,
				    msg.msg_state, msg.server, msg.line_number, msg.message);
		}
	}
	tds_free_msg(&msg);
	return TDS_SUCCEED;
}

/**
 * Read a string from wire in a new allocated buffer
 * @param len length of string to read
 */
int
tds_alloc_get_string(TDSSOCKET * tds, char **string, int len)
{
	char *s;
	int out_len;

	if (len < 0) {
		*string = NULL;
		return 0;
	}

	/* assure sufficient space for every conversion */
	s = (char *) malloc(len * 4 + 1);
	out_len = tds_get_string(tds, len, s, len * 4);
	if (!s) {
		*string = NULL;
		return -1;
	}
	s = realloc(s, out_len + 1);
	s[out_len] = '\0';
	*string = s;
	return 0;
}

/**
 * tds_process_cancel() processes the incoming token stream until it finds
 * an end token (DONE, DONEPROC, DONEINPROC) with the cancel flag set.
 * a that point the connetion should be ready to handle a new query.
 */
int
tds_process_cancel(TDSSOCKET * tds)
{
	int marker, done_flags = 0;
	int retcode = TDS_SUCCEED;

	tds->queryStarttime = 0;
	/* TODO support TDS5 cancel, wait for cancel packet first, then wait for done */
	do {
		marker = tds_get_byte(tds);
		if (marker == TDS_DONE_TOKEN) {
			{
				if (tds_process_end(tds, marker, &done_flags) == TDS_FAIL)
					retcode = TDS_FAIL;
			}
		} else if (marker == 0) {
			done_flags = TDS_DONE_CANCELLED;
		} else {
			retcode = tds_process_default_tokens(tds, marker);
		}
	} while (retcode == TDS_SUCCEED && !(done_flags & TDS_DONE_CANCELLED));


	if (retcode == TDS_SUCCEED && !IS_TDSDEAD(tds))
		tds->state = TDS_IDLE;
	else
		retcode = TDS_FAIL;

	/* TODO clear cancelled results */
	return retcode;
}

/**
 * set the null bit for the given column in the row buffer
 */
void
tds_set_null(unsigned char *current_row, int column)
{
	int bytenum = ((unsigned int) column) / 8u;
	int bit = ((unsigned int) column) % 8u;
	unsigned char mask = 1 << bit;

	tdsdump_log(TDS_DBG_INFO1, "setting column %d NULL bit\n", column);
	current_row[bytenum] |= mask;
}

/**
 * clear the null bit for the given column in the row buffer
 */
void
tds_clr_null(unsigned char *current_row, int column)
{
	int bytenum = ((unsigned int) column) / 8u;
	int bit = ((unsigned int) column) % 8u;
	unsigned char mask = ~(1 << bit);

	tdsdump_log(TDS_DBG_INFO1, "clearing column %d NULL bit\n", column);
	current_row[bytenum] &= mask;
}

/**
 * Return the null bit for the given column in the row buffer
 */
int
tds_get_null(unsigned char *current_row, int column)
{
	int bytenum = ((unsigned int) column) / 8u;
	int bit = ((unsigned int) column) % 8u;

	return (current_row[bytenum] >> bit) & 1;
}

/**
 * Find a dynamic given string id
 * @return dynamic or NULL is not found
 * @param id   dynamic id to search
 */
TDSDYNAMIC *
tds_lookup_dynamic(TDSSOCKET * tds, char *id)
{
	int i;

	for (i = 0; i < tds->num_dyns; i++) {
		if (!strcmp(tds->dyns[i]->id, id)) {
			return tds->dyns[i];
		}
	}
	return NULL;
}

/**
 * tds_process_dynamic()
 * finds the element of the dyns array for the id
 */
static TDSDYNAMIC *
tds_process_dynamic(TDSSOCKET * tds)
{
	int token_sz;
	unsigned char type, status;
	int id_len;
	char id[TDS_MAX_DYNID_LEN + 1];
	int drain = 0;

	token_sz = tds_get_smallint(tds);
	type = tds_get_byte(tds);
	status = tds_get_byte(tds);
	/* handle only acknowledge */
	if (type != 0x20) {
		tdsdump_log(TDS_DBG_ERROR, "Unrecognized TDS5_DYN type %x\n", type);
		tds_get_n(tds, NULL, token_sz - 2);
		return NULL;
	}
	id_len = tds_get_byte(tds);
	if (id_len > TDS_MAX_DYNID_LEN) {
		drain = id_len - TDS_MAX_DYNID_LEN;
		id_len = TDS_MAX_DYNID_LEN;
	}
	id_len = tds_get_string(tds, id_len, id, TDS_MAX_DYNID_LEN);
	id[id_len] = '\0';
	if (drain) {
		tds_get_string(tds, drain, NULL, drain);
	}
	return tds_lookup_dynamic(tds, id);
}

static int
tds_process_dyn_result(TDSSOCKET * tds)
{
	int hdrsize;
	int col, num_cols;
	TDSCOLUMN *curcol;
	TDSPARAMINFO *info;
	TDSDYNAMIC *dyn;

	hdrsize = tds_get_smallint(tds);
	num_cols = tds_get_smallint(tds);

	if (tds->cur_dyn) {
		dyn = tds->cur_dyn;
		tds_free_param_results(dyn->res_info);
		/* read number of columns and allocate the columns structure */
		if ((dyn->res_info = tds_alloc_results(num_cols)) == NULL)
			return TDS_FAIL;
		info = dyn->res_info;
	} else {
		tds_free_param_results(tds->param_info);
		if ((tds->param_info = tds_alloc_results(num_cols)) == NULL)
			return TDS_FAIL;
		info = tds->param_info;
	}
	tds->current_results = info;

	for (col = 0; col < info->num_cols; col++) {
		curcol = info->columns[col];

		tds_get_data_info(tds, curcol, 1);

		/* skip locale information */
		tds_get_n(tds, NULL, tds_get_byte(tds));

		tds_add_row_column_size(info, curcol);
	}

	if ((info->current_row = tds_alloc_row(info)) != NULL)
		return TDS_SUCCEED;
	else
		return TDS_FAIL;
}

/**
 *  New TDS 5.0 token for describing output parameters
 */
static int
tds5_process_dyn_result2(TDSSOCKET * tds)
{
	int hdrsize;
	int col, num_cols;
	TDSCOLUMN *curcol;
	TDSPARAMINFO *info;
	TDSDYNAMIC *dyn;

	hdrsize = tds_get_int(tds);
	num_cols = tds_get_smallint(tds);

	if (tds->cur_dyn) {
		dyn = tds->cur_dyn;
		tds_free_param_results(dyn->res_info);
		/* read number of columns and allocate the columns structure */
		if ((dyn->res_info = tds_alloc_results(num_cols)) == NULL)
			return TDS_FAIL;
		info = dyn->res_info;
	} else {
		tds_free_param_results(tds->param_info);
		if ((tds->param_info = tds_alloc_results(num_cols)) == NULL)
			return TDS_FAIL;
		info = tds->param_info;
	}
	tds->current_results = info;

	for (col = 0; col < info->num_cols; col++) {
		curcol = info->columns[col];

		/* TODO reuse tds_get_data_info code, sligthly different */

		/* column name */
		/*
		 * column_name can store up to 512 bytes however column_namelen can hold
		 * that size so limit column name, fixed in 0.64 -- freddy77
		 */
		curcol->column_namelen =
			tds_get_string(tds, tds_get_byte(tds), curcol->column_name, 255);
		curcol->column_name[curcol->column_namelen] = '\0';

		/* column status */
		curcol->column_flags = tds_get_int(tds);
		curcol->column_nullable = (curcol->column_flags & 0x20) > 0;

		/* user type */
		curcol->column_usertype = tds_get_int(tds);

		/* column type */
		tds_set_column_type(curcol, tds_get_byte(tds));

		/* FIXME this should be done by tds_set_column_type */
		curcol->column_varint_size = tds5_get_varint_size(curcol->column_type);
		/* column size */
		switch (curcol->column_varint_size) {
		case 5:
			curcol->column_size = tds_get_int(tds);
			break;
		case 4:
			if (curcol->column_type == SYBTEXT || curcol->column_type == SYBIMAGE) {
				curcol->column_size = tds_get_int(tds);
				/* read table name */
				curcol->table_namelen =
					tds_get_string(tds, tds_get_smallint(tds), curcol->table_name,
						       sizeof(curcol->table_name) - 1);
			} else
				tdsdump_log(TDS_DBG_INFO1, "UNHANDLED TYPE %x\n", curcol->column_type);
			break;
		case 2:
			curcol->column_size = tds_get_smallint(tds);
			break;
		case 1:
			curcol->column_size = tds_get_byte(tds);
			break;
		case 0:
			break;
		}

		/* Adjust column size according to client's encoding */
		curcol->on_server.column_size = curcol->column_size;
		adjust_character_column_size(tds, curcol);

		/* numeric and decimal have extra info */
		if (is_numeric_type(curcol->column_type)) {
			curcol->column_prec = tds_get_byte(tds);	/* precision */
			curcol->column_scale = tds_get_byte(tds);	/* scale */
		}

		/* discard Locale */
		tds_get_n(tds, NULL, tds_get_byte(tds));

		tds_add_row_column_size(info, curcol);

		tdsdump_log(TDS_DBG_INFO1, "elem %d:\n", col);
		tdsdump_log(TDS_DBG_INFO1, "\tcolumn_name=[%s]\n", curcol->column_name);
		tdsdump_log(TDS_DBG_INFO1, "\tflags=%x utype=%d type=%d varint=%d\n",
			    curcol->column_flags, curcol->column_usertype, curcol->column_type, curcol->column_varint_size);
		tdsdump_log(TDS_DBG_INFO1, "\tcolsize=%d prec=%d scale=%d\n",
			    curcol->column_size, curcol->column_prec, curcol->column_scale);
	}

	if ((info->current_row = tds_alloc_row(info)) != NULL)
		return TDS_SUCCEED;
	else
		return TDS_FAIL;
}

/**
 * tds_get_token_size() returns the size of a fixed length token
 * used by tds_process_cancel() to determine how to read past a token
 */
int
tds_get_token_size(int marker)
{
	/* TODO finish */
	switch (marker) {
	case TDS_DONE_TOKEN:
	case TDS_DONEPROC_TOKEN:
	case TDS_DONEINPROC_TOKEN:
		return 8;
	case TDS_RETURNSTATUS_TOKEN:
		return 4;
	case TDS_PROCID_TOKEN:
		return 8;
	default:
		return 0;
	}
}

void
tds_swap_datatype(int coltype, unsigned char *buf)
{
	TDS_NUMERIC *num;

	switch (coltype) {
	case SYBINT2:
		tds_swap_bytes(buf, 2);
		break;
	case SYBINT4:
	case SYBMONEY4:
	case SYBREAL:
		tds_swap_bytes(buf, 4);
		break;
	case SYBINT8:
	case SYBFLT8:
		tds_swap_bytes(buf, 8);
		break;
	case SYBMONEY:
	case SYBDATETIME:
		tds_swap_bytes(buf, 4);
		tds_swap_bytes(&buf[4], 4);
		break;
	case SYBDATETIME4:
		tds_swap_bytes(buf, 2);
		tds_swap_bytes(&buf[2], 2);
		break;
		/*
		 * should we place numeric conversion in another place ??
		 * this is not used for big/little-endian conversion...
		 */
	case SYBNUMERIC:
	case SYBDECIMAL:
		num = (TDS_NUMERIC *) buf;
		/* swap the sign */
		num->array[0] = (num->array[0] == 0) ? 1 : 0;
		/* swap the data */
		tds_swap_bytes(&(num->array[1]), tds_numeric_bytes_per_prec[num->precision] - 1);
		break;
	case SYBUNIQUE:
		tds_swap_bytes(buf, 4);
		tds_swap_bytes(&buf[4], 2);
		tds_swap_bytes(&buf[6], 2);
		break;
	}
}

/**
 * tds5_get_varint_size5() returns the size of a variable length integer
 * returned in a TDS 5.1 result string
 */
/* TODO can we use tds_get_varint_size ?? */
static int
tds5_get_varint_size(int datatype)
{
	switch (datatype) {
	case SYBTEXT:
	case SYBNTEXT:
	case SYBIMAGE:
	case SYBVARIANT:
		return 4;

	case SYBLONGBINARY:
	case XSYBCHAR:
		return 5;	/* Special case */

	case SYBVOID:
	case SYBINT1:
	case SYBBIT:
	case SYBINT2:
	case SYBINT4:
	case SYBINT8:
	case SYBDATETIME4:
	case SYBREAL:
	case SYBMONEY:
	case SYBDATETIME:
	case SYBFLT8:
	case SYBMONEY4:
	case SYBSINT1:
	case SYBUINT2:
	case SYBUINT4:
	case SYBUINT8:
		return 0;

	case XSYBNVARCHAR:
	case XSYBVARCHAR:
	case XSYBBINARY:
	case XSYBVARBINARY:
		return 2;

	default:
		return 1;
	}
}

/**
 * tds_process_compute_names() processes compute result sets.  
 */
static int
tds_process_compute_names(TDSSOCKET * tds)
{
	int hdrsize;
	int remainder;
	int num_cols = 0;
	int col;
	int memrc = 0;
	TDS_SMALLINT compute_id = 0;
	TDS_TINYINT namelen;
	TDSCOMPUTEINFO *info;
	TDSCOLUMN *curcol;

	struct namelist
	{
		char name[256];
		int namelen;
		struct namelist *nextptr;
	};

	struct namelist *topptr = NULL;
	struct namelist *curptr = NULL;
	struct namelist *freeptr = NULL;

	hdrsize = tds_get_smallint(tds);
	remainder = hdrsize;
	tdsdump_log(TDS_DBG_INFO1, "processing tds5 compute names. remainder = %d\n", remainder);

	/*
	 * compute statement id which this relates
	 * to. You can have more than one compute
	 * statement in a SQL statement  
	 */

	compute_id = tds_get_smallint(tds);
	remainder -= 2;

	while (remainder) {
		namelen = tds_get_byte(tds);
		remainder--;
		if (topptr == (struct namelist *) NULL) {
			if ((topptr = (struct namelist *) malloc(sizeof(struct namelist))) == NULL) {
				memrc = -1;
				break;
			}
			curptr = topptr;
			curptr->nextptr = NULL;
		} else {
			if ((curptr->nextptr = (struct namelist *) malloc(sizeof(struct namelist))) == NULL) {
				memrc = -1;
				break;
			}
			curptr = curptr->nextptr;
			curptr->nextptr = NULL;
		}
		if (namelen == 0)
			strcpy(curptr->name, "");
		else {
			namelen = tds_get_string(tds, namelen, curptr->name, sizeof(curptr->name) - 1);
			curptr->name[namelen] = 0;
			remainder -= namelen;
		}
		curptr->namelen = namelen;
		num_cols++;
		tdsdump_log(TDS_DBG_INFO1, "processing tds5 compute names. remainder = %d\n", remainder);
	}

	tdsdump_log(TDS_DBG_INFO1, "processing tds5 compute names. num_cols = %d\n", num_cols);

	if ((tds->comp_info = tds_alloc_compute_results(&(tds->num_comp_info), tds->comp_info, num_cols, 0)) == NULL)
		memrc = -1;

	tdsdump_log(TDS_DBG_INFO1, "processing tds5 compute names. num_comp_info = %d\n", tds->num_comp_info);

	info = tds->comp_info[tds->num_comp_info - 1];
	tds->current_results = info;

	info->computeid = compute_id;

	curptr = topptr;

	if (memrc == 0) {
		for (col = 0; col < num_cols; col++) {
			curcol = info->columns[col];

			assert(strlen(curcol->column_name) == curcol->column_namelen);
			memcpy(curcol->column_name, curptr->name, curptr->namelen + 1);
			curcol->column_namelen = curptr->namelen;

			freeptr = curptr;
			curptr = curptr->nextptr;
			free(freeptr);
		}
		return TDS_SUCCEED;
	} else {
		while (curptr != NULL) {
			freeptr = curptr;
			curptr = curptr->nextptr;
			free(freeptr);
		}
		return TDS_FAIL;
	}
}

/**
 * tds7_process_compute_result() processes compute result sets for TDS 7/8.
 * They is are very  similar to normal result sets.
 */
static int
tds7_process_compute_result(TDSSOCKET * tds)
{
	int col, num_cols;
	TDS_TINYINT by_cols;
	TDS_TINYINT *cur_by_col;
	TDS_SMALLINT compute_id;
	TDSCOLUMN *curcol;
	TDSCOMPUTEINFO *info;

	/*
	 * number of compute columns returned - so
	 * COMPUTE SUM(x), AVG(x)... would return
	 * num_cols = 2
	 */

	num_cols = tds_get_smallint(tds);

	tdsdump_log(TDS_DBG_INFO1, "processing tds7 compute result. num_cols = %d\n", num_cols);

	/*
	 * compute statement id which this relates
	 * to. You can have more than one compute
	 * statement in a SQL statement
	 */

	compute_id = tds_get_smallint(tds);

	tdsdump_log(TDS_DBG_INFO1, "processing tds7 compute result. compute_id = %d\n", compute_id);
	/*
	 * number of "by" columns in compute - so
	 * COMPUTE SUM(x) BY a, b, c would return
	 * by_cols = 3
	 */

	by_cols = tds_get_byte(tds);
	tdsdump_log(TDS_DBG_INFO1, "processing tds7 compute result. by_cols = %d\n", by_cols);

	if ((tds->comp_info = tds_alloc_compute_results(&(tds->num_comp_info), tds->comp_info, num_cols, by_cols)) == NULL)
		return TDS_FAIL;

	tdsdump_log(TDS_DBG_INFO1, "processing tds7 compute result. num_comp_info = %d\n", tds->num_comp_info);

	info = tds->comp_info[tds->num_comp_info - 1];
	tds->current_results = info;

	tdsdump_log(TDS_DBG_INFO1, "processing tds7 compute result. point 0\n");

	info->computeid = compute_id;

	/*
	 * the by columns are a list of the column
	 * numbers in the select statement
	 */

	cur_by_col = info->bycolumns;
	for (col = 0; col < by_cols; col++) {
		/* FIXME *cur_by_col is smaller... */
		*cur_by_col = tds_get_smallint(tds);
		cur_by_col++;
	}
	tdsdump_log(TDS_DBG_INFO1, "processing tds7 compute result. point 1\n");

	for (col = 0; col < num_cols; col++) {
		tdsdump_log(TDS_DBG_INFO1, "processing tds7 compute result. point 2\n");
		curcol = info->columns[col];

		curcol->column_operator = tds_get_byte(tds);
		curcol->column_operand = tds_get_smallint(tds);

		tds7_get_data_info(tds, curcol);

		if (!curcol->column_namelen) {
			strcpy(curcol->column_name, tds_pr_op(curcol->column_operator));
			curcol->column_namelen = strlen(curcol->column_name);
		}

		tds_add_row_column_size(info, curcol);
	}

	/* all done now allocate a row for tds_process_row to use */
	tdsdump_log(TDS_DBG_INFO1, "processing tds7 compute result. point 5 \n");
	if ((info->current_row = tds_alloc_compute_row(info)) != NULL)
		return TDS_SUCCEED;
	else
		return TDS_FAIL;
}

static int 
tds_process_cursor_tokens(TDSSOCKET * tds)
{
	TDS_SMALLINT hdrsize;
	TDS_INT rowcount;
	TDS_INT cursor_id;
	TDS_TINYINT namelen;
	char name[30];	
	unsigned char cursor_cmd;
	TDS_SMALLINT cursor_status;
	TDSCURSOR *cursor;
	
	hdrsize  = tds_get_smallint(tds);
	cursor_id = tds_get_int(tds);
	hdrsize  -= sizeof(TDS_INT);
	if (cursor_id == 0){
		namelen = (int)tds_get_byte(tds);
		hdrsize -= 1;
		tds_get_n(tds, name, namelen);
		hdrsize -= namelen;
	}
	cursor_cmd    = tds_get_byte(tds);
	cursor_status = tds_get_smallint(tds);
	hdrsize -= 3;

	if (hdrsize == sizeof(TDS_INT))
		rowcount = tds_get_int(tds); 

	if (tds->client_cursor_id) {
		tdsdump_log(TDS_DBG_FUNC, "locating cursor_id %d\n", tds->client_cursor_id);
		cursor = tds->cursor; 
		while (cursor && cursor->client_cursor_id != tds->client_cursor_id)
			cursor = cursor->next;
	
		if (!cursor) {
			tdsdump_log(TDS_DBG_FUNC, "tds_process_cursor_tokens() : cannot find cursor_id %d\n", tds->client_cursor_id);
			return TDS_FAIL;
		}
		cursor->cursor_id = cursor_id;
		if ((cursor_status & TDS_CUR_ISTAT_DEALLOC) != 0)
			tds_free_cursor(tds, tds->client_cursor_id);
	} 
	return TDS_SUCCEED;
}

int
tds5_send_optioncmd(TDSSOCKET * tds, TDS_OPTION_CMD tds_command, TDS_OPTION tds_option, TDS_OPTION_ARG * ptds_argument,
		    TDS_INT * ptds_argsize)
{
	static const TDS_TINYINT token = TDS_OPTIONCMD_TOKEN;
	TDS_TINYINT expected_acknowledgement = 0;
	int marker, status;

	TDS_TINYINT command = tds_command;
	TDS_TINYINT option = tds_option;
	TDS_TINYINT argsize = (*ptds_argsize == TDS_NULLTERM) ? 1 + strlen(ptds_argument->c) : *ptds_argsize;

	TDS_SMALLINT length = sizeof(command) + sizeof(option) + sizeof(argsize) + argsize;

	tdsdump_log(TDS_DBG_INFO1, "entering %s::tds_send_optioncmd() \n", __FILE__);

	assert(IS_TDS50(tds));
	assert(ptds_argument);

	tds_put_tinyint(tds, token);
	tds_put_smallint(tds, length);
	tds_put_tinyint(tds, command);
	tds_put_tinyint(tds, option);
	tds_put_tinyint(tds, argsize);

	switch (*ptds_argsize) {
	case 1:
		tds_put_tinyint(tds, ptds_argument->ti);
		break;
	case 4:
		tds_put_int(tds, ptds_argument->i);
		break;
	case TDS_NULLTERM:
		tds_put_string(tds, ptds_argument->c, argsize);
		break;
	default:
		tdsdump_log(TDS_DBG_INFO1, "tds_send_optioncmd: failed: argsize is %d.\n", *ptds_argsize);
		return -1;
	}

	tds_flush_packet(tds);

	/* TODO: read the server's response.  Don't use this function yet. */

	switch (command) {
	case TDS_OPT_SET:
	case TDS_OPT_DEFAULT:
		expected_acknowledgement = TDS_DONE_TOKEN;
		break;
	case TDS_OPT_LIST:
		expected_acknowledgement = TDS_OPTIONCMD_TOKEN;	/* with TDS_OPT_INFO */
		break;
	}
	while ((marker = tds_get_byte(tds)) != expected_acknowledgement) {
		if (tds_process_default_tokens(tds, marker) == TDS_FAIL)
			return TDS_FAIL;
	}

	if (marker == TDS_DONE_TOKEN) {
		tds_process_end(tds, marker, &status);
		return (TDS_DONE_FINAL == (status | TDS_DONE_FINAL)) ? TDS_SUCCEED : TDS_FAIL;
	}

	length = tds_get_smallint(tds);
	command = tds_get_byte(tds);
	option = tds_get_byte(tds);
	argsize = tds_get_byte(tds);

	if (argsize > *ptds_argsize) {
		/* return oversize length to caller, copying only as many bytes as caller provided. */
		TDS_INT was = *ptds_argsize;

		*ptds_argsize = argsize;
		argsize = was;
	}

	switch (argsize) {
	case 0:
		break;
	case 1:
		ptds_argument->ti = tds_get_byte(tds);
		break;
	case 4:
		ptds_argument->i = tds_get_int(tds);
		break;
	default:
		/* FIXME not null terminated and size not saved */
		/* FIXME do not take into account conversion */
		tds_get_string(tds, argsize, ptds_argument->c, argsize);
		break;
	}


	while ((marker = tds_get_byte(tds)) != TDS_DONE_TOKEN) {
		if (tds_process_default_tokens(tds, marker) == TDS_FAIL)
			return TDS_FAIL;
	}

	tds_process_end(tds, marker, &status);
	return (TDS_DONE_FINAL == (status | TDS_DONE_FINAL)) ? TDS_SUCCEED : TDS_FAIL;

}

static const char *
tds_pr_op(int op)
{
#define TYPE(con, s) case con: return s; break
	switch (op) {
		TYPE(SYBAOPAVG, "avg");
		TYPE(SYBAOPAVGU, "avg");
		TYPE(SYBAOPCNT, "count");
		TYPE(SYBAOPCNTU, "count");
		TYPE(SYBAOPMAX, "max");
		TYPE(SYBAOPMIN, "min");
		TYPE(SYBAOPSUM, "sum");
		TYPE(SYBAOPSUMU, "sum");
		TYPE(SYBAOPCHECKSUM_AGG, "checksum_agg");
		TYPE(SYBAOPCNT_BIG, "count");
		TYPE(SYBAOPSTDEV, "stdevp");
		TYPE(SYBAOPSTDEVP, "stdevp");
		TYPE(SYBAOPVAR, "var");
		TYPE(SYBAOPVARP, "varp");
	default:
		break;
	}
	return "";
#undef TYPE
}

const char *
tds_prtype(int token)
{
#define TYPE(con, s) case con: return s; break
	switch (token) {
		TYPE(SYBAOPAVG, "avg");
		TYPE(SYBAOPCNT, "count");
		TYPE(SYBAOPMAX, "max");
		TYPE(SYBAOPMIN, "min");
		TYPE(SYBAOPSUM, "sum");

		TYPE(SYBBINARY, "binary");
		TYPE(SYBLONGBINARY, "longbinary");
		TYPE(SYBBIT, "bit");
		TYPE(SYBBITN, "bit-null");
		TYPE(SYBCHAR, "char");
		TYPE(SYBDATETIME4, "smalldatetime");
		TYPE(SYBDATETIME, "datetime");
		TYPE(SYBDATETIMN, "datetime-null");
		TYPE(SYBDECIMAL, "decimal");
		TYPE(SYBFLT8, "float");
		TYPE(SYBFLTN, "float-null");
		TYPE(SYBIMAGE, "image");
		TYPE(SYBINT1, "tinyint");
		TYPE(SYBINT2, "smallint");
		TYPE(SYBINT4, "int");
		TYPE(SYBINT8, "bigint");
		TYPE(SYBINTN, "integer-null");
		TYPE(SYBMONEY4, "smallmoney");
		TYPE(SYBMONEY, "money");
		TYPE(SYBMONEYN, "money-null");
		TYPE(SYBNTEXT, "UCS-2 text");
		TYPE(SYBNVARCHAR, "UCS-2 varchar");
		TYPE(SYBNUMERIC, "numeric");
		TYPE(SYBREAL, "real");
		TYPE(SYBTEXT, "text");
		TYPE(SYBUNIQUE, "uniqueidentifier");
		TYPE(SYBVARBINARY, "varbinary");
		TYPE(SYBVARCHAR, "varchar");
		TYPE(SYBVARIANT, "variant");
		TYPE(SYBVOID, "void");
		TYPE(XSYBBINARY, "xbinary");
		TYPE(XSYBCHAR, "xchar");
		TYPE(XSYBNCHAR, "x UCS-2 char");
		TYPE(XSYBNVARCHAR, "x UCS-2 varchar");
		TYPE(XSYBVARBINARY, "xvarbinary");
		TYPE(XSYBVARCHAR, "xvarchar");
	default:
		break;
	}
	return "";
#undef TYPE
}

/** \@} */

static const char *
_tds_token_name(unsigned char marker)
{
	switch (marker) {

	case 0x20:
		return "TDS5_PARAMFMT2";
	case 0x22:
		return "ORDERBY2";
	case 0x61:
		return "ROWFMT2";
	case 0x71:
		return "LOGOUT";
	case 0x79:
		return "RETURNSTATUS";
	case 0x7C:
		return "PROCID";
	case 0x81:
		return "TDS7_RESULT";
	case 0x88:
		return "TDS7_COMPUTE_RESULT";
	case 0xA0:
		return "COLNAME";
	case 0xA1:
		return "COLFMT";
	case 0xA3:
		return "DYNAMIC2";
	case 0xA4:
		return "TABNAME";
	case 0xA5:
		return "COLINFO";
	case 0xA7:
		return "COMPUTE_NAMES";
	case 0xA8:
		return "COMPUTE_RESULT";
	case 0xA9:
		return "ORDERBY";
	case 0xAA:
		return "ERROR";
	case 0xAB:
		return "INFO";
	case 0xAC:
		return "PARAM";
	case 0xAD:
		return "LOGINACK";
	case 0xAE:
		return "CONTROL";
	case 0xD1:
		return "ROW";
	case 0xD3:
		return "CMP_ROW";
	case 0xD7:
		return "TDS5_PARAMS";
	case 0xE2:
		return "CAPABILITY";
	case 0xE3:
		return "ENVCHANGE";
	case 0xE5:
		return "EED";
	case 0xE6:
		return "DBRPC";
	case 0xE7:
		return "TDS5_DYNAMIC";
	case 0xEC:
		return "TDS5_PARAMFMT";
	case 0xED:
		return "AUTH";
	case 0xEE:
		return "RESULT";
	case 0xFD:
		return "DONE";
	case 0xFE:
		return "DONEPROC";
	case 0xFF:
		return "DONEINPROC";

	default:
		break;
	}

	return "";
}

/** 
 * Adjust column size according to client's encoding 
 */
static void
adjust_character_column_size(const TDSSOCKET * tds, TDSCOLUMN * curcol)
{
	if (is_unicode_type(curcol->on_server.column_type))
		curcol->char_conv = tds->char_convs[client2ucs2];

	/* Sybase UNI(VAR)CHAR fields are transmitted via SYBLONGBINARY and in UTF-16*/
	if (curcol->on_server.column_type == SYBLONGBINARY && (
		curcol->column_usertype == USER_UNICHAR_TYPE ||
		curcol->column_usertype == USER_UNIVARCHAR_TYPE)) {
		/* FIXME ucs2 is not UTF-16... */
		/* FIXME what happen if client is big endian ?? */
		curcol->char_conv = tds->char_convs[client2ucs2];
	}

	/* FIXME: and sybase ?? */
	if (!curcol->char_conv && IS_TDS7_PLUS(tds) && is_ascii_type(curcol->on_server.column_type))
		curcol->char_conv = tds->char_convs[client2server_chardata];

	if (!curcol->char_conv)
		return;

	curcol->on_server.column_size = curcol->column_size;
	curcol->column_size = determine_adjusted_size(curcol->char_conv, curcol->column_size);

	tdsdump_log(TDS_DBG_INFO1, "adjust_character_column_size:\n"
				   "\tServer charset: %s\n"
				   "\tServer column_size: %d\n"
				   "\tClient charset: %s\n"
				   "\tClient column_size: %d\n", 
				   curcol->char_conv->server_charset.name, 
				   curcol->on_server.column_size, 
				   curcol->char_conv->client_charset.name, 
				   curcol->column_size);
}

/** 
 * Allow for maximum possible size of converted data, 
 * while being careful about integer division truncation. 
 * All character data pass through iconv.  It doesn't matter if the server side 
 * is Unicode or not; even Latin1 text need conversion if,
 * for example, the client is UTF-8.  
 */
static int
determine_adjusted_size(const TDSICONV * char_conv, int size)
{
	if (!char_conv)
		return size;

	size *= char_conv->client_charset.max_bytes_per_char;
	if (size % char_conv->server_charset.min_bytes_per_char)
		size += char_conv->server_charset.min_bytes_per_char;
	size /= char_conv->server_charset.min_bytes_per_char;

	return size;
}
