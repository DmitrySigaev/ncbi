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
 * Authors:  Frank Ludwig
 *
 * File Description:  Write gff file
 *
 */

#ifndef OBJTOOLS_WRITERS___GFF_WRITER__HPP
#define OBJTOOLS_READERS___GFF_WRITER__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objtools/readers/gff3_data.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ----------------------------------------------------------------------------
class CGff3RecordSet
//  ----------------------------------------------------------------------------
{
public:
    typedef vector< CGff3Record* > TRecords;
    typedef TRecords::const_iterator TCit;
    typedef TRecords::iterator TIt;

public:
    CGff3RecordSet() {};
    ~CGff3RecordSet() {
        for ( TRecords::iterator it = begin(); it != end(); ++it ) {
            delete *it;
        }
    };

    void AddRecord(
        CGff3Record* pRecord ) { m_Set.push_back( pRecord ); };

    const TRecords& Set() const { return m_Set; };
    TCit begin() const { return Set().begin(); };
    TCit end() const { return Set().end(); }; 
    TIt begin() { return m_Set.begin(); };
    TIt end() { return m_Set.end(); };

protected:
    TRecords m_Set;
};


//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CGffWriter
//  ============================================================================
{
public:
    typedef enum {
        fNormal =       0,
        fSoQuirks =     1<<0,
    } TFlags;
    
public:
    CGffWriter(
        CNcbiOstream&,
        TFlags = fNormal );
    ~CGffWriter();

    bool WriteAnnot( const CSeq_annot& );

protected:
    bool x_WriteHeader();
    bool x_WriteAnnotFTable( 
        const CSeq_annot& );
    bool x_WriteAnnotAlign( 
        const CSeq_annot& );
    bool x_WriteRecord( 
        const CGff3Record& );
    bool x_WriteRecords( 
        const CGff3RecordSet& );
    bool x_AssignObject( 
        const CSeq_annot&,
        const CSeq_feat&,        
        CGff3RecordSet& );

    string x_GffId(
        const CGff3Record& ) const;
    string x_GffSource(
        const CGff3Record& ) const;
    string x_GffType(
        const CGff3Record& ) const;
    string x_GffSeqStart(
        const CGff3Record& ) const;
    string x_GffSeqStop(
        const CGff3Record& ) const;
    string x_GffScore(
        const CGff3Record& ) const;
    string x_GffStrand(
        const CGff3Record& ) const;
    string x_GffPhase(
        const CGff3Record& ) const;
    string x_GffAttributes( 
        const CGff3Record& ) const;

    CNcbiOstream& m_Os;
    TFlags m_uFlags;

};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___GFF_WRITER__HPP
