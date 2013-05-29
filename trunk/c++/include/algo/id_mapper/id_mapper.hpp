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
 * Author:  Nathan Bouk
 *
 * File Description:
 *
 */

#ifndef ID_MAPPER_DEF__HPP
#define ID_MAPPER_DEF__HPP


#include <objmgr/scope.hpp>

#include <objects/seq/seq_id_handle.hpp>

#include <objects/genomecoll/genome_collection__.hpp>

BEGIN_NCBI_SCOPE


BEGIN_SCOPE(objects)
    class CScope;
    class CSeq_loc;
    class CSeq_id;
    class CSeq_id_Handle;
    class CGC_Assembly;
    class CGC_AssemblyUnit;
    class CGC_Sequence;
//    class CGC_TypedSeqId;
//    class CGC_TypedSeqId::E_Choice;
END_SCOPE(objects)


class CGencollIdMapper : public CObject
{
public:
    CGencollIdMapper(CRef<objects::CGC_Assembly> SourceAsm);

    struct SIdSpec {
        enum E_Alias {
            e_NotSet,
            e_Public,
            e_Gpipe,
            e_Gi
        };
        objects::CGC_TypedSeqId::E_Choice TypedChoice;
        E_Alias Alias;
        string External;
        string Pattern;
        enum { e_Role_Gpipe_Top = objects::eGC_SequenceRole_top_level+1, // Fake role for Gpipe top
               e_Role_NotSet = 10000  };
        enum { e_Top_NotSet,  // Unknown, don't care
               e_Top_NotTop,  // Specifically not any top
               e_Top_Public,  // Public ID top
               e_Top_Gpipe }; // Gpipe top
        int Role;
        int Top;

        SIdSpec() : TypedChoice(objects::CGC_TypedSeqId::e_not_set), 
                    Alias(e_NotSet), External(""), Pattern(""), 
                    Role(e_Role_NotSet), Top(e_Top_NotSet) { ; }

        string ToString() const {
            string Result;
            Result.reserve(64);
            
            if(TypedChoice == 0)
                Result += "NotSet";
            else if(TypedChoice == 1)
                Result += "GenBank";
            else if(TypedChoice == 2)
                Result += "RefSeq";
            else if(TypedChoice == 3)
                Result += "Private";
            else if(TypedChoice == 4)
                Result += "External";
            Result += ":";
            
            if(Alias == 0)
                Result += "NotSet";
            else if(Alias == 1)
                Result += "Public";
            else if(Alias == 2)
                Result += "Gpipe";
            else if(Alias == 3)
                Result += "Gi";
            Result += ":";
            
            Result += External + ":" + Pattern;
            Result += ":";
            
            if(Role == objects::eGC_SequenceRole_chromosome)
                Result += "CHRO";
            else if(Role == objects::eGC_SequenceRole_scaffold)
                Result += "SCAF";
            else if(Role == objects::eGC_SequenceRole_component)
                Result += "COMP";
            else if(Role != e_Role_NotSet)
                Result += NStr::IntToString(Role);
            Result += ":";
        
            if(Top == e_Top_NotSet)
                Result += "NotSet";
            else if(Top == e_Top_NotTop)
                Result += "NotTop";
            else if(Top == e_Top_Public)
                Result += "Public";
            else if(Top == e_Top_Gpipe)
                Result += "Gpipe";


            return Result;
        }

        bool operator<(const SIdSpec& Other) const {
            return !(TypedChoice < Other.TypedChoice);
        }

        bool operator==(const SIdSpec& Other) const {
            if(TypedChoice != Other.TypedChoice)
                return false;
            else if(Alias != Other.Alias)
                return false;
            else if(External != Other.External)
                return false;
            else if(Pattern != Other.Pattern)
                return false;
            else if(Role != Other.Role)
                return false;
            else if(Top != Other.Top)
                return false;
            return true;
        }
    };

    bool Guess(const objects::CSeq_loc& Loc, SIdSpec& Spec) const;

    // Returning NULL means the given ID is fine, do nothing.
    CRef<objects::CSeq_loc> Map(const objects::CSeq_loc& Loc, const SIdSpec& Spec) const;

    bool CanMeetSpec(const objects::CSeq_loc& Loc, const SIdSpec& Spec) const;

    enum E_Gap {
        e_None = 0,
        e_Spans,
        e_Overlaps,
        e_Contained,
        e_Complicated
    };
    E_Gap IsLocInAGap(const objects::CSeq_loc& Loc) const;

