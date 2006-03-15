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
#include "tds.h"
#include "tdsutil.h"
#ifdef NCBI_FTDS
#if HAVE_ICONV
#include <iconv.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tdsiconvinfo {
	int use_iconv;
#if HAVE_ICONV
     iconv_t cdto;
     iconv_t cdfrom;
#endif
} TDSICONVINFO;

#ifdef __cplusplus
}
#endif

#else
#include "tdsiconv.h"
#endif
#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char  software_version[]   = "$Id$";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};


TDSENVINFO *tds_alloc_env(TDSSOCKET *tds);
void tds_free_env(TDSSOCKET *tds);

TDSDYNAMIC *tds_alloc_dynamic(TDSSOCKET *tds, char *id)
{
int i;

	/* if this is the first dynamic stmt */
	if (!tds->num_dyns) {
		tds->dyns = (TDSDYNAMIC **) malloc(sizeof(TDSDYNAMIC *));
		tds->dyns[0] = (TDSDYNAMIC *) malloc(sizeof(TDSDYNAMIC));
		memset(tds->dyns[0], 0, sizeof(TDSDYNAMIC));
		strncpy(tds->dyns[0]->id, id, TDS_MAX_DYNID_LEN);
		tds->dyns[0]->id[TDS_MAX_DYNID_LEN-1]='\0';
		tds->num_dyns++;
		return tds->dyns[0];
	}
	/* otherwise check to see if id already exists (shouldn't) */
	for (i=0;i<tds->num_dyns;i++) {
		if (!strcmp(tds->dyns[i]->id, id)) {
			/* id already exists! just return it */
			return(tds->dyns[i]);
		}
	}

	/* ok, we have a list and need to add another */
	tds->dyns = (TDSDYNAMIC **)
		realloc(tds->dyns, sizeof(TDSDYNAMIC *) * tds->num_dyns);
	tds->dyns[tds->num_dyns] = (TDSDYNAMIC *) malloc(sizeof(TDSDYNAMIC));
	memset(tds->dyns[tds->num_dyns], 0, sizeof(TDSDYNAMIC));
	strncpy(tds->dyns[tds->num_dyns]->id, id, TDS_MAX_DYNID_LEN);
	tds->dyns[tds->num_dyns]->id[TDS_MAX_DYNID_LEN-1]='\0';
	tds->num_dyns++;

	return tds->dyns[tds->num_dyns-1];
}

TDSINPUTPARAM *tds_add_input_param(TDSDYNAMIC *dyn)
{
TDSINPUTPARAM *param;

	if (!dyn->num_params) {
		param = (TDSINPUTPARAM *) malloc(sizeof(TDSINPUTPARAM));
		memset(param,'\0',sizeof(TDSINPUTPARAM));
		dyn->num_params=1;
		dyn->params = (TDSINPUTPARAM **)
			malloc(sizeof(TDSINPUTPARAM *));
		dyn->params[0] = param;
	} else {
		param = (TDSINPUTPARAM *) malloc(sizeof(TDSINPUTPARAM));
		memset(param,'\0',sizeof(TDSINPUTPARAM));
		dyn->num_params++;
		dyn->params = (TDSINPUTPARAM **)
			realloc(dyn->params,
			sizeof(TDSINPUTPARAM *) * dyn->num_params);
		dyn->params[dyn->num_params-1] = param;
	}
	return param;
}
void tds_free_input_params(TDSDYNAMIC *dyn)
{
int i;

	if (dyn->num_params) {
		for (i=0;i<dyn->num_params;i++) {
			free(dyn->params[i]);
		}
		free(dyn->params);
		dyn->num_params = 0;
	}
}
void tds_free_dynamic(TDSSOCKET *tds)
{
int i;
TDSDYNAMIC *dyn;

	for (i=0;i<tds->num_dyns;i++) {
		dyn = tds->dyns[i];
		tds_free_input_params(dyn);
		free(dyn);
	}
	if (tds->dyns) TDS_ZERO_FREE(tds->dyns);
	tds->num_dyns = 0;

	return;
}
/*
** tds_alloc_param_result() works a bit differently than the other alloc result
** functions.  Output parameters come in individually with no total number
** given in advance, so we simply call this func every time with get a
** TDS_PARAM_TOKEN and let it realloc the columns struct one bigger.
** tds_free_all_results() usually cleans up after us.
*/
TDSPARAMINFO *tds_alloc_param_result(TDSPARAMINFO *old_param)
{
TDSPARAMINFO *param_info;

	if (!old_param) {
		param_info = (TDSPARAMINFO *) malloc(sizeof(TDSPARAMINFO));
		memset(param_info,'\0',sizeof(TDSPARAMINFO));
		param_info->num_cols=1;
		param_info->columns = (TDSCOLINFO **)
			malloc(sizeof(TDSCOLINFO *));
		param_info->columns[0] = (TDSCOLINFO *) malloc(sizeof(TDSCOLINFO));
		memset(param_info->columns[0],'\0',sizeof(TDSCOLINFO));
	} else {
		param_info = old_param;
		param_info->num_cols++;
		param_info->columns = (TDSCOLINFO **)
			realloc(param_info->columns,
			sizeof(TDSCOLINFO *) * param_info->num_cols);
		param_info->columns[param_info->num_cols-1] =
		        (TDSCOLINFO *) malloc(sizeof(TDSCOLINFO));
		memset(param_info->columns[param_info->num_cols-1],'\0',
			sizeof(TDSCOLINFO));
	}
	return param_info;
}
TDSCOMPUTEINFO *tds_alloc_compute_results(int num_cols)
{
/*TDSCOLINFO *curcol;
 */
TDSCOMPUTEINFO *comp_info;
int col;
int null_sz;

	comp_info = (TDSCOMPUTEINFO *) malloc(sizeof(TDSCOMPUTEINFO));
	memset(comp_info,'\0',sizeof(TDSCOMPUTEINFO));
	comp_info->columns = (TDSCOLINFO **)
		malloc(sizeof(TDSCOLINFO *) * num_cols);
	for (col=0;col<num_cols;col++)  {
		comp_info->columns[col] = (TDSCOLINFO *) malloc(sizeof(TDSCOLINFO));
		memset(comp_info->columns[col],'\0',sizeof(TDSCOLINFO));
	}
	comp_info->num_cols = num_cols;

	null_sz = (unsigned) (num_cols + (8 * TDS_ALIGN_SIZE - 1)) / 8u;
	null_sz = null_sz - null_sz % TDS_ALIGN_SIZE;
	/* set the initial row size to the size of the null info */
	comp_info->row_size = comp_info->null_info_size = null_sz;

	return comp_info;
}

