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
 * Authors:  Sema
 * Created:  01/29/2015
 */

#ifndef _MISC_DISCREPANCY_DISCREPANCY_CORE_H_
#define _MISC_DISCREPANCY_DISCREPANCY_CORE_H_

#include <misc/discrepancy/discrepancy.hpp>
#include "report_object.hpp"
#include <objects/macro/Suspect_rule_set.hpp>
#include <objects/seqfeat/BioSource.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)

/// Housekeeping classes
class CDiscrepancyConstructor;


class CDiscrepancyConstructor
{
protected:
    virtual CRef<CDiscrepancyCase> Create(void) const = 0;
    static void Register(const string& name, const string& descr, TGroup group, CDiscrepancyConstructor& obj);
    static string GetDiscrepancyCaseName(const string& s);
    static const CDiscrepancyConstructor* GetDiscrepancyConstructor(const string& name);
    static map<string, CDiscrepancyConstructor*>& GetTable(void) { return sm_Table.Get();}
    static map<string, string>& GetDescrTable(void) { return sm_DescrTable.Get(); }
    static map<string, TGroup>& GetGroupTable(void) { return sm_GroupTable.Get(); }
    static map<string, string>& GetAliasTable(void) { return sm_AliasTable.Get(); }
    static map<string, vector<string> >& GetAliasListTable(void) { return sm_AliasListTable.Get(); }
private:
    static CSafeStatic<map<string, CDiscrepancyConstructor*> > sm_Table;
    static CSafeStatic<map<string, string> > sm_DescrTable;
    static CSafeStatic<map<string, TGroup> > sm_GroupTable;
    static CSafeStatic<map<string, string> > sm_AliasTable;
    static CSafeStatic<map<string, vector<string> > > sm_AliasListTable;

friend string GetDiscrepancyCaseName(const string& s);
friend string GetDiscrepancyDescr(const string& s);
friend TGroup GetDiscrepancyGroup(const string& s);
friend vector<string> GetDiscrepancyNames(void);
friend vector<string> GetDiscrepancyAliases(const string& name);
friend class CDiscrepancyAlias;
friend class CDiscrepancyContext;
};


inline void CDiscrepancyConstructor::Register(const string& name, const string& descr, TGroup group, CDiscrepancyConstructor& obj)
{
    GetTable()[name] = &obj;
    GetDescrTable()[name] = descr;
    GetGroupTable()[name] = group;
    GetAliasListTable()[name] = vector<string>();
}


class CDiscrepancyAlias : public CDiscrepancyConstructor
{
protected:
    CRef<CDiscrepancyCase> Create(void) const { return CRef<CDiscrepancyCase>(0);}
    static void Register(const string& name, const string& alias)
    {
        map<string, string>& AliasTable = GetAliasTable();
        if (AliasTable.find(alias) != AliasTable.end()) {
            return;
        }
        AliasTable[alias] = name;
        map<string, vector<string> >& AliasListTable = GetAliasListTable();
        AliasListTable[name].push_back(alias);
    }
};


/// CDiscrepancyItem and CReportObject

class CDiscrepancyObject : public CReportObject
{
public:
    CDiscrepancyObject(CConstRef<CBioseq> obj, CScope& scope, const string& filename, bool keep_ref, bool autofix = false) : CReportObject(obj, scope), m_Autofix(autofix)
    {
        SetFilename(filename);
        SetText(scope);
        if(!keep_ref) {
            DropReference();
        }
    }
    CDiscrepancyObject(CConstRef<CSeq_feat> obj, CScope& scope, const string& filename, bool keep_ref, bool autofix = false) : CReportObject(obj, scope), m_Autofix(autofix)
    {
        SetFilename(filename);
        SetText(scope);
        if(!keep_ref) {
            DropReference();
        }
    }
    bool CanAutofix() const { return m_Autofix; }
protected:
    bool m_Autofix;
};


class CReportNode;

class CDiscrepancyItem : public CReportItem
{
public:
    CDiscrepancyItem(const string& t, const string& s) : m_Title(t), m_Msg(s), m_Autofix(false) {}
    string GetTitle(void) const { return m_Title;}
    string GetMsg(void) const { return m_Msg;}
    TReportObjectList GetDetails(void) const { return m_Objs;}
    TReportItemList GetSubitems(void) const { return m_Subs;}
    bool CanAutofix() const { return m_Autofix;}
protected:
    string m_Title;
    string m_Msg;
    bool m_Autofix;
    TReportObjectList m_Objs;
    TReportItemList m_Subs;
friend class CReportNode;
};


