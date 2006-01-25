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
 * Author:  Maxim Didenko
 *
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistre.hpp>

#include <objmgr/edits_db_saver.hpp>
#include <objmgr/edits_db_engine.hpp>
#include <objmgr/blob_id.hpp>
#include <objmgr/bio_object_id.hpp>

#include <objmgr/seq_feat_handle.hpp>
#include <objmgr/seq_align_handle.hpp>
#include <objmgr/seq_graph_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>

#include <objects/seq/seq_id_handle.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objects/seqedit/SeqEdit_Id.hpp>
#include <objects/seqedit/SeqEdit_Cmd.hpp>
#include <objects/seqedit/SeqEdit_Cmd_AddId.hpp>
#include <objects/seqedit/SeqEdit_Cmd_RemoveId.hpp>
#include <objects/seqedit/SeqEdit_Cmd_ResetIds.hpp>
#include <objects/seqedit/SeqEdit_Cmd_ChangeSeqAttr.hpp>
#include <objects/seqedit/SeqEdit_Cmd_ResetSeqAttr.hpp>
#include <objects/seqedit/SeqEdit_Cmd_ChangeSetAttr.hpp>
#include <objects/seqedit/SeqEdit_Cmd_ResetSetAttr.hpp>
#include <objects/seqedit/SeqEdit_Cmd_AddDescr.hpp>
#include <objects/seqedit/SeqEdit_Cmd_SetDescr.hpp>
#include <objects/seqedit/SeqEdit_Cmd_ResetDescr.hpp>
#include <objects/seqedit/SeqEdit_Cmd_AddDesc.hpp>
#include <objects/seqedit/SeqEdit_Cmd_RemoveDesc.hpp>
#include <objects/seqedit/SeqEdit_Cmd_AttachSeq.hpp>
#include <objects/seqedit/SeqEdit_Cmd_AttachSet.hpp>
#include <objects/seqedit/SeqEdit_Cmd_ResetSeqEntry.hpp>
#include <objects/seqedit/SeqEdit_Cmd_AttachSeqEntry.hpp>
#include <objects/seqedit/SeqEdit_Cmd_RemoveSeqEntry.hpp>
#include <objects/seqedit/SeqEdit_Cmd_AttachAnnot.hpp>
#include <objects/seqedit/SeqEdit_Cmd_RemoveAnnot.hpp>
#include <objects/seqedit/SeqEdit_Cmd_AddAnnot.hpp>
#include <objects/seqedit/SeqEdit_Cmd_ReplaceAnnot.hpp>

#include <algorithm>
#include <functional>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

static CRef<CSeqEdit_Id> s_Convert(const CBioObjectId& id)
{
    CRef<CSeqEdit_Id> ret(new CSeqEdit_Id);
    switch( id.GetType() ) {
    case CBioObjectId::eSeqId :
        {
            CSeq_id& seq_id = const_cast<CSeq_id&>(*id.GetSeqId().GetSeqId());
            ret->SetBioseq_id(seq_id);
        }
        break;
    case CBioObjectId::eSetId :
        ret->SetBioseqset_id(id.GetSetId());
        break;
    case CBioObjectId::eUniqNumber :
        ret->SetUnique_num(id.GetUniqNumber());
        break;
    default:
        _ASSERT(0);
    }
    return ret;
}

CEditsSaver::CEditsSaver(IEditsDBEngine& engine)
    : m_Engine(&engine) 
{
}

CEditsSaver::~CEditsSaver()
{
}

void CEditsSaver::BeginTransaction()
{
    _ASSERT( m_Commands.empty() );
    m_Commands.clear();
    m_ChangedIds.clear();
}
void CEditsSaver::CommitTransaction()
{    
    if (m_Commands.empty())
        return;

    ITERATE(TCommands, it, m_Commands) {
        GetDBEngine().SaveCommand(**it);
    }
    GetDBEngine().NotifyIdsChanged(m_ChangedIds);

    m_Commands.clear();
    m_ChangedIds.clear();
}
void CEditsSaver::RollbackTransaction()
{
    m_Commands.clear();
    m_ChangedIds.clear();
}

template<int type> 
struct STypeChooser;

#define DEFCHOOSER(type, name)         \
template<> \
struct STypeChooser<CSeqEdit_Cmd::e_##type> { \
    typedef CSeqEdit_Cmd_##name TCommand; \
    static inline TCommand& GetCommand(CSeqEdit_Cmd& cmd) \
    { return cmd.Set##type(); } \
}

DEFCHOOSER(Add_descr,       AddDescr);
DEFCHOOSER(Set_descr,       SetDescr);
DEFCHOOSER(Reset_descr,     ResetDescr);
DEFCHOOSER(Add_desc,        AddDesc);
DEFCHOOSER(Remove_desc,     RemoveDesc);
DEFCHOOSER(Add_id,          AddId);
DEFCHOOSER(Remove_id,       RemoveId);
DEFCHOOSER(Reset_ids,       ResetIds);
DEFCHOOSER(Change_seqattr,  ChangeSeqAttr);
DEFCHOOSER(Reset_seqattr,   ResetSeqAttr);
DEFCHOOSER(Change_setattr,  ChangeSetAttr);
DEFCHOOSER(Reset_setattr,   ResetSetAttr);
DEFCHOOSER(Attach_seq,      AttachSeq);
DEFCHOOSER(Attach_set,      AttachSet);
DEFCHOOSER(Reset_seqentry,  ResetSeqEntry);
DEFCHOOSER(Attach_seqentry, AttachSeqEntry);
DEFCHOOSER(Remove_seqentry, RemoveSeqEntry);
DEFCHOOSER(Attach_annot,    AttachAnnot);
DEFCHOOSER(Remove_annot,    RemoveAnnot);
DEFCHOOSER(Add_annot,       AddAnnot);
DEFCHOOSER(Replace_annot,   ReplaceAnnot);

