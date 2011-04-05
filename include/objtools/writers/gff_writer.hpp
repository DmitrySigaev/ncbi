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

#ifndef OBJTOOLS_READERS___GFF_WRITER__HPP
#define OBJTOOLS_READERS___GFF_WRITER__HPP

#include <corelib/ncbistd.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objtools/writers/gff2_write_data.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ----------------------------------------------------------------------------
class CGff2WriteRecordSet
//  ----------------------------------------------------------------------------
{
public:
    typedef vector< CGff2WriteRecord* > TRecords;
    typedef TRecords::const_iterator TCit;
    typedef TRecords::iterator TIt;

public:
    CGff2WriteRecordSet() {};
    ~CGff2WriteRecordSet() {
        for ( TRecords::iterator it = begin(); it != end(); ++it ) {
//            delete *it;
        }
    };
 
    void AddRecord(
        CGff2WriteRecord* pRecord );

	void AddOrMergeRecord( CGff2WriteRecord* );

    const TRecords& Set() const { return m_Set; };
    TCit begin() const { return Set().begin(); };
    TCit end() const { return Set().end(); }; 
    TIt begin() { return m_Set.begin(); };
    TIt end() { return m_Set.end(); };

protected:
    TRecords m_Set;

	struct PGff2WriteRecordPtrLess {
    	bool operator()(const CGff2WriteRecord* x, const CGff2WriteRecord* y) const; 
	};

	typedef map< const CGff2WriteRecord*, CGff2WriteRecord*, 
				 PGff2WriteRecordPtrLess > TMergeMap;
	TMergeMap m_MergeMap;
};


//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CGff2Writer
//  ============================================================================
{
public:
    typedef enum {
        fNormal =       0,
        fNoHeader =     1<<0,
        fSoQuirks =     1<<15,
    } TFlags;
    
public:
    CGff2Writer(
        CScope&,
        CNcbiOstream&,
        TFlags = fNormal );
    virtual ~CGff2Writer();

    bool WriteAnnot( 
        const CSeq_annot& );
    bool WriteAlign( 
        const CSeq_align& );

protected:
    virtual bool x_WriteHeader();
    virtual bool x_WriteFooter();
    bool x_WriteAnnotFTable( 
        const CSeq_annot& );
    virtual bool x_WriteRecord( 
        const CGff2WriteRecord* );
    bool x_WriteRecords( 
        const CGff2WriteRecordSet& );
    bool x_WriteBrowserLine(
        const CRef< CUser_object > );
    bool x_WriteTrackLine(
        const CRef< CUser_object > );

    virtual bool x_AssignObject( 
        feature::CFeatTree&,
        CMappedFeat,        
        CGff2WriteRecordSet& );

    virtual void x_PriorityProcess(
        const string&,
        map<string, string >&,
        string& ) const;

    CRef< CUser_object > x_GetDescriptor(
        const CSeq_annot&,
        const string& ) const;

    CRef< CUser_object > x_GetDescriptor(
        const CSeq_align&,
        const string& ) const;

    static bool x_NeedsQuoting(
        const string& );

    virtual SAnnotSelector x_GetAnnotSelector();

    virtual CGff2WriteRecord* x_CreateRecord(
        feature::CFeatTree& );

    virtual CGff2WriteRecord* x_CloneRecord(
        const CGff2WriteRecord& );

    CScope& m_Scope;
    CNcbiOstream& m_Os;
    TFlags m_uFlags;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___GFF_WRITER__HPP