TDSRESULTINFO *tds_alloc_results(int num_cols)
{
/*TDSCOLINFO *curcol;
 */
TDSRESULTINFO *res_info;
int col;
int null_sz;

	res_info = (TDSRESULTINFO *) malloc(sizeof(TDSRESULTINFO));
	memset(res_info,'\0',sizeof(TDSRESULTINFO));
	res_info->columns = (TDSCOLINFO **)
		malloc(sizeof(TDSCOLINFO *) * num_cols);
	for (col=0;col<num_cols;col++)  {
		res_info->columns[col] = (TDSCOLINFO *) malloc(sizeof(TDSCOLINFO));
		memset(res_info->columns[col],'\0',sizeof(TDSCOLINFO));
	}
	res_info->num_cols = num_cols;

	null_sz = (unsigned) (num_cols + (8 * TDS_ALIGN_SIZE - 1)) / 8u;
	null_sz = null_sz - null_sz % TDS_ALIGN_SIZE;
	res_info->null_info_size = null_sz;
	/* set the initial row size to the size of the null info */
	res_info->row_size = res_info->null_info_size;

	return res_info;
}

void *tds_alloc_row(TDSRESULTINFO *res_info)
{
void *ptr;

	ptr = (void *) malloc(res_info->row_size);
	memset(ptr,'\0',res_info->row_size);
	return ptr;
}

void tds_free_param_results(TDSPARAMINFO *param_info)
{
int i;

	if(param_info)
	{
		for (i=0;i<param_info->num_cols;i++)
		{
			if(param_info->columns[i])
				TDS_ZERO_FREE(param_info->columns[i]);
		}
		if (param_info->num_cols) TDS_ZERO_FREE(param_info->columns);
		if (param_info->current_row) TDS_ZERO_FREE(param_info->current_row);
		TDS_ZERO_FREE(param_info);
	}
}
void tds_free_compute_results(TDSCOMPUTEINFO *comp_info)
{
int i;

	if(comp_info)
	{
		for (i=0;i<comp_info->num_cols;i++)
		{
			if(comp_info->columns[i])
			TDS_ZERO_FREE(comp_info->columns[i]);
		}
		if (comp_info->num_cols) TDS_ZERO_FREE(comp_info->columns);
		if (comp_info->current_row) TDS_ZERO_FREE(comp_info->current_row);
		TDS_ZERO_FREE(comp_info);
	}
}