#undef DEFCHOOSER

template<int type> 
struct SCmdCreator 
{
    typedef STypeChooser<type> TTypeChooser;
    typedef typename TTypeChooser::TCommand TCommand;

    template<typename THandle>
    static inline TCommand& CreateCmd(const THandle& handle, 
                                      CRef<CSeqEdit_Cmd>& holder) 
    {
        holder.Reset(new CSeqEdit_Cmd(handle.GetTSE_Handle().
                                      GetBlobId().ToString()));
        TCommand& cmd = TTypeChooser::GetCommand(*holder);
        cmd.SetId(*s_Convert(handle.GetBioObjectId()));
        return cmd;
    }
    template<typename THandle>
    static inline TCommand& CreateCmd(const THandle& handle, 
                                      const CBioObjectId& id,
                                      CRef<CSeqEdit_Cmd>& holder) 
    {
        holder.Reset(new CSeqEdit_Cmd(handle.GetTSE_Handle().
                                      GetBlobId().ToString()));
        TCommand& cmd = TTypeChooser::GetCommand(*holder);
        cmd.SetId(*s_Convert(id));
        return cmd;
    }

};

template<typename THandle>
static inline void s_AddDescr(const THandle& handle, const CSeq_descr& descr,
                              CEditsSaver::TCommands& cmds)
{
    CRef<CSeqEdit_Cmd> cmd;
    SCmdCreator<CSeqEdit_Cmd::e_Add_descr>::CreateCmd(handle,cmd)
        .SetAdd_descr(const_cast<CSeq_descr&>(descr));
    cmds.push_back(cmd); 
}

void CEditsSaver::AddDescr(const CBioseq_Handle& handle, 
                           const CSeq_descr& descr, ECallMode mode)
{
    s_AddDescr(handle, descr, m_Commands);
}

void CEditsSaver::AddDescr(const CBioseq_set_Handle& handle, 
                           const CSeq_descr& descr, ECallMode mode)
{
    s_AddDescr(handle, descr, m_Commands);
}

template<typename THandle>
static inline void s_SetDescr(const THandle& handle, const CSeq_descr& descr,
                              CEditsSaver::TCommands& cmds)
{
    CRef<CSeqEdit_Cmd> cmd;
    SCmdCreator<CSeqEdit_Cmd::e_Set_descr>::CreateCmd(handle,cmd)
        .SetSet_descr(const_cast<CSeq_descr&>(descr));
    cmds.push_back(cmd); 
}
void CEditsSaver::SetDescr(const CBioseq_Handle& handle, 
                           const CSeq_descr& descr, ECallMode mode)
{
    s_SetDescr(handle, descr, m_Commands);
}
void CEditsSaver::SetDescr(const CBioseq_set_Handle& handle, 
                           const CSeq_descr& descr, ECallMode mode)
{
    s_SetDescr(handle, descr, m_Commands);
}

template<typename THandle>
static inline void s_ResetDescr(const THandle& handle,
                                CEditsSaver::TCommands& cmds)
{
    CRef<CSeqEdit_Cmd> cmd;
    SCmdCreator<CSeqEdit_Cmd::e_Reset_descr>::CreateCmd(handle,cmd);
    cmds.push_back(cmd); 
}
void CEditsSaver::ResetDescr(const CBioseq_Handle& handle, 
                             ECallMode mode)
{
    s_ResetDescr(handle, m_Commands);
}
void CEditsSaver::ResetDescr(const CBioseq_set_Handle& handle, 
                             ECallMode mode)
{
    s_ResetDescr(handle, m_Commands);
}

template<typename THandle>
static inline void s_AddDesc(const THandle& handle, 
                             const CSeqdesc& desc,
                             CEditsSaver::TCommands& cmds)
{
    CRef<CSeqEdit_Cmd> cmd;
    SCmdCreator<CSeqEdit_Cmd::e_Add_desc>::CreateCmd(handle,cmd).
        SetAdd_desc(const_cast<CSeqdesc&>(desc));
    cmds.push_back(cmd); 
}
void CEditsSaver::AddDesc(const CBioseq_Handle& handle, 
                          const CSeqdesc& desc, ECallMode mode)
{
    s_AddDesc(handle, desc, m_Commands);
}
void CEditsSaver::AddDesc(const CBioseq_set_Handle& handle, 
                          const CSeqdesc& desc, ECallMode mode)
{
    s_AddDesc(handle, desc, m_Commands);
}