/// CDiscrepancyCore and CDiscrepancyVisitor - parents for CDiscrepancyCase_* classes

class CDiscrepancyContext;
//typedef map<string, vector<CRef<CReportObj> > > TReportObjectMap; // sema: dont forget to remove it!

class CReportNode : public CObject
{
public:
    typedef map<string, CRef<CReportNode> > TNodeMap;
    CReportNode(const string& name = kEmptyStr) : m_Name(name) {}
    CReportNode& operator[](const string& name);
    void SetFatal(bool b = true) { m_Fatal = b; }
    void SetAutofix(bool b = true) { m_Autofix = b; }
    static void Add(TReportObjectList& list, CReportObj& obj, bool unique = true);
    void Add(CReportObj& obj, bool unique = true) { Add(m_Objs, obj, unique); }
    static void Add(TReportObjectList& list, TReportObjectList& objs, bool unique = true);
    void Add(TReportObjectList& objs, bool unique = true) { Add(m_Objs, objs, unique); }
    TReportObjectList& GetObjects() { return m_Objs; }
    TNodeMap& GetMap() { return m_Map; }
    CRef<CReportItem> Export(const string& test, bool unique = true);
    bool empty() { return m_Map.empty() && m_Objs.empty(); }
    void clear() { m_Map.clear(); m_Objs.clear(); }
protected:
    string m_Name;
    TNodeMap m_Map;
    TReportObjectList m_Objs;
    bool m_Fatal;
    bool m_Autofix;
};


class CDiscrepancyCore : public CDiscrepancyCase
{
public:
    CDiscrepancyCore() : m_Count(0) {}
    void Summarize(void){}
    virtual TReportItemList GetReport(void) const { return m_ReportItems;}
protected:
    CReportNode m_Objs;
    TReportItemList m_ReportItems;
    size_t m_Count;
};


template<typename T> class CDiscrepancyVisitor : public CDiscrepancyCore
{
public:
    void Call(const T& node, CDiscrepancyContext& context);
    virtual void Visit(const T& node, CDiscrepancyContext& context) = 0;
};


/// CDiscrepancyContext - manage and run the list of tests

class CDiscrepancyContext : public CDiscrepancySet
{
public:
    CDiscrepancyContext(objects::CScope& scope) :
            m_Scope(&scope),
            m_Count_Bioseq(0),
            m_Count_Seq_feat(0),
            m_Enable_CSeq_inst(false),
            m_Enable_CSeqFeatData(false)
        {}
    bool AddTest(const string& name);
    void Parse(const objects::CSeq_entry_Handle& handle);
    void Summarize(void);
    const vector<CRef<CDiscrepancyCase> >& GetTests(void){ return m_Tests;}

    template<typename T> void Call(CDiscrepancyVisitor<T>& disc, const T& obj){ disc.Call(obj, *this);}

    CConstRef<CBioseq> GetCurrentBioseq(void) const;
    CConstRef<CBioseq_set> GetCurrentBioseq_set(void) const { return m_Current_Bioseq_set;}
    CConstRef<CSeq_feat> GetCurrentSeq_feat(void) const { return m_Current_Seq_feat;}
    size_t GetCountBioseq(void) const { return m_Count_Bioseq;}
    size_t GetCountSeq_feat(void) const { return m_Count_Seq_feat;}
    objects::CScope& GetScope(void) const { return const_cast<objects::CScope&>(*m_Scope);}

    void SetSuspectRules(const string& name);
    CConstRef<CSuspect_rule_set> GetProductRules(void);
    CConstRef<CSuspect_rule_set> GetOrganelleProductRules(void);

