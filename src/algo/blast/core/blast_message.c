/* 
**************************************************************************
*                                                                         *
*                             COPYRIGHT NOTICE                            *
*                                                                         *
* This software/database is categorized as "United States Government      *
* Work" under the terms of the United States Copyright Act.  It was       *
* produced as part of the author's official duties as a Government        *
* employee and thus can not be copyrighted.  This software/database is    *
* freely available to the public for use without a copyright notice.      *
* Restrictions can not be placed on its present or future use.            *
*                                                                         *
* Although all reasonable efforts have been taken to ensure the accuracy  *
* and reliability of the software and data, the National Library of       *
* Medicine (NLM) and the U.S. Government do not and can not warrant the   *
* performance or results that may be obtained by using this software,     *
* data, or derivative works thereof.  The NLM and the U.S. Government     *
* disclaim any and all warranties, expressed or implied, as to the        *
* performance, merchantability or fitness for any particular purpose or   *
* use.                                                                    *
*                                                                         *
* In any work or product derived from this material, proper attribution   *
* of the author(s) as the source of the software or data would be         *
* appreciated.                                                            *
*                                                                         *
**************************************************************************
 *
 * $Log$
 * Revision 1.2  2003/05/15 22:01:22  coulouri
 * add rcsid string to sources
 *
 * Revision 1.1  2003/03/31 18:22:30  camacho
 * Moved from parent directory
 *
 * Revision 1.2  2003/03/04 14:09:14  madden
 * Fix prototype problem
 *
 * Revision 1.1  2003/02/13 21:38:54  madden
 * Files for messaging warnings etc.
 *
 *
*/

static char const rcsid[] = "$Id$";

#include <blast_message.h>

/*
	Deallocates message memory.
*/

Blast_MessagePtr 
Blast_MessageFree(Blast_MessagePtr blast_msg)
{
	if (blast_msg == NULL)
		return NULL;

	blast_msg->message = MemFree(blast_msg->message);

	return MemFree(blast_msg);
}

/*
	Writes a message to a structure.
*/

Int2 
Blast_MessageWrite(Blast_MessagePtr *blast_msg, Int4 severity, Int4 code,
	Int4 subcode, const Char *message)
{
	if (blast_msg == NULL)
		return 1;

	*blast_msg = MemNew(sizeof(Blast_Message));

	(*blast_msg)->severity = severity;
	(*blast_msg)->code = code;
	(*blast_msg)->subcode = subcode;
	(*blast_msg)->message = StringSave(message);

	return 0;
}

/*
	Print a message with ErrPostEx
*/

Int2 
Blast_MessagePost(Blast_MessagePtr blast_msg)
{
	if (blast_msg == NULL)
		return 1;

	ErrPostEx(blast_msg->severity, blast_msg->code, blast_msg->subcode, "%s", blast_msg->message);

	return 0;
}