template<typename THandle>
static inline void s_RemoveDesc(const THandle& handle, 
                                const CSeqdesc& desc,
                                CEditsSaver::TCommands& cmds)
{
    CRef<CSeqEdit_Cmd> cmd;
    SCmdCreator<CSeqEdit_Cmd::e_Remove_desc>::CreateCmd(handle,cmd).
        SetRemove_desc(const_cast<CSeqdesc&>(desc));
    cmds.push_back(cmd); 
}
void CEditsSaver::RemoveDesc(const CBioseq_Handle& handle, 
                             const CSeqdesc& desc, ECallMode mode)
{
    s_RemoveDesc(handle, desc, m_Commands);
}
void CEditsSaver::RemoveDesc(const CBioseq_set_Handle& handle, 
                             const CSeqdesc& desc, ECallMode mode)
{
    s_RemoveDesc(handle, desc, m_Commands);
}
//------------------------------------------------------------------
template<int>
struct SSeqAttrChanger;

#define DEFINSTCH(type)                                                         \
template<>                                                                      \
struct SSeqAttrChanger<CSeqEdit_Cmd_ChangeSeqAttr::TData::e_##type> {           \
    typedef CSeqEdit_Cmd_ChangeSeqAttr::TData::T##type TParam;                  \
    static inline void CreateCmd(const CBioseq_Handle& handle,                  \
                                 const TParam& value,                           \
                                 CEditsSaver::TCommands& cmds)                  \
    {                                                                           \
        CRef<CSeqEdit_Cmd> cmd;                                                 \
        CSeqEdit_Cmd_ChangeSeqAttr& c =                                         \
            SCmdCreator<CSeqEdit_Cmd::e_Change_seqattr>::CreateCmd(handle,cmd); \
        c.SetData().Set##type(const_cast<TParam&>(value));                      \
        cmds.push_back(cmd);                                                    \
    }                                                                           \
}

DEFINSTCH(Inst);
DEFINSTCH(Repr);
DEFINSTCH(Mol);
DEFINSTCH(Length);
DEFINSTCH(Topology);
DEFINSTCH(Strand);
DEFINSTCH(Fuzz);
DEFINSTCH(Ext);
DEFINSTCH(Hist);
DEFINSTCH(Seq_data);

#undef DEFINSTCH

void CEditsSaver::SetSeqInst(const CBioseq_Handle& handle, 
                             const CSeq_inst& value, 
                             ECallMode mode)
{   
    SSeqAttrChanger<CSeqEdit_Cmd_ChangeSeqAttr::TData::e_Inst>::
        CreateCmd(handle,value,m_Commands);
}

void CEditsSaver::SetSeqInstRepr(const CBioseq_Handle& handle, 
                                 CSeq_inst::TRepr value, ECallMode mode)
{
    SSeqAttrChanger<CSeqEdit_Cmd_ChangeSeqAttr::TData::e_Repr>::
        CreateCmd(handle,value,m_Commands);
}
void CEditsSaver::SetSeqInstMol(const CBioseq_Handle& handle, 
                                CSeq_inst::TMol value, ECallMode mode)
{
    SSeqAttrChanger<CSeqEdit_Cmd_ChangeSeqAttr::TData::e_Mol>::
        CreateCmd(handle,value,m_Commands);
}
void CEditsSaver::SetSeqInstLength(const CBioseq_Handle& handle, 
                                   CSeq_inst::TLength value,
                                   ECallMode mode)
{
    SSeqAttrChanger<CSeqEdit_Cmd_ChangeSeqAttr::TData::e_Length>::
        CreateCmd(handle,value,m_Commands);
}
void CEditsSaver::SetSeqInstFuzz(const CBioseq_Handle& handle, 
                                 const CSeq_inst::TFuzz& value, 
                                 ECallMode mode)
{
    SSeqAttrChanger<CSeqEdit_Cmd_ChangeSeqAttr::TData::e_Fuzz>::
        CreateCmd(handle,value,m_Commands);
}
void CEditsSaver::SetSeqInstTopology(const CBioseq_Handle& handle, 
                                     CSeq_inst::TTopology value,
                                     ECallMode mode)
{
    SSeqAttrChanger<CSeqEdit_Cmd_ChangeSeqAttr::TData::e_Topology>::
        CreateCmd(handle,value,m_Commands);
}
void CEditsSaver::SetSeqInstStrand(const CBioseq_Handle& handle, 
                                   CSeq_inst::TStrand value, 
                                   ECallMode mode)
{
    SSeqAttrChanger<CSeqEdit_Cmd_ChangeSeqAttr::TData::e_Strand>::
        CreateCmd(handle,value,m_Commands);
}
void CEditsSaver::SetSeqInstExt(const CBioseq_Handle& handle, 
                                const CSeq_inst::TExt& value, 
                                ECallMode mode)
{
    SSeqAttrChanger<CSeqEdit_Cmd_ChangeSeqAttr::TData::e_Ext>::
        CreateCmd(handle,value,m_Commands);
}
void CEditsSaver::SetSeqInstHist(const CBioseq_Handle& handle, 
                                 const CSeq_inst::THist& value, 
                                 ECallMode mode)
{
    SSeqAttrChanger<CSeqEdit_Cmd_ChangeSeqAttr::TData::e_Hist>::
        CreateCmd(handle,value,m_Commands);
}
void CEditsSaver::SetSeqInstSeq_data(const CBioseq_Handle& handle, 
                                     const CSeq_inst::TSeq_data& value, 
                                     ECallMode mode)
{
    SSeqAttrChanger<CSeqEdit_Cmd_ChangeSeqAttr::TData::e_Seq_data>::
        CreateCmd(handle,value,m_Commands);
}

