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
* Author:  Sergiy Gotvyanskyy, NCBI
*
* File Description:
*   Reader for feature tables 
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/general/Object_id.hpp>

#include <objects/submit/Seq_submit.hpp>
//#include <objects/seq/Seq_annot.hpp>
//#include <objects/seqloc/Seq_interval.hpp>
//#include <objects/general/User_object.hpp>
//#include <objects/seq/Annot_descr.hpp>
//#include <objects/seq/Annotdesc.hpp>


#include <util/line_reader.hpp>
#include <objtools/readers/readfeat.hpp>

#include <objmgr/util/sequence.hpp>
#include <objmgr/object_manager.hpp>

//#include <objects/seq/Seq_data.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include "feature_table_reader.hpp"

#include "table2asn_context.hpp"


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

namespace
{

void MoveSomeDescr(CSeq_entry& dest, CBioseq& src)
{
	CSeq_descr::Tdata::iterator it = src.SetDescr().Set().begin();

	while(it != src.SetDescr().Set().end())
	{
		switch ((**it).Which())
		{
		case CSeqdesc_Base::e_Pub:
		case CSeqdesc_Base::e_Source:
			{
			dest.SetDescr().Set().push_back(*it); 
			src.SetDescr().Set().erase(it++);
			}
			break;
		default:
			it++;
		}
	}
}

bool GetOrgName(string& name, const CSeq_entry& entry)
{
	if (entry.IsSet())
	{
		ITERATE(CSeq_descr_Base::Tdata, it, entry.GetSet().GetDescr().Get())
		{
			if ((**it).IsOrg())
			{
				if ((**it).GetOrg().IsSetOrgname())
				{
					if ((**it).GetOrg().GetOrgname().GetFlatName(name))
						return true;
				}
			}
			if ((**it).IsSource())
			{
				if ((**it).GetSource().IsSetOrgname())
				{
					if ((**it).GetSource().GetOrgname().GetFlatName(name))
						return true;
				}
				if ((**it).GetSource().IsSetOrg())
				{
					if ((**it).GetSource().GetOrg().GetOrgname().GetFlatName(name))
						return true;
				}
				if ((**it).GetSource().IsSetTaxname())
				{
					name = (**it).GetSource().GetTaxname();
					return true;
				}

			}

		}
	}
	else
	if (entry.IsSeq())
	{
	}
	return false;
}

bool GetProteinName(string& protein_name, const CSeq_feat& feature)
{
	if (feature.IsSetXref())
	{
		ITERATE(CSeq_feat_Base::TXref, xref_it, feature.GetXref())
		{
			if ((**xref_it).IsSetData())
			{
				if ((**xref_it).GetData().IsProt())
				{
					protein_name = (**xref_it).GetData().GetProt().GetName().front();
					return true;
				}
			}
		}
	}
	return false;
}

CRef<objects::CSeq_id> 
GetNewProteinId(objects::CSeq_entry_Handle seh, const string& id_base) 
{
#if 0
    string id_base;
    objects::CSeq_id_Handle hid;

    ITERATE(objects::CBioseq_Handle::TId, it, bsh.GetId()) {
        if (!hid || !it->IsBetter(hid)) {
            hid = *it;
        }
    }

    hid.GetSeqId()->GetLabel(&id_base, objects::CSeq_id::eContent);
#endif
    
    int offset = 1;
    string id_label = id_base + "_" + NStr::NumericToString(offset);
    CRef<objects::CSeq_id> id(new objects::CSeq_id());
    id->SetLocal().SetStr(id_label);
    objects::CBioseq_Handle b_found = seh.GetBioseqHandle(*id);
    while (b_found) {
        offset++;
        id_label = id_base + "_" + NStr::NumericToString(offset);
        id->SetLocal().SetStr(id_label);
        b_found = seh.GetBioseqHandle(*id);
    }
    return id;
}

CRef<CSeq_entry> TranslateProtein(CScope& scope, CSeq_entry_Handle top_entry_h, const CSeq_feat& feature)
{
	CRef<CBioseq> protein = CSeqTranslator::TranslateToProtein(feature, scope);

	if (protein.Empty())
		return CRef<CSeq_entry>();

	CBioseq_Handle bioseq_handle = scope.AddBioseq(*protein);

	CRef<CSeq_entry> pentry(new CSeq_entry);
	pentry->SetSeq(*protein);

	string org_name;

	GetOrgName(org_name, *top_entry_h.GetCompleteObject());

	CRef<CSeqdesc> molinfo_desc(new CSeqdesc);
	molinfo_desc->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
	molinfo_desc->SetMolinfo().SetTech(CMolInfo::eTech_concept_trans);
	pentry->SetSeq().SetDescr().Set().push_back(molinfo_desc);

	string protein_name;
	GetProteinName(protein_name, feature);
	if (org_name.length() > 0)
		protein_name += " [" + org_name + "]";

	CRef<CSeqdesc> title_desc(new CSeqdesc);
	title_desc->SetTitle(protein_name);
	pentry->SetSeq().SetDescr().Set().push_back(title_desc);

	string base_name;
	if (feature.IsSetProduct())
	{
		const CSeq_id* id = feature.GetProduct().GetId();
		if (id)
		{
			//id->GetLabel(&base_name, CSeq_id::eContent);
			CRef<CSeq_id> newid(new CSeq_id);
			newid->Assign(*id);
			protein->SetId().push_back(newid);
		}
		else
		{
			CRef<CSeq_id> newid = GetNewProteinId(top_entry_h, base_name);
			protein->SetId().push_back(newid);
		}
	}


	return pentry;
}

void ParseCdregions(CSeq_entry& entry)
{
	if (!entry.IsSet() ||
		entry.GetSet().GetClass() != CBioseq_set::eClass_nuc_prot)
		return;

	CRef<CObjectManager> mgr(CObjectManager::GetInstance());

	CScope scope(*mgr);
	CSeq_entry_Handle entry_h = scope.AddTopLevelSeqEntry(entry);

	// Create empty annotation holding cdregion features
	CRef<CSeq_annot> set_annot(new CSeq_annot);
	set_annot->SetData().SetFtable();
	entry.SetSet().SetAnnot().push_back(set_annot);

	NON_CONST_ITERATE(CBioseq_set::TSeq_set, seq_it, entry.SetSet().SetSeq_set())
	{
		CRef<CSeq_entry> seq = *seq_it;
		if (seq->IsSeq() &&
			seq->GetSeq().IsSetInst() &&
			seq->GetSeq().IsNa() )
		NON_CONST_ITERATE(CBioseq::TAnnot, annot_it, seq->SetSeq().SetAnnot())
		{
			CRef<CSeq_annot> seq_annot(*annot_it);

			if (seq_annot->IsFtable())
			for (CSeq_annot::TData::TFtable::iterator feat_it = seq_annot->SetData().SetFtable().begin();
				seq_annot->SetData().SetFtable().end() != feat_it;)
			{
				CRef<CSeq_feat> feature = (*feat_it);
				if (feature->IsSetData())
				{
					if (feature->SetData().IsCdregion())
					{
						CRef<CSeq_entry> protein = TranslateProtein(scope, entry_h, *feature);
						if (protein.NotEmpty())
						{
							entry.SetSet().SetSeq_set().push_back(protein);
							// move the cdregion into protein and step iterator to next
							set_annot->SetData().SetFtable().push_back(feature);
							seq_annot->SetData().SetFtable().erase(feat_it++);
							continue; // avoid iterator increment 
						}
					}
				}
				++feat_it;
			}
		}
	}
	entry.Parentize();
}

void ConvertSeqIntoSeqSet(CSeq_entry& entry)
{
	CRef<CSeq_entry> newentry(new CSeq_entry);
	newentry->SetSeq(entry.SetSeq());

	entry.SetSet().SetSeq_set().push_back(newentry);
	MoveSomeDescr(entry, newentry->SetSeq());
	entry.SetSet().SetClass(CBioseq_set::eClass_nuc_prot);
}

bool CheckIfNeedConversion(const CSeq_entry& entry)
{
	ITERATE(CSeq_entry::TAnnot, annot_it, entry.GetAnnot())
	{
		if ((**annot_it).IsFtable())
		{
			ITERATE(CSeq_annot::C_Data::TFtable, feat_it, (**annot_it).GetData().GetFtable())
			{
				if((**feat_it).CanGetData())
				{
					switch ((**feat_it).GetData().Which())
					{
					case CSeqFeatData::e_Cdregion:
						return true;
					default:
						break;
					}
				}
			}
		}
	}

	return false;
}

void MergeCdRegions(CSeq_entry& entry)
{
	switch(entry.Which())
	{
	case CSeq_entry::e_Seq:
		if (CheckIfNeedConversion(entry))
		{
			ConvertSeqIntoSeqSet(entry);
    			ParseCdregions(entry);
		}
		break;
	case CSeq_entry::e_Set:
   		ParseCdregions(entry);
		break;
	default:
		break;
	}
}

}

void CFeatureTableReader::MergeFeatures(CRef<CSerialObject>& obj)
{
	CRef<CSeq_entry> entry(dynamic_cast<CSeq_entry*>(obj.GetPointerOrNull()));
	if (entry.NotEmpty())
	{
		MergeCdRegions(*entry);
	}
	else
	{
		CRef<CSeq_submit> submit(dynamic_cast<CSeq_submit*>(obj.GetPointerOrNull()));
		if (submit.NotEmpty())
		NON_CONST_ITERATE(CSeq_submit::TData::TEntrys, it, submit->SetData().SetEntrys())
		{
			MergeCdRegions(**it);
		}
	}
}

void CFeatureTableReader::ReadFeatureTable(CRef<CSerialObject>& obj, ILineReader& line_reader)
{
	CRef<CSeq_entry> entry(dynamic_cast<CSeq_entry*>(obj.GetPointerOrNull()));
	CFeature_table_reader::ReadSequinFeatureTables(line_reader, *entry, CFeature_table_reader::fCreateGenesFromCDSs);
}


END_NCBI_SCOPE