    CConstRef<objects::CGC_Assembly> GetInternalGencoll() const { return m_Assembly; }

private:

    CRef<objects::CGC_Assembly> m_Assembly;
    string m_SourceAsm;

    CRef<objects::CScope> m_Scope;

    typedef map<objects::CSeq_id_Handle, CConstRef<objects::CGC_Sequence> > TIdToSeqMap;
    TIdToSeqMap m_IdToSeqMap;

    typedef map<string, int> TAccToVerMap;
    TAccToVerMap m_AccToVerMap;

    vector<string> m_Chromosomes;

    // All component IDs to the Parent CGC_Sequence
    typedef map<objects::CSeq_id_Handle, CConstRef<objects::CGC_Sequence> > TChildToParentMap;
    TChildToParentMap m_ChildToParentMap;

    void x_Init();

    bool x_NCBI34_Guess(const objects::CSeq_id& Id, SIdSpec& Spec) const;
    CConstRef<objects::CSeq_id> x_NCBI34_Map_IdFix(CConstRef<objects::CSeq_id> SourceId) const;

    void x_RecursiveSeqFix(objects::CGC_Sequence& Seq);
    void x_FillGpipeTopRole(objects::CGC_Sequence& Seq);
    void x_FillChromosomeIds();
    void x_PrioritizeIds();
    void x_PrioritizeIds(objects::CGC_Sequence& Sequence);


    // Fixes locals that should be accessions, and versionless accessions
    CConstRef<objects::CSeq_id> x_FixImperfectId(CConstRef<objects::CSeq_id> Id,
                                                 const SIdSpec& Spec) const;
    CConstRef<objects::CSeq_id> x_ApplyPatternToId(CConstRef<objects::CSeq_id> Id,
                                                 const SIdSpec& Spec) const;
    
    int x_GetRole(const objects::CGC_Sequence& Seq) const;
    bool x_HasTop(const objects::CGC_Sequence& Seq, int Top) const;

    void x_AddSeqToMap(const objects::CSeq_id& Id, CConstRef<objects::CGC_Sequence> Seq);

    void x_BuildSeqMap(const objects::CGC_Assembly& assm);
    void x_BuildSeqMap(const objects::CGC_AssemblyUnit& assm);
    void x_BuildSeqMap(const objects::CGC_Sequence& Seq);

    CConstRef<objects::CSeq_id>
    x_GetIdFromSeqAndSpec(const objects::CGC_Sequence& Seq, const SIdSpec& Spec) const;

    enum { e_No, e_Yes, e_Up, e_Down };

    int x_CanSeqMeetSpec(const objects::CGC_Sequence& Seq, const SIdSpec& Spec, int Level=0) const;

    bool x_MakeSpecForSeq(const objects::CSeq_id& Id, const objects::CGC_Sequence& Seq, SIdSpec& Spec) const;

    CConstRef<objects::CGC_Sequence> x_FindChromosomeSequence(const objects::CSeq_id& Id, const SIdSpec& Spec) const;

    CConstRef<objects::CGC_Sequence>
    x_FindParentSequence(const objects::CSeq_id& Id, const objects::CGC_Assembly& Assembly, int Depth = 0) const;

    bool x_IsParentSequence(const objects::CSeq_id& Id, const objects::CGC_Sequence& Parent) const;


    CRef<objects::CSeq_loc> x_Map_OneToOne(const objects::CSeq_loc& SourceLoc,
                                           const objects::CGC_Sequence& Seq,
                                           const SIdSpec& Spec) const;

    CRef<objects::CSeq_loc> x_Map_Up(const objects::CSeq_loc& SourceLoc,
                                     const objects::CGC_Sequence& Seq,
                                     const SIdSpec& Spec) const;
    
    CRef<objects::CSeq_loc> x_Map_Down(const objects::CSeq_loc& SourceLoc,
                                       const objects::CGC_Sequence& Seq,
                                       const SIdSpec& Spec) const;


    //bool x_IsLocInGap(const objects::CSeq_loc& Loc) const;
    E_Gap x_Merge_E_Gaps(E_Gap First, E_Gap Second) const;
    E_Gap x_IsLoc_Int_InAGap(const objects::CSeq_interval& Int) const;
};


END_NCBI_SCOPE

#endif // ID_MAPPER_DEF__HPP