static inline 
void s_ResetSeqAttr(const CBioseq_Handle& handle, 
                    CSeqEdit_Cmd_ResetSeqAttr::TWhat what, 
                    CEditsSaver::TCommands& cmds)
{
    CRef<CSeqEdit_Cmd> cmd;
    SCmdCreator<CSeqEdit_Cmd::e_Reset_seqattr>::CreateCmd(handle,cmd).
        SetWhat(what);
    cmds.push_back(cmd);
}
    
void CEditsSaver::ResetSeqInst(const CBioseq_Handle& handle, 
                               ECallMode mode)
{
    s_ResetSeqAttr(handle, CSeqEdit_Cmd_ResetSeqAttr::eWhat_inst,
                   m_Commands);
}
void CEditsSaver::ResetSeqInstRepr(const CBioseq_Handle& handle, 
                                   ECallMode mode)
{
    s_ResetSeqAttr(handle, CSeqEdit_Cmd_ResetSeqAttr::eWhat_repr,
                   m_Commands);
}
void CEditsSaver::ResetSeqInstMol(const CBioseq_Handle& handle, 
                                  ECallMode mode)
{
    s_ResetSeqAttr(handle, CSeqEdit_Cmd_ResetSeqAttr::eWhat_mol,
                   m_Commands);
}
void CEditsSaver::ResetSeqInstLength(const CBioseq_Handle& handle, 
                                     ECallMode mode)
{
    s_ResetSeqAttr(handle, CSeqEdit_Cmd_ResetSeqAttr::eWhat_length,
                    m_Commands);
}
void CEditsSaver::ResetSeqInstFuzz(const CBioseq_Handle& handle, 
                                   ECallMode mode)
{
    s_ResetSeqAttr(handle, CSeqEdit_Cmd_ResetSeqAttr::eWhat_fuzz,
                   m_Commands);
}
void CEditsSaver::ResetSeqInstTopology(const CBioseq_Handle& handle, 
                                       ECallMode mode)
{
    s_ResetSeqAttr(handle, CSeqEdit_Cmd_ResetSeqAttr::eWhat_topology,
                   m_Commands);
}
void CEditsSaver::ResetSeqInstStrand(const CBioseq_Handle& handle, 
                                     ECallMode mode)
{
    s_ResetSeqAttr(handle, CSeqEdit_Cmd_ResetSeqAttr::eWhat_strand,
                   m_Commands);
} 
void CEditsSaver::ResetSeqInstExt(const CBioseq_Handle& handle, 
                                  ECallMode mode)
{
    s_ResetSeqAttr(handle, CSeqEdit_Cmd_ResetSeqAttr::eWhat_ext,
                   m_Commands);
}
void CEditsSaver::ResetSeqInstHist(const CBioseq_Handle& handle, 
                                   ECallMode mode)
{
    s_ResetSeqAttr(handle, CSeqEdit_Cmd_ResetSeqAttr::eWhat_hist,
                   m_Commands);
}
void CEditsSaver::ResetSeqInstSeq_data(const CBioseq_Handle& handle, 
                                       ECallMode mode)
{
    s_ResetSeqAttr(handle, CSeqEdit_Cmd_ResetSeqAttr::eWhat_seq_data,
                   m_Commands);

}

    //----------------------------------------------------------------
void CEditsSaver::AddId(const CBioseq_Handle& handle, 
                        const CSeq_id_Handle& id, 
                        ECallMode mode)
{
    
    CRef<CSeqEdit_Cmd> cmd;
    SCmdCreator<CSeqEdit_Cmd::e_Add_id>::CreateCmd(handle,cmd).
        SetAdd_id(const_cast<CSeq_id&>(*id.GetSeqId()));
    m_Commands.push_back(cmd); 
    m_ChangedIds[id] = cmd->GetBlobId();
}
void CEditsSaver::RemoveId(const CBioseq_Handle& handle, 
                                const CSeq_id_Handle& id, 
                                ECallMode mode)
{
    CRef<CSeqEdit_Cmd> cmd;
    SCmdCreator<CSeqEdit_Cmd::e_Remove_id>::CreateCmd(handle,cmd).
        SetRemove_id(const_cast<CSeq_id&>(*id.GetSeqId()));
    m_Commands.push_back(cmd); 
    m_ChangedIds[id] = "";
}

static inline CRef<CSeq_id> s_ConvertId(const CSeq_id_Handle& handle)
{
    return CRef<CSeq_id>(const_cast<CSeq_id*>(&*handle.GetSeqId()));
}
void CEditsSaver::ResetIds(const CBioseq_Handle& handle, 
                           const TIds& ids,
                           ECallMode mode)
{
    typedef CSeqEdit_Cmd_ResetIds::TRemove_ids TList;
    CRef<CSeqEdit_Cmd> cmd;
    TList& id_list = SCmdCreator<CSeqEdit_Cmd::e_Reset_ids>::CreateCmd(handle,cmd)
        .SetRemove_ids();
    transform(ids.begin(), ids.end(), back_inserter(id_list), s_ConvertId);
    m_Commands.push_back(cmd); 
    ITERATE(TIds, id, ids) {
        m_ChangedIds[*id] = "";
    }
}
//-------------------------------------------------------
template<int>
struct SSetAttrChanger;

