#ifndef ALGO_SEQUENCE___GENE_MODEL__HPP
#define ALGO_SEQUENCE___GENE_MODEL__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
    class CScope;
    class CSeq_align;
    class CSeq_annot;
    class CBioseq_set;
END_SCOPE(objects)


class NCBI_XALGOSEQ_EXPORT CGeneModel
{
public:
    enum EGeneModelCreateFlags {
        fCreateGene         = 0x01,
        fCreateMrna         = 0x02,
        fCreateCdregion     = 0x04,
        fPromoteAllFeatures = 0x08,
        fPropagateOnly      = 0x10,
        fForceTranslateCds  = 0x20,

        fDefaults = fCreateGene | fCreateMrna | fCreateCdregion
    };
    typedef int TGeneModelCreateFlags;


    /// Create a gene model from an alignment
    /// this will optionally promote all features through the alignment
    static void CreateGeneModelFromAlign(const objects::CSeq_align& align,
                                         objects::CScope& scope,
                                         objects::CSeq_annot& annot,
                                         objects::CBioseq_set& translated_proteins,
                                         TGeneModelCreateFlags flags = fDefaults);
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2006/04/24 13:54:38  dicuccio
 * Added new options to create mRNA feature optionally instead ofalways and to
 * force translation of the CDS feature instead of using a supplied product
 *
 * Revision 1.2  2005/04/15 01:47:51  ucko
 * Remove illegal semicolon after END_SCOPE macro.
 *
 * Revision 1.1  2005/04/14 16:48:38  dicuccio
 * Initial revision of CGeneModel
 *
 * ===========================================================================
 */

#endif  // ALGO_SEQUENCE___GENE_MODEL__HPP
