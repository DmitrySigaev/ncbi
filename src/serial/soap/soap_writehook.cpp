
/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Author: Andrei Gourianov
*
* File Description:
*   Write SOAP message contents
*   
*/

#include <ncbi_pch.hpp>
#include <serial/objostr.hpp>
#include "soap_envelope.hpp"
#include "soap_writehook.hpp"


BEGIN_NCBI_SCOPE

CSoapWriteHook::CSoapWriteHook(
    const vector< CConstRef<CSerialObject> >& content)
    : m_Content(content)
{
}


void CSoapWriteHook::WriteObject(CObjectOStream& out,
                                 const CConstObjectInfo& /*object*/)
{
    vector< CConstRef<CSerialObject> >::const_iterator i;
    for ( i = m_Content.begin(); i != m_Content.end(); ++i) {
        TTypeInfo type = (*i)->GetThisTypeInfo();
        if (!type->HasNamespaceName()) {
            const CAnyContentObject* any =
                dynamic_cast<const CAnyContentObject*>((*i).GetPointer());
            if (!any || (any && any->GetNamespaceName().empty())) {
                out.ThrowError(CObjectOStream::fInvalidData,
                    "SOAP content object must have a namespace name");
            }
        }
        out.WriteObject(*i, (*i)->GetThisTypeInfo());
    }
}


END_NCBI_SCOPE



/* --------------------------------------------------------------------------
* $Log$
* Revision 1.4  2004/06/22 15:01:20  gouriano
* Corrected checking namespace name of AnyContentObjects
*
* Revision 1.3  2004/05/17 21:03:24  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.2  2004/01/22 20:43:48  gouriano
* Added check for non-empty namespace
*
* Revision 1.1  2003/09/22 21:00:04  gouriano
* Initial revision
*
*
* ===========================================================================
*/