#define DEFINSTCH(type)                                                         \
template<>                                                                      \
struct SSetAttrChanger<CSeqEdit_Cmd_ChangeSetAttr::TData::e_##type> {           \
    typedef CSeqEdit_Cmd_ChangeSetAttr::TData::T##type TParam;                  \
    static inline void CreateCmd(const CBioseq_set_Handle& handle,              \
                                 const TParam& value,                           \
                                 CEditsSaver::TCommands& cmds)                  \
    {                                                                           \
        CRef<CSeqEdit_Cmd> cmd;                                                 \
        CSeqEdit_Cmd_ChangeSetAttr& c =                                         \
            SCmdCreator<CSeqEdit_Cmd::e_Change_setattr>::CreateCmd(handle,cmd); \
        c.SetData().Set##type(const_cast<TParam&>(value));                      \
        cmds.push_back(cmd);                                                    \
    }                                                                           \
}

DEFINSTCH(Id);
DEFINSTCH(Coll);
DEFINSTCH(Level);
DEFINSTCH(Class);
DEFINSTCH(Release);
DEFINSTCH(Date);

#undef DEFINSTCH

void CEditsSaver::SetBioseqSetId(const CBioseq_set_Handle& handle,
                                 const CBioseq_set::TId& value, 
                                 ECallMode mode)
{
    SSetAttrChanger<CSeqEdit_Cmd_ChangeSetAttr::TData::e_Id>::
        CreateCmd(handle,value,m_Commands);
}
void CEditsSaver::SetBioseqSetColl(const CBioseq_set_Handle& handle,
                                   const CBioseq_set::TColl& value, 
                                   ECallMode mode)
{
    SSetAttrChanger<CSeqEdit_Cmd_ChangeSetAttr::TData::e_Coll>::
        CreateCmd(handle,value,m_Commands);
}
void CEditsSaver::SetBioseqSetLevel(const CBioseq_set_Handle& handle,
                                    CBioseq_set::TLevel value, 
                                    ECallMode mode)
{
    SSetAttrChanger<CSeqEdit_Cmd_ChangeSetAttr::TData::e_Level>::
        CreateCmd(handle,value,m_Commands);
}
void CEditsSaver::SetBioseqSetClass(const CBioseq_set_Handle& handle,
                                    CBioseq_set::TClass value, 
                                    ECallMode mode)
{
    SSetAttrChanger<CSeqEdit_Cmd_ChangeSetAttr::TData::e_Class>::
        CreateCmd(handle,value,m_Commands);
}
void CEditsSaver::SetBioseqSetRelease(const CBioseq_set_Handle& handle,
                                      const CBioseq_set::TRelease& value,  
                                      ECallMode mode)
{
    SSetAttrChanger<CSeqEdit_Cmd_ChangeSetAttr::TData::e_Release>::
        CreateCmd(handle,value,m_Commands);
}
void CEditsSaver::SetBioseqSetDate(const CBioseq_set_Handle& handle,
                                   const CBioseq_set::TDate& value, 
                                   ECallMode mode)
{
    SSetAttrChanger<CSeqEdit_Cmd_ChangeSetAttr::TData::e_Date>::
        CreateCmd(handle,value,m_Commands);
}

static inline 
void s_ResetSetAttr(const CBioseq_set_Handle& handle, 
                    CSeqEdit_Cmd_ResetSetAttr::TWhat what, 
                    CEditsSaver::TCommands& cmds)
{
    CRef<CSeqEdit_Cmd> cmd;
    SCmdCreator<CSeqEdit_Cmd::e_Reset_setattr>::CreateCmd(handle,cmd).
        SetWhat(what);
    cmds.push_back(cmd);
}
 
void CEditsSaver::ResetBioseqSetId(const CBioseq_set_Handle& handle, 
                                   ECallMode mode)
{
    s_ResetSetAttr(handle, CSeqEdit_Cmd_ResetSetAttr::eWhat_id,
                   m_Commands);
} 
void CEditsSaver::ResetBioseqSetColl(const CBioseq_set_Handle& handle, 
                                     ECallMode mode)
{
    s_ResetSetAttr(handle, CSeqEdit_Cmd_ResetSetAttr::eWhat_coll,
                   m_Commands);
}
void CEditsSaver::ResetBioseqSetLevel(const CBioseq_set_Handle& handle, 
                                      ECallMode mode)
{
    s_ResetSetAttr(handle, CSeqEdit_Cmd_ResetSetAttr::eWhat_level,
                   m_Commands);
}
void CEditsSaver::ResetBioseqSetClass(const CBioseq_set_Handle& handle, 
                                      ECallMode mode)
{
    s_ResetSetAttr(handle, CSeqEdit_Cmd_ResetSetAttr::eWhat_class,
                   m_Commands);
}
void CEditsSaver::ResetBioseqSetRelease(const CBioseq_set_Handle& handle, 
                                             ECallMode mode)
{
    s_ResetSetAttr(handle, CSeqEdit_Cmd_ResetSetAttr::eWhat_release,
                   m_Commands);
}
void CEditsSaver::ResetBioseqSetDate(const CBioseq_set_Handle& handle,
                                          ECallMode mode)
{
    s_ResetSetAttr(handle, CSeqEdit_Cmd_ResetSetAttr::eWhat_date,
                   m_Commands);
}
  
    //-----------------------------------------------------------------

