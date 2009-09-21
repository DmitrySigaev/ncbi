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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   validation of seq_desc
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include "validatorp.hpp"
#include "utilities.hpp"

#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>

#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objtools/format/items/comment_item.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)


CValidError_desc::CValidError_desc(CValidError_imp& imp) :
    CValidError_base(imp)
{
}


CValidError_desc::~CValidError_desc(void)
{
}


/**
 * Validate descriptors as stand alone objects (no context)
 **/
void CValidError_desc::ValidateSeqDesc
(const CSeqdesc& desc,
 const CSeq_entry& ctx)
{
    m_Ctx.Reset(&ctx);

    // switch on type, e.g., call ValidateBioSource, ValidatePubdesc, ...
    switch ( desc.Which() ) {
        case CSeqdesc::e_Modif:
        case CSeqdesc::e_Mol_type:
        case CSeqdesc::e_Method:
            PostErr(eDiag_Info, eErr_SEQ_DESCR_InvalidForType,
                desc.SelectionName(desc.Which()) + " descriptor is obsolete", *m_Ctx, desc);
            break;

        case CSeqdesc::e_Comment:
            ValidateComment(desc.GetComment(), desc);
            break;

        case CSeqdesc::e_Pub:
            m_Imp.ValidatePubdesc(desc.GetPub(), desc, &ctx);
            break;

        case CSeqdesc::e_User:
            ValidateUser(desc.GetUser(), desc);
            break;

        case CSeqdesc::e_Source:
            m_Imp.ValidateBioSource (desc.GetSource(), desc, &ctx);
            break;
        
        case CSeqdesc::e_Molinfo:
            ValidateMolInfo(desc.GetMolinfo(), desc);
            break;

        case CSeqdesc::e_not_set:
            break;
        case CSeqdesc::e_Name:
            if (NStr::IsBlank (desc.GetName())) {
                PostErr (eDiag_Error, eErr_SEQ_DESCR_MissingText, 
                         "Name descriptor needs text", ctx, desc);
			}
            break;
        case CSeqdesc::e_Title:
            if (NStr::IsBlank (desc.GetTitle())) {
                PostErr (eDiag_Error, eErr_SEQ_DESCR_MissingText, 
                         "Title descriptor needs text", ctx, desc);
            } else {
                string title = desc.GetTitle();
                if (s_StringHasPMID (desc.GetTitle())) {
                    PostErr (eDiag_Warning, eErr_SEQ_DESCR_TitleHasPMID, 
                             "Title descriptor has internal PMID", ctx, desc);
                }
                NStr::TruncateSpacesInPlace (title);
                char end = title.c_str()[title.length() - 1];
                
                if (end == '.' && title.length() > 4) {
                    end = title.c_str()[title.length() - 2];
                }
                if (end == ','
                    || end == '.'
                    || end == ';'
                    || end == ':') {
                    PostErr (eDiag_Warning, eErr_SEQ_DESCR_BadPunctuation, 
                             "Title descriptor ends in bad punctuation", ctx, desc);
                }
            }
            break;
        case CSeqdesc::e_Org:
            break;
        case CSeqdesc::e_Num:
            break;
        case CSeqdesc::e_Maploc:
            break;
        case CSeqdesc::e_Pir:
            break;
        case CSeqdesc::e_Genbank:
            break;
        case CSeqdesc::e_Region:
			if (NStr::IsBlank (desc.GetRegion())) {
				PostErr (eDiag_Error, eErr_SEQ_DESCR_MissingText, 
						 "Region descriptor needs text", ctx, desc);
			}
            break;
        case CSeqdesc::e_Sp:
            break;
        case CSeqdesc::e_Dbxref:
            break;
        case CSeqdesc::e_Embl:
            break;
        case CSeqdesc::e_Create_date:
            {
                int rval = CheckDate (desc.GetCreate_date(), true);
                if (rval != eDateValid_valid) {
                    m_Imp.PostBadDateError (eDiag_Error, "Create date has error", rval, desc, &ctx);
                }
            }
            break;
        case CSeqdesc::e_Update_date:
            {
                int rval = CheckDate (desc.GetUpdate_date(), true);
                if (rval != eDateValid_valid) {
                    m_Imp.PostBadDateError (eDiag_Error, "Update date has error", rval, desc, &ctx);
                }
            }
            break;
        case CSeqdesc::e_Prf:
            break;
        case CSeqdesc::e_Pdb:
            break;
        case CSeqdesc::e_Het:
            break;
        default:
            break;
    }

    m_Ctx.Reset();
}