    // Lazy
    const CBioSource* GetCurrentBiosource(void);
    CBioSource::TGenome GetCurrentGenome(void);

protected:
    CRef<objects::CScope> m_Scope;
    set<string> m_Names;
    vector<CRef<CDiscrepancyCase> > m_Tests;
    CConstRef<CBioseq> m_Current_Bioseq;
    CConstRef<CBioseq_set> m_Current_Bioseq_set;
    CConstRef<CSeq_feat> m_Current_Seq_feat;
    CConstRef<CSuspect_rule_set> m_ProductRules;
    CConstRef<CSuspect_rule_set> m_OrganelleProductRules;
    string m_SuspectRules;
    size_t m_Count_Bioseq;
    size_t m_Count_Seq_feat;

#define ADD_DISCREPANCY_TYPE(type) bool m_Enable_##type; vector<CDiscrepancyVisitor<type>* > m_All_##type;
    ADD_DISCREPANCY_TYPE(CSeq_inst)
    ADD_DISCREPANCY_TYPE(CSeqFeatData)
    ADD_DISCREPANCY_TYPE(CBioSource)
};


// MACRO definitions

// The following two macros are required to make sure all modules get loaded from the library
// Use this in discrepancy_core.cpp when adding a new module
#define DISCREPANCY_LINK_MODULE(name) \
    struct CDiscrepancyModule_##name { static void* dummy; CDiscrepancyModule_##name(void){ dummy=0;} };            \
    static CDiscrepancyModule_##name module_##name;

// Use this in the new module
#define DISCREPANCY_MODULE(name) \
    struct CDiscrepancyModule_##name { static void* dummy; CDiscrepancyModule_##name(void){ dummy=0;} };            \
    void* CDiscrepancyModule_##name::dummy=0;


#define DISCREPANCY_CASE(name, type, group, descr, ...) \
    class CDiscrepancyCase_##name : public CDiscrepancyVisitor<type>                                                \
    {                                                                                                               \
    public:                                                                                                         \
        void Visit(const type&, CDiscrepancyContext&);                                                              \
        void Summarize(void);                                                                                       \
        string GetName(void) const { return #name;}                                                                 \
        string GetType(void) const { return #type;}                                                                 \
    protected:                                                                                                      \
        __VA_ARGS__;                                                                                                \
    };                                                                                                              \
    class CDiscrepancyConstructor_##name : public CDiscrepancyConstructor                                           \
    {                                                                                                               \
    public:                                                                                                         \
        CDiscrepancyConstructor_##name(void){ Register(#name, descr, group, *this);}                                \
    protected:                                                                                                      \
        CRef<CDiscrepancyCase> Create(void) const { return CRef<CDiscrepancyCase>(new CDiscrepancyCase_##name);}    \
    };                                                                                                              \
    static CDiscrepancyConstructor_##name DiscrepancyConstructor_##name;                                            \
    static const char* descr_for_##name = descr;                                                                    \
    static TGroup group_for_##name = group;                                                                         \
    void CDiscrepancyCase_##name::Visit(const type& obj, CDiscrepancyContext& context)


#define DISCREPANCY_SUMMARIZE(name) \
    void CDiscrepancyCase_##name::Summarize(void)


#define DISCREPANCY_AUTOFIX(name) \
    class CDiscrepancyCaseA_##name : public CDiscrepancyCase_##name                                                 \
    {                                                                                                               \
    public:                                                                                                         \
        void Autofix(CScope& scope);                                                                                \
    };                                                                                                              \
    class CDiscrepancyCaseAConstructor_##name : public CDiscrepancyConstructor                                      \
    {                                                                                                               \
    public:                                                                                                         \
        CDiscrepancyCaseAConstructor_##name(void){ Register(#name, descr_for_##name, group_for_##name, *this);}     \
    protected:                                                                                                      \
        CRef<CDiscrepancyCase> Create(void) const { return CRef<CDiscrepancyCase>(new CDiscrepancyCaseA_##name);}   \
    };                                                                                                              \
    static CDiscrepancyCaseAConstructor_##name DiscrepancyCaseAConstructor_##name;                                  \
    void CDiscrepancyCaseA_##name::Autofix(CScope& scope)


#define DISCREPANCY_ALIAS(name, alias) \
    class CDiscrepancyCase_##alias { void* name; }; /* prevent name conflicts */                                    \
    class CDiscrepancyAlias_##alias : public CDiscrepancyAlias                                                      \
    {                                                                                                               \
    public:                                                                                                         \
        CDiscrepancyAlias_##alias(void) { Register(#name, #alias);}                                                 \
    };                                                                                                              \
    static CDiscrepancyAlias_##alias DiscrepancyAlias_##alias;


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE

#endif  // _MISC_DISCREPANCY_DISCREPANCY_CORE_H_