void CEditsSaver::Attach(const CSeq_entry_Handle& handle, 
                         const CBioseq_Handle& bioseq, 
                         ECallMode mode)
{
    CRef<CSeqEdit_Cmd> cmd;
    SCmdCreator<CSeqEdit_Cmd::e_Attach_seq>::CreateCmd(handle,cmd).
        SetSeq(const_cast<CBioseq&>(*bioseq.GetCompleteBioseq()));
    m_Commands.push_back(cmd); 
    ITERATE(CBioseq_Handle::TId, id, bioseq.GetId()) {
        m_ChangedIds[*id] = cmd->GetBlobId();
    }
}

static void s_CollectSeqIds(const CSeq_entry& entry, IEditSaver::TIds& ids);
static void s_CollectSeqIds(const CBioseq_set& bset, IEditSaver::TIds& ids)
{
    if (bset.IsSetSeq_set()) {
        const CBioseq_set::TSeq_set& sset = bset.GetSeq_set();
        ITERATE(CBioseq_set::TSeq_set, entry, sset) {
            s_CollectSeqIds(**entry, ids);
        }
    }
}

static void s_CollectSeqIds(const CSeq_entry& entry, IEditSaver::TIds& ids)
{
    if (entry.IsSet())
        s_CollectSeqIds(entry.GetSet(),ids);
    else if (entry.IsSeq()) {
        const CBioseq::TId& bids = entry.GetSeq().GetId();
        ITERATE(CBioseq::TId, id, bids) {
            ids.insert(CSeq_id_Handle::GetHandle(**id));
        }
    }
}
void CEditsSaver::Attach(const CSeq_entry_Handle& handle, 
                         const CBioseq_set_Handle& bioseq_set, 
                         ECallMode mode)
{
    CRef<CSeqEdit_Cmd> cmd;
    const CBioseq_set& bset = *bioseq_set.GetCompleteBioseq_set();
    SCmdCreator<CSeqEdit_Cmd::e_Attach_set>::CreateCmd(handle,cmd).
        SetSet(const_cast<CBioseq_set&>(bset));
    m_Commands.push_back(cmd); 
    IEditSaver::TIds ids;
    s_CollectSeqIds(bset,ids);
    ITERATE(IEditSaver::TIds, id, ids) {
        m_ChangedIds[*id] = cmd->GetBlobId();
    }
    
}
void CEditsSaver::Detach(const CSeq_entry_Handle& entry, 
                         const CBioseq_Handle& handle, ECallMode mode)
{
    CRef<CSeqEdit_Cmd> cmd;
    SCmdCreator<CSeqEdit_Cmd::e_Reset_seqentry>
        ::CreateCmd(entry, handle.GetBioObjectId(),cmd);
    m_Commands.push_back(cmd); 
    ITERATE(CBioseq_Handle::TId, id, handle.GetId()) {
        m_ChangedIds[*id] = "";
    }

}
void CEditsSaver::Detach(const CSeq_entry_Handle& entry, 
                         const CBioseq_set_Handle& handle, ECallMode mode)
{
    CRef<CSeqEdit_Cmd> cmd;
    const CBioseq_set& bset = *handle.GetCompleteBioseq_set();
    SCmdCreator<CSeqEdit_Cmd::e_Reset_seqentry>
        ::CreateCmd(entry, handle.GetBioObjectId(),cmd);
    m_Commands.push_back(cmd); 
    IEditSaver::TIds ids;
    s_CollectSeqIds(bset,ids);
    ITERATE(IEditSaver::TIds, id, ids) {
        m_ChangedIds[*id] = "";
    }
}

void CEditsSaver::Attach(const CSeq_entry_Handle& entry, 
                         const CSeq_annot_Handle& annot, 
                         ECallMode mode)
{
    CRef<CSeqEdit_Cmd> cmd;
    SCmdCreator<CSeqEdit_Cmd::e_Attach_annot>::CreateCmd(entry,cmd).
        SetAnnot(const_cast<CSeq_annot&>(*annot.GetCompleteSeq_annot()));   
    m_Commands.push_back(cmd); 
}

void CEditsSaver::Attach(const CBioseq_set_Handle& handle, 
                         const CSeq_entry_Handle& entry, 
                         int index, ECallMode mode)
{
    CRef<CSeqEdit_Cmd> cmd;
    const CSeq_entry& sentry = *entry.GetCompleteSeq_entry();
    CSeqEdit_Cmd_AttachSeqEntry& c = 
        SCmdCreator<CSeqEdit_Cmd::e_Attach_seqentry>::CreateCmd(handle,cmd);
    c.SetSeq_entry(const_cast<CSeq_entry&>(sentry));
    c.SetIndex(index);
    m_Commands.push_back(cmd); 
    IEditSaver::TIds ids;
    s_CollectSeqIds(sentry,ids);
    ITERATE(IEditSaver::TIds, id, ids) {
        m_ChangedIds[*id] = cmd->GetBlobId();
    }
}
void CEditsSaver::Remove(const CBioseq_set_Handle& handle, 
                         const CSeq_entry_Handle& entry, 
                         int, ECallMode mode)
{
    CRef<CSeqEdit_Cmd> cmd;
    const CSeq_entry& sentry = *entry.GetCompleteSeq_entry();
    SCmdCreator<CSeqEdit_Cmd::e_Remove_seqentry>::CreateCmd(handle,cmd).
        SetEntry_id(*s_Convert(entry.GetBioObjectId()));
    m_Commands.push_back(cmd); 
    IEditSaver::TIds ids;
    s_CollectSeqIds(sentry,ids);
    ITERATE(IEditSaver::TIds, id, ids) {
        m_ChangedIds[*id] = "";
    }
}
/*
void CEditsSaver::RemoveTSE(const CTSE_Handle& handle, 
                                      ECallMode mode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "RemoveTSE(const CTSE_Handle&, ECallMode)");
}
*/
    //-----------------------------------------------------------------