void CValidError_desc::ValidateComment
(const string& comment,
 const CSeqdesc& desc)
{
    if ( m_Imp.IsSerialNumberInComment(comment) ) {
        PostErr(eDiag_Info, eErr_SEQ_DESCR_SerialInComment,
            "Comment may refer to reference by serial number - "
            "attach reference specific comments to the reference "
            "REMARK instead.", *m_Ctx, desc);
    }
    if (NStr::IsBlank (comment)) {
        PostErr (eDiag_Error, eErr_SEQ_DESCR_MissingText, 
                 "Comment descriptor needs text", *m_Ctx, desc);
	}
}

void CValidError_desc::ValidateUser
(const CUser_object& usr,
 const CSeqdesc& desc)
{
    if ( !usr.CanGetType() ) {
        return;
    }
    const CObject_id& oi = usr.GetType();
    if ( !oi.IsStr() ) {
        return;
    }
    if ( NStr::EqualNocase(oi.GetStr(), "RefGeneTracking")) {
        bool has_ref_track_status = false;
        ITERATE(CUser_object::TData, field, usr.GetData()) {
            if ( (*field)->CanGetLabel() ) {
                const CObject_id& obj_id = (*field)->GetLabel();
                if ( !obj_id.IsStr() ) {
                    continue;
                }
                if ( NStr::CompareNocase(obj_id.GetStr(), "Status") == 0 ) {
                    has_ref_track_status = true;
					if ((*field)->IsSetData() && (*field)->GetData().IsStr()) {
						if (CCommentItem::GetRefTrackStatus(usr) == CCommentItem::eRefTrackStatus_Unknown) {
							PostErr(eDiag_Error, eErr_SEQ_DESCR_RefGeneTrackingIllegalStatus, 
									"RefGeneTracking object has illegal Status '" 
									+ (*field)->GetData().GetStr() + "'",
									*m_Ctx, desc);
						}
					}
                }
            }
        }
        if ( !has_ref_track_status ) {
            PostErr(eDiag_Error, eErr_SEQ_DESCR_RefGeneTrackingWithoutStatus,
                "RefGeneTracking object needs to have Status set", *m_Ctx, desc);
        }
    } else if ( NStr::EqualCase(oi.GetStr(), "StructuredComment")) {
        if (!usr.IsSetData() || usr.GetData().size() == 0) {
            PostErr (eDiag_Warning, eErr_SEQ_DESCR_UserObjectProblem, 
                     "Structured Comment user object descriptor is empty", *m_Ctx, desc);
        }
    }
}


void CValidError_desc::ValidateMolInfo
(const CMolInfo& minfo,
 const CSeqdesc& desc)
{
    if ( !minfo.IsSetBiomol() ) {
        return;
    }

    int biomol = minfo.GetBiomol();

    switch ( biomol ) {
    case CMolInfo::eBiomol_unknown:
        PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
            "Molinfo-biomol unknown used", *m_Ctx, desc);
        break;

    case CMolInfo::eBiomol_other:
        if ( !m_Imp.IsXR() ) {
            PostErr(eDiag_Warning, eErr_SEQ_DESCR_InvalidForType,
                "Molinfo-biomol other used", *m_Ctx, desc);
        }
        break;

    default:
        break;
    }
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