void tds_free_results(TDSRESULTINFO *res_info)
{
int i;


	if(res_info)
	{
		if (res_info->current_row) TDS_ZERO_FREE(res_info->current_row);
		for (i=0;i<res_info->num_cols;i++)
		{
			if(res_info->columns && res_info->columns[i])
				tds_free_column(res_info->columns[i]);
		}
		if (res_info->num_cols) TDS_ZERO_FREE(res_info->columns);
		TDS_ZERO_FREE(res_info);
	}

}
void tds_free_all_results(TDSSOCKET *tds)
{
	tds_free_results(tds->res_info);
	tds->res_info = NULL;
	tds_free_param_results(tds->param_info);
	tds->param_info = NULL;
	tds_free_compute_results(tds->comp_info);
	tds->comp_info = NULL;
    tds->has_status= 0;
}
void tds_free_column(TDSCOLINFO *column)
{
	if (column->column_textvalue) TDS_ZERO_FREE(column->column_textvalue);
	TDS_ZERO_FREE(column);
}
TDSCONTEXT *tds_alloc_context()
{
TDSCONTEXT *context;

	context = (TDSCONTEXT *) malloc(sizeof(TDSCONTEXT));
	memset(context, '\0', sizeof(TDSCONTEXT));
	context->locale = tds_get_locale();

	return context;
}
void tds_free_context(TDSCONTEXT *context)
{
	if (context->locale) tds_free_locale(context->locale);
	TDS_ZERO_FREE(context);
}
TDSLOCINFO *tds_alloc_locale()
{
TDSLOCINFO *locale;

	locale = (TDSLOCINFO *) malloc(sizeof(TDSLOCINFO));
	memset(locale, '\0', sizeof(TDSLOCINFO));

	return locale;
}
TDSCONFIGINFO *tds_alloc_config(TDSLOCINFO *locale)
{
TDSCONFIGINFO *config;
char hostname[30];

	config = (TDSCONFIGINFO *) malloc(sizeof(TDSCONFIGINFO));
	memset(config, '\0', sizeof(TDSCONFIGINFO));

	/* fill in all hardcoded defaults */
	config->server_name = strdup(TDS_DEF_SERVER);
	config->major_version = TDS_DEF_MAJOR;
	config->minor_version = TDS_DEF_MINOR;
#ifdef NCBI_FTDS
	config->port[0] = TDS_DEF_PORT;
#else
	config->port = TDS_DEF_PORT;
#endif
	config->block_size = TDS_DEF_BLKSZ;
	if (locale) {
		if (locale->language)
        		config->language = strdup(locale->language);
		else
        		config->language = strdup(TDS_DEF_LANG);
		if (locale->char_set)
        		config->char_set = strdup(locale->char_set);
		else
        		config->char_set = strdup(TDS_DEF_CHARSET);
	}
	config->try_server_login = 1;
	memset(hostname,'\0', 30);
	gethostname(hostname,30);
	hostname[29]='\0'; /* make sure it's truncated */
	config->host_name = strdup(hostname);

	return config;
}
TDSLOGIN *tds_alloc_login()
{
TDSLOGIN *tds_login;
static const unsigned char defaultcaps[] = {0x01,0x07,0x03,109,127,0xFF,0xFF,0xFF,0xFE,0x02,0x07,0x00,0x00,0x0A,104,0x00,0x00,0x00};
char *tdsver;

	tds_login = (TDSLOGIN *) malloc(sizeof(TDSLOGIN));
	memset(tds_login, '\0', sizeof(TDSLOGIN));
	if ((tdsver=getenv("TDSVER"))) {
		if (!strcmp(tdsver,"42") || !strcmp(tdsver,"4.2")) {
			tds_login->major_version=4;
			tds_login->minor_version=2;
		} else if (!strcmp(tdsver,"46") || !strcmp(tdsver,"4.6")) {
			tds_login->major_version=4;
			tds_login->minor_version=6;
		} else if (!strcmp(tdsver,"50") || !strcmp(tdsver,"5.0")) {
			tds_login->major_version=5;
			tds_login->minor_version=0;
		} else if (!strcmp(tdsver,"70") || !strcmp(tdsver,"7.0")) {
			tds_login->major_version=7;
			tds_login->minor_version=0;
		} else if (!strcmp(tdsver,"80") || !strcmp(tdsver,"8.0")) {
			tds_login->major_version=8;
			tds_login->minor_version=0;
		}
		/* else unrecognized...use compile time default above */
	}
	memcpy(tds_login->capabilities,defaultcaps,TDS_MAX_CAPABILITY);
	return tds_login;
}
void tds_free_login(TDSLOGIN *login)
{
	if (login) {
		/* for security reason clear memory */
		memset(login->password,0,sizeof(login->password));
		free(login);
	}
}
TDSSOCKET *tds_alloc_socket(TDSCONTEXT *context, int bufsize)
{
TDSSOCKET *tds_socket;
TDSICONVINFO *iconv;

	tds_socket = (TDSSOCKET *) malloc(sizeof(TDSSOCKET));
	memset(tds_socket, '\0', sizeof(TDSSOCKET));
	tds_socket->tds_ctx = context;
	tds_socket->in_buf_max=0;
	tds_socket->out_buf = (unsigned char *) malloc(bufsize);
	tds_socket->msg_info = (TDSMSGINFO *) malloc(sizeof(TDSMSGINFO));
	memset(tds_socket->msg_info,'\0',sizeof(TDSMSGINFO));
	tds_socket->parent = (char*)NULL;
	tds_socket->env = tds_alloc_env(tds_socket);
	iconv = (TDSICONVINFO *) malloc(sizeof(TDSICONVINFO));
	tds_socket->iconv_info = (void *) iconv;
	memset(tds_socket->iconv_info,'\0',sizeof(TDSICONVINFO));
#if HAVE_ICONV
	iconv->cdfrom = (iconv_t)-1;
	iconv->cdto = (iconv_t)-1;
#endif
	/* Jeff's hack, init to no timeout */
	tds_socket->timeout = 0;
	tds_init_write_buf(tds_socket);
	return tds_socket;
}
TDSSOCKET *tds_realloc_socket(int bufsize)
{
   return NULL; /* XXX */
}
void tds_free_socket(TDSSOCKET *tds)
{
TDSICONVINFO *iconv_info;

	if (tds) {
		tds_free_all_results(tds);
		tds_free_env(tds);
		tds_free_dynamic(tds);
		if (tds->msg_info) {
			tds_free_msg(tds->msg_info); /* dnr */
			TDS_ZERO_FREE(tds->msg_info);
		}
		if (tds->in_buf) TDS_ZERO_FREE(tds->in_buf);
		if (tds->out_buf) TDS_ZERO_FREE(tds->out_buf);
		if (tds->s) CLOSESOCKET(tds->s);
		if (tds->date_fmt) free(tds->date_fmt);
		if (tds->iconv_info) {
			iconv_info = (TDSICONVINFO *) tds->iconv_info;
			if (iconv_info->use_iconv) tds_iconv_close(tds);
			free(tds->iconv_info);
		}
		if (tds->date_fmt) free(tds->date_fmt);
		TDS_ZERO_FREE(tds);
	}
}
void tds_free_locale(TDSLOCINFO *locale)
{
	if (locale->language) free(locale->language);
	if (locale->char_set) free(locale->char_set);
	if (locale->date_fmt) free(locale->date_fmt);
	TDS_ZERO_FREE(locale);
}
void tds_free_config(TDSCONFIGINFO *config)
{
	if (config->server_name) free(config->server_name);
	if (config->host_name) free(config->host_name);
#ifdef NCBI_FTDS
	{int i;
	    for(i= 0; i < NCBI_NUM_SERVERS; i++) {
	        if(config->ip_addr[i]) free(config->ip_addr[i]);
	    }
	}
#else
	if (config->ip_addr) free(config->ip_addr);
#endif
	if (config->language) free(config->language);
	if (config->char_set) free(config->char_set);
	if (config->database) free(config->database);
	if (config->dump_file) free(config->dump_file);
	if (config->default_domain) free(config->default_domain);
	if (config->client_charset) free(config->client_charset);
	/* dnr */
	if (config->host) free(config->host);
	if (config->app_name) free(config->app_name);
	if (config->user_name) free(config->user_name);
	if (config->password) {
		/* for security reason clear memory */
		memset(config->password,0,strlen(config->password));
		free(config->password);
	}
	if (config->library) free(config->library);
	/* !dnr */
	TDS_ZERO_FREE(config);
}
TDSENVINFO *tds_alloc_env(TDSSOCKET *tds)
{
TDSENVINFO *env;

	env = (TDSENVINFO *) malloc(sizeof(TDSENVINFO));
	memset(env,'\0',sizeof(TDSENVINFO));
	env->block_size = 512;

	return env;
}
void tds_free_env(TDSSOCKET *tds)
{
	if (tds->env) {
		if (tds->env->language)
			TDS_ZERO_FREE(tds->env->language);
		if (tds->env->charset)
			TDS_ZERO_FREE(tds->env->charset);
		if (tds->env->database)
			TDS_ZERO_FREE(tds->env->database);
		TDS_ZERO_FREE(tds->env);
	}
}
void tds_free_msg(TDSMSGINFO *msg_info)
{
	if (msg_info) {
		if(msg_info->message) TDS_ZERO_FREE(msg_info->message);
		if(msg_info->server) TDS_ZERO_FREE(msg_info->server);
		if(msg_info->proc_name) TDS_ZERO_FREE(msg_info->proc_name);
		if(msg_info->sql_state) TDS_ZERO_FREE(msg_info->sql_state);
	}
}