template<int type>
struct SAnnotCmdPreparer {
    typedef SCmdCreator<type> TCmdCreator;
    typedef typename TCmdCreator::TCommand TCommand;

    static inline
    TCommand& PrepareCmd(const CSeq_annot_Handle& annot, 
                         CRef<CSeqEdit_Cmd>& cmd)
    {
        const CSeq_entry_Handle& entry = annot.GetParentEntry();
        return PrepareCmd(annot, entry, cmd);
    }
    static inline
    TCommand& PrepareCmd(const CSeq_annot_Handle& annot, 
                         const CSeq_entry_Handle& entry,
                         CRef<CSeqEdit_Cmd>& cmd)
    {
        TCommand& c = TCmdCreator::CreateCmd(entry, entry.GetBioObjectId(), cmd);
        if (annot.IsNamed()) {
            c.SetNamed(true);
            c.SetName(annot.GetName());
        } else {
            c.SetNamed(false);
        }
        return c;
    }
};

void CEditsSaver::Replace(const CSeq_feat_Handle& handle,
                          const CSeq_feat& old_value, 
                          ECallMode mode)
{                          
    const CSeq_annot_Handle& annot = handle.GetAnnot();
    CRef<CSeqEdit_Cmd> cmd;
    CSeqEdit_Cmd_ReplaceAnnot& c = 
        SAnnotCmdPreparer<CSeqEdit_Cmd::e_Replace_annot>::PrepareCmd(annot,cmd);
    c.SetData().SetFeat().SetOvalue(const_cast<CSeq_feat&>(old_value));
    c.SetData().SetFeat().SetNvalue(const_cast<CSeq_feat&>(*handle.GetSeq_feat()));
    m_Commands.push_back(cmd);

}
void CEditsSaver::Replace(const CSeq_align_Handle& handle,
                          const CSeq_align& old_value,
                          ECallMode mode)
{
    const CSeq_annot_Handle& annot = handle.GetAnnot();
    CRef<CSeqEdit_Cmd> cmd;
    CSeqEdit_Cmd_ReplaceAnnot& c = 
        SAnnotCmdPreparer<CSeqEdit_Cmd::e_Replace_annot>::PrepareCmd(annot,cmd);
    c.SetData().SetAlign().SetOvalue(const_cast<CSeq_align&>(old_value));
    c.SetData().SetAlign().SetNvalue(const_cast<CSeq_align&>(*handle.GetSeq_align()));
    m_Commands.push_back(cmd);
}
void CEditsSaver::Replace(const CSeq_graph_Handle& handle,
                          const CSeq_graph& old_value, 
                          ECallMode mode)
{
    const CSeq_annot_Handle& annot = handle.GetAnnot();
    CRef<CSeqEdit_Cmd> cmd;
    CSeqEdit_Cmd_ReplaceAnnot& c = 
        SAnnotCmdPreparer<CSeqEdit_Cmd::e_Replace_annot>::PrepareCmd(annot,cmd);
    c.SetData().SetGraph().SetOvalue(const_cast<CSeq_graph&>(old_value));
    c.SetData().SetGraph().SetNvalue(const_cast<CSeq_graph&>(*handle.GetSeq_graph()));
    m_Commands.push_back(cmd);
}


template<typename T>
struct AnnotObjTrait;

template<>
struct AnnotObjTrait<CSeq_feat> {
    typedef CSeq_annot::TData::TFtable TCont;
    static const TCont& GetCont(const CSeq_annot& annot) 
    {
        _ASSERT(annot.IsSetData() && 
                annot.GetData().Which() == CSeq_annot::TData::e_Ftable);
        return annot.GetData().GetFtable();
    }
    template<typename Holder>
    static void Set(Holder& holder, CSeq_feat& feat) { holder.SetFeat(feat); }
};


template<>
struct AnnotObjTrait<CSeq_align> {
    typedef CSeq_annot::TData::TAlign TCont;
    static const TCont& GetCont(const CSeq_annot& annot) 
    {
        _ASSERT(annot.IsSetData() && 
                annot.GetData().Which() == CSeq_annot::TData::e_Align);
        return annot.GetData().GetAlign();
    }
    template<typename Holder>
    static void Set(Holder& holder, CSeq_align& align) { holder.SetAlign(align); }
};

template<>
struct AnnotObjTrait<CSeq_graph> {
    typedef CSeq_annot::TData::TGraph TCont;
    static const TCont& GetCont(const CSeq_annot& annot) 
    {
        _ASSERT(annot.IsSetData() && 
                annot.GetData().Which() == CSeq_annot::TData::e_Graph);
        return annot.GetData().GetGraph();
    }
    template<typename Holder>
    static void Set(Holder& holder, CSeq_graph& graph) { holder.SetGraph(graph); }
};

