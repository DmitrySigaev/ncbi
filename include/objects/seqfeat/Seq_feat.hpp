/* $Id$
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
 * Author:  .......
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the ASN data definition file
 *   'seqfeat.asn'.
 */

#ifndef OBJECTS_SEQFEAT_SEQ_FEAT_HPP
#define OBJECTS_SEQFEAT_SEQ_FEAT_HPP


// generated includes
#include <objects/seqfeat/Seq_feat_.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Prot_ref.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class NCBI_SEQFEAT_EXPORT CSeq_feat : public CSeq_feat_Base
{
    typedef CSeq_feat_Base Tparent;
public:
    // constructor
    CSeq_feat(void);
    // destructor
    ~CSeq_feat(void);
    
    //
    // See related function in util/feature.hpp
    //
    //   void GetLabel (const CSeq_feat&, string*, ELabelType, CScope*)
    //

    // get gene (if present) from Seq-feat.xref list
    const CGene_ref* GetGeneXref(void) const;

    // get protein (if present) from Seq-feat.xref list
    const CProt_ref* GetProtXref(void) const;

private:
    // Prohibit copy constructor and assignment operator
    CSeq_feat(const CSeq_feat& value);
    CSeq_feat& operator=(const CSeq_feat& value);        
};

// Corresponds to SortFeatItemListByPos from the C toolkit
NCBI_SEQFEAT_EXPORT bool operator< (const CSeq_feat& f1, const CSeq_feat& f2);

/////////////////// CSeq_feat inline methods

// constructor
inline
CSeq_feat::CSeq_feat(void)
{
}


/////////////////// end of CSeq_feat inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_SEQFEAT_SEQ_FEAT_HPP
/* Original file checksum: lines: 90, chars: 2388, CRC32: c285198b */

/*
* ===========================================================================
* $Log$
* Revision 1.7  2002/12/26 12:43:27  dicuccio
* Added Win32 export specifiers
*
* Revision 1.6  2002/12/19 22:12:03  kans
* include Gene_ref.hpp and Prot_ref.hpp, move log to bottom of file
*
* Revision 1.5  2002/12/19 21:31:09  kans
* added GetGeneXref and GetProtXref
*
* Revision 1.4  2002/06/07 11:59:10  clausen
* Added related function comment
*
* Revision 1.3  2002/06/06 20:56:30  clausen
* Moved GetLabel to objects/util/feature.hpp
*
* Revision 1.2  2002/01/10 19:55:01  clausen
* Added GetLabel
*
* Revision 1.1  2001/10/30 20:25:57  ucko
* Implement feature labels/keys, subtypes, and sorting
*
*
* ===========================================================================
*/
