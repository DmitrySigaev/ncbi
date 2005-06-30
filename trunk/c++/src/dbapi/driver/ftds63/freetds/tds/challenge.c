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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include "tds.h"
#include "md4.h"
#include "des.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id$";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

/**
 * \ingroup libtds
 * \defgroup auth Authentication
 * Functions for handling authentication.
 */

/**
 * \addtogroup auth
 *  \@{ 
 */

/*
 * The following code is based on some psuedo-C code from ronald@innovation.ch
 */

static void tds_encrypt_answer(unsigned char *hash, const unsigned char *challenge, unsigned char *answer);
static void tds_convert_key(unsigned char *key_56, DES_KEY * ks);

/**
 * Crypt a given password using schema required for NTLMv1 authentication
 * @param passwd clear text domain password
 * @param challenge challenge data given by server
 * @param answer buffer where to store crypted password
 */
void
tds_answer_challenge(const char *passwd, const unsigned char *challenge, TDSANSWER * answer)
{
#define MAX_PW_SZ 14
	int len;
	int i;
	static const des_cblock magic = { 0x4B, 0x47, 0x53, 0x21, 0x40, 0x23, 0x24, 0x25 };
	DES_KEY ks;
	unsigned char hash[24];
	unsigned char passwd_up[MAX_PW_SZ];
	unsigned char nt_pw[256];
	MD4_CTX context;

	memset(answer, 0, sizeof(TDSANSWER));

	/* convert password to upper and pad to 14 chars */
	memset(passwd_up, 0, MAX_PW_SZ);
	len = strlen(passwd);
	if (len > MAX_PW_SZ)
		len = MAX_PW_SZ;
	for (i = 0; i < len; i++)
		passwd_up[i] = toupper((unsigned char) passwd[i]);

	/* hash the first 7 characters */
	tds_convert_key(passwd_up, &ks);
	tds_des_ecb_encrypt(&magic, sizeof(magic), &ks, (hash + 0));

	/* hash the second 7 characters */
	tds_convert_key(passwd_up + 7, &ks);
	tds_des_ecb_encrypt(&magic, sizeof(magic), &ks, (hash + 8));

	memset(hash + 16, 0, 5);

	tds_encrypt_answer(hash, challenge, answer->lm_resp);

	/* NT resp */
	len = strlen(passwd);
	if (len > 128)
		len = 128;
	for (i = 0; i < len; ++i) {
		nt_pw[2 * i] = passwd[i];
		nt_pw[2 * i + 1] = 0;
	}

	MD4Init(&context);
	MD4Update(&context, nt_pw, len * 2);
	MD4Final(&context, hash);

	memset(hash + 16, 0, 5);
	tds_encrypt_answer(hash, challenge, answer->nt_resp);

	/* with security is best be pedantic */
	memset(&ks, 0, sizeof(ks));
	memset(hash, 0, sizeof(hash));
	memset(passwd_up, 0, sizeof(passwd_up));
	memset(nt_pw, 0, sizeof(nt_pw));
	memset(&context, 0, sizeof(context));
}


/*
* takes a 21 byte array and treats it as 3 56-bit DES keys. The
* 8 byte plaintext is encrypted with each key and the resulting 24
* bytes are stored in the results array.
*/
static void
tds_encrypt_answer(unsigned char *hash, const unsigned char *challenge, unsigned char *answer)
{
	DES_KEY ks;

	tds_convert_key(hash, &ks);
	tds_des_ecb_encrypt(challenge, 8, &ks, answer);

	tds_convert_key(&hash[7], &ks);
	tds_des_ecb_encrypt(challenge, 8, &ks, &answer[8]);

	tds_convert_key(&hash[14], &ks);
	tds_des_ecb_encrypt(challenge, 8, &ks, &answer[16]);

	memset(&ks, 0, sizeof(ks));
}


/*
* turns a 56 bit key into the 64 bit, odd parity key and sets the key.
* The key schedule ks is also set.
*/
static void
tds_convert_key(unsigned char *key_56, DES_KEY * ks)
{
	des_cblock key;

	key[0] = key_56[0];
	key[1] = ((key_56[0] << 7) & 0xFF) | (key_56[1] >> 1);
	key[2] = ((key_56[1] << 6) & 0xFF) | (key_56[2] >> 2);
	key[3] = ((key_56[2] << 5) & 0xFF) | (key_56[3] >> 3);
	key[4] = ((key_56[3] << 4) & 0xFF) | (key_56[4] >> 4);
	key[5] = ((key_56[4] << 3) & 0xFF) | (key_56[5] >> 5);
	key[6] = ((key_56[5] << 2) & 0xFF) | (key_56[6] >> 6);
	key[7] = (key_56[6] << 1) & 0xFF;

	tds_des_set_odd_parity(key);
	tds_des_set_key(ks, key, sizeof(key));

	memset(&key, 0, sizeof(key));
}


/** \@} */