template<typename T> static inline
void s_SetSearchParam(CSeqEdit_Cmd_AddAnnot& cmd, const T& new_obj, 
                      const CSeq_annot_Handle& handle)
{
    typedef AnnotObjTrait<T> TAnnotObjTrait;
    typedef typename TAnnotObjTrait::TCont TCont;
    CConstRef<CSeq_annot> annot = handle.GetCompleteSeq_annot();
    if (annot->IsSetData()) {
        const TCont& cont = TAnnotObjTrait::GetCont(*annot);
        if( cont.size() > 1 ) {
            ITERATE(typename TCont, it, cont) {
                if ( !(*it)->Equals(new_obj) ) {
                    T& obj = const_cast<T&>(**it);
                    TAnnotObjTrait::Set(cmd.SetSearch_param().SetObj(), obj);
                    return;
                }
            }
        }
    }
    if (annot->IsSetDesc()) {
        cmd.SetSearch_param()
            .SetDescr(const_cast<CSeq_annot::TDesc&>(annot->GetDesc()));
    }
}

template<typename T> static inline 
void s_AddAnnot(const CSeq_annot_Handle& handle, const T& value, 
                CEditsSaver::TCommands& cmds)
{
    CRef<CSeqEdit_Cmd> cmd;
    CSeqEdit_Cmd_AddAnnot& c = 
        SAnnotCmdPreparer<CSeqEdit_Cmd::e_Add_annot>::PrepareCmd(handle,cmd);

    s_SetSearchParam(c, value, handle);

    T& nc_value = const_cast<T&>(value);
    AnnotObjTrait<T>::Set(c.SetData(), nc_value);

    cmds.push_back(cmd); 
}


void CEditsSaver::Add(const CSeq_annot_Handle& handle,
                      const CSeq_feat& value, 
                      ECallMode mode)
{
    s_AddAnnot(handle, value, m_Commands);
}

void CEditsSaver::Add(const CSeq_annot_Handle& handle,
                      const CSeq_align& value, 
                      ECallMode mode)
{
    s_AddAnnot(handle, value, m_Commands);
}
void CEditsSaver::Add(const CSeq_annot_Handle& handle,
                      const CSeq_graph& value, 
                      ECallMode mode)
{
    s_AddAnnot(handle, value, m_Commands);
}

template<typename T> static inline
void s_RemoveAnnot(const CSeq_entry_Handle& entry,
                   const CSeq_annot_Handle& annot,
                   const T& old_value,
                   CEditsSaver::TCommands& cmds)
{
    CRef<CSeqEdit_Cmd> cmd;
    CSeqEdit_Cmd_RemoveAnnot& c = 
        SAnnotCmdPreparer<CSeqEdit_Cmd::e_Remove_annot>::PrepareCmd(annot,entry,cmd);

    T& nc_value = const_cast<T&>(old_value);
    AnnotObjTrait<T>::Set(c.SetData(), nc_value);

    cmds.push_back(cmd); 


}
void CEditsSaver::Remove(const CSeq_annot_Handle& handle, 
                         const CSeq_feat& old_value,
                         ECallMode mode)
{
    s_RemoveAnnot(handle.GetParentEntry(), handle, old_value, m_Commands);
}

void CEditsSaver::Remove(const CSeq_annot_Handle& handle, 
                         const CSeq_align& old_value,
                         ECallMode mode)
{
    s_RemoveAnnot(handle.GetParentEntry(), handle, old_value, m_Commands);
}
void CEditsSaver::Remove(const CSeq_annot_Handle& handle, 
                         const CSeq_graph& old_value,
                         ECallMode mode)
{
    s_RemoveAnnot(handle.GetParentEntry(), handle, old_value, m_Commands);
}


void CEditsSaver::Remove(const CSeq_entry_Handle& entry, 
                         const CSeq_annot_Handle& annot, 
                         ECallMode mode)
{
    CConstRef<CSeq_annot> annots = annot.GetCompleteSeq_annot();
    switch (annots->GetData().Which()) {
    case CSeq_annot::TData::e_Ftable :
        {
        const CSeq_annot::TData::TFtable& cont = annots->GetData().GetFtable();
        ITERATE(CSeq_annot::TData::TFtable, it, cont) {
            s_RemoveAnnot(entry, annot, **it, m_Commands);
        }
        }
        break;
    case CSeq_annot::TData::e_Align :
        {
        const CSeq_annot::TData::TAlign& cont = annots->GetData().GetAlign();
        ITERATE(CSeq_annot::TData::TAlign, it, cont) {
            s_RemoveAnnot(entry, annot, **it, m_Commands);
        }
        }
        break;
    case CSeq_annot::TData::e_Graph :
        {
        const CSeq_annot::TData::TGraph& cont = annots->GetData().GetGraph();
        ITERATE(CSeq_annot::TData::TGraph, it, cont) {
            s_RemoveAnnot(entry, annot, **it, m_Commands);
        }
        break;
        }
    default:
        return;
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/01/25 18:59:04  didenko
 * Redisgned bio objects edit facility
 *
 * ===========================================================================
 */
