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
 * Author:  Irena Zaretskaya
 *
 * File Description:
 *   Blast4_parameteres helper functions
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using the following specifications:
 *   'blast.asn'.
 */

// standard includes
#include <ncbi_pch.hpp>

// generated includes
#include <objects/blast/Blast4_parameters.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CBlast4_parameters::~CBlast4_parameters(void)
{
}

CRef< CBlast4_parameter> CBlast4_parameters::Add(const string name,const int &value) 
{
	CRef< CBlast4_parameter> p;
	p = GetParamByName(name); 		
	if(p.Empty()) {
		p.Reset(new CBlast4_parameter);
		CRef< CBlast4_value> v(new CBlast4_value);
		v->SetInteger(value);
		p->SetName(name);
		p->SetValue(*v);	
		this->Set().push_back(p);
	}
	else {
		p->SetValue().SetInteger(value);
	}
	return p;	
}


CRef< CBlast4_parameter> CBlast4_parameters::Add(const string name,const string &value) 
{
	CRef< CBlast4_parameter> p;
	p = GetParamByName(name); 		
	if(p.Empty()) {
		p.Reset(new CBlast4_parameter);
		CRef< CBlast4_value> v(new CBlast4_value);
		v->SetString(value);		
		p->SetName(name);
		p->SetValue(*v);
		this->Set().push_back(p);
	}
	else {
		p->SetValue().SetString(value);				
	}
	return p;	
}



CRef< CBlast4_parameter> CBlast4_parameters::Add(const string name,const bool &value) 
{
	CRef< CBlast4_parameter> p;	
	p = GetParamByName(name); 
	if(p.Empty()) {
		CRef< CBlast4_parameter> param(new CBlast4_parameter);
		CRef< CBlast4_value> v(new CBlast4_value);
		v->SetBoolean(value);
		param->SetName(name);
		param->SetValue(*v);
		this->Set().push_back(param);
	}
	else {
		p->SetValue().SetBoolean(value);
	}
	return p;	
}

CRef< CBlast4_parameter> CBlast4_parameters::Add(const string name,const double &value) 
{
	CRef< CBlast4_parameter> p;
	p = GetParamByName(name); 
	if(p.Empty()) {
		CRef< CBlast4_parameter> param(new CBlast4_parameter);
		CRef< CBlast4_value> v(new CBlast4_value);
		v->SetReal(value);		
		param->SetName(name);
		param->SetValue(*v);
		this->Set().push_back(param);
	}
	else {
		p->SetValue().SetReal(value);
	}
	return p;	
}


CRef< CBlast4_parameter> CBlast4_parameters::Add(const string name,const Int8 &value) 
{
	CRef< CBlast4_parameter> p;
	p = GetParamByName(name); 
	if(p.Empty()) {
		CRef< CBlast4_parameter> param(new CBlast4_parameter);
		CRef< CBlast4_value> v(new CBlast4_value);
		v->SetBig_integer(value);		
		param->SetName(name);
		param->SetValue(*v);
		this->Set().push_back(param);
	}
	else {
		p->SetValue().SetBig_integer(value);
	}
	return p;	
}

CRef< CBlast4_parameter> CBlast4_parameters::Add(const string name,const EBlast4_strand_type &value) 
{
	CRef< CBlast4_parameter> p;
	p = GetParamByName(name); 
	if(p.Empty()) {
		CRef< CBlast4_parameter> param(new CBlast4_parameter);
		CRef< CBlast4_value> v(new CBlast4_value);
		v->SetStrand_type(value);		
		param->SetName(name);
		param->SetValue(*v);
		this->Set().push_back(param);
	}
	else {
		p->SetValue().SetStrand_type(value);
	}
	return p;	
}

CRef< CBlast4_parameter> CBlast4_parameters::Add(const string name,const CBlast4_cutoff &value)
{
	CRef< CBlast4_parameter> p;
	p = GetParamByName(name); 		
	if(p.Empty()) {
		p.Reset(new CBlast4_parameter);
		CRef< CBlast4_value> v(new CBlast4_value);
		v->SetCutoff((CBlast4_cutoff &)value);		
		p->SetName(name);
		p->SetValue(*v);
		this->Set().push_back(p);
	}
	else {
		p->SetValue().SetCutoff((CBlast4_cutoff &)value);				
	}
	return p;	
}



CRef <CBlast4_parameter> CBlast4_parameters::GetParamByName(const string name) 
{
	CRef <CBlast4_parameter> blast4ParamFound;

	ITERATE(Tdata,it,this->Set()) {
		if((*it)->GetName() == name) {
			blast4ParamFound = (*it);			
			break;
		}
	}
	return blast4ParamFound;	
}

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 65, chars: 1911, CRC32: c0649b03 */
