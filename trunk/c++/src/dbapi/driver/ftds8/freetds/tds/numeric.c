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
#include <tds.h>
#include <stdio.h>
#ifdef DMALLOC
#include <dmalloc.h>
#endif

/* 
** these routines use arrays of unsigned char to handle arbitrary
** precision numbers.  All-in-all it's probably pretty slow, but it
** does work. I just heard of a GNU lib for arb. precision math, so
** that might be an option in the future.
*/

static int multiply_byte(unsigned char *product, int num, unsigned char *multiplier);
static int do_carry(unsigned char *product);
static char *array_to_string(unsigned char *array, int scale, char *s);

/*
** The following little table is indexed by precision-1 and will
** tell us the number of bytes required to store the specified
** precision (with the sign).
** Support precision up to 77 digits
*/
const int g__numeric_bytes_per_prec[] =
{
 -1, 2, 2, 3, 3, 4, 4, 4, 5, 5, 
  6, 6, 6, 7, 7, 8, 8, 9, 9, 9,
  10, 10, 11, 11, 11, 12, 12, 13, 13, 14, 
  14, 14, 15, 15, 16, 16, 16, 17, 17, 18, 
  18, 19, 19, 19, 20, 20, 21, 21, 21, 22, 
  22, 23, 23, 24, 24, 24, 25, 25, 26, 26, 
  26, 27, 27, 28, 28, 28, 29, 29, 30, 30,
  31, 31, 31, 32, 32, 33, 33, 33
};

/*
** money is a special case of numeric really...that why its here
*/
char *tds_money_to_string(TDS_MONEY *money, char *s)
{
unsigned char multiplier[MAXPRECISION], temp[MAXPRECISION];
unsigned char product[MAXPRECISION];
unsigned char *number;
#ifndef WORDS_BIGENDIAN
unsigned char reorder[8];
#endif
int num_bytes = 8;
int i;
int pos;
int neg=0;

	memset(multiplier,0,MAXPRECISION);
	memset(product,0,MAXPRECISION);
	multiplier[0]=1;

#ifdef WORDS_BIGENDIAN
	/* big endian makes things easy */
	number = (unsigned char *) money;
#else
	/* money is two 32 bit ints and thus is out of order on 
	** little endian machines. Proof of the superiority of 
	** big endian design. ;)
	*/
	number = (unsigned char *) money;
	for (i=0;i<4;i++)
		reorder[3-i] = number[i];
	for (i=4;i<8;i++)
		reorder[7-i+4] = number[i];
	number = reorder;
#endif

	if (number[0] & 0x80) {
		/* negative number -- preform two's complement */
		neg = 1;
		for (i=0;i<8;i++) {
			number[i] = ~number[i];
		}
		for (i=7; i>=0; i--) {
			number[i] += 1;
			if (number[i]!=0) break;
		}
	}
	for (pos=num_bytes-1;pos>=0;pos--) {
		multiply_byte(product, number[pos], multiplier);

		memcpy(temp, multiplier, MAXPRECISION);
		memset(multiplier,0,MAXPRECISION);
		multiply_byte(multiplier, 256, temp);
	}
	if (neg) {
		s[0]='-';
		array_to_string(product, 4, &s[1]);
	} else {
		array_to_string(product, 4, s);
	}
	return s;
}
char *tds_numeric_to_string(TDS_NUMERIC *numeric, char *s)
{
unsigned char multiplier[MAXPRECISION], temp[MAXPRECISION];
unsigned char product[MAXPRECISION];
unsigned char *number;
int num_bytes;
int pos;

	memset(multiplier,0,MAXPRECISION);
	memset(product,0,MAXPRECISION);
	multiplier[0]=1;
	number = numeric->array;
	num_bytes = g__numeric_bytes_per_prec[numeric->precision];

	if (numeric->array[0] == 1)
		*s++ = '-';

	for (pos=num_bytes-1;pos>0;pos--) {
		multiply_byte(product, number[pos], multiplier);

		memcpy(temp, multiplier, MAXPRECISION);
		memset(multiplier,0,MAXPRECISION);
		multiply_byte(multiplier, 256, temp);
	}
	array_to_string(product, numeric->scale, s);
	return s;
}
static int multiply_byte(unsigned char *product, int num, unsigned char *multiplier)
{
unsigned char number[3];
int i, top, j, start;

	number[0]=num%10;
	number[1]=(num/10)%10;
	number[2]=(num/100)%10;

	for (top=MAXPRECISION-1;top>=0 && !multiplier[top];top--);
	start=0;
	for (i=0;i<=top;i++) {
		for (j=0;j<3;j++) {
			product[j+start]+=multiplier[i]*number[j];
		}
		do_carry(product);
		start++;
	}
	return 0;
}
static int do_carry(unsigned char *product)
{
int j;

	for (j=0;j<MAXPRECISION;j++) {
		if (product[j]>9) {
			product[j+1]+=product[j]/10;
			product[j]=product[j]%10;
		}
	}
	return 0;
}
static char *array_to_string(unsigned char *array, int scale, char *s)
{
int top, i, j;
	
	for (top=MAXPRECISION-1;top>=0 && top>scale && !array[top];top--);

	if (top == -1)
	{
		s[0] = '0';
		s[1] = '\0';
		return s;
	}

	j=0;
	for (i=top;i>=0;i--) {
		if (top+1-j == scale) s[j++]='.';
		s[j++]=array[i]+'0';
	}
	s[j]='\0';
	return s;
}
