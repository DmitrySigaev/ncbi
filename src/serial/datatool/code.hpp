#ifndef CODE_HPP
#define CODE_HPP

#include <corelib/ncbistd.hpp>
#include <set>
#include <list>
#include "autoptr.hpp"

BEGIN_NCBI_SCOPE

class CNcbiRegistry;

END_NCBI_SCOPE

USING_NCBI_SCOPE;

class ASNType;
class CClassCode;

class CFileCode
{
public:
    typedef set<string> TIncludes;
    typedef map<string, string> TForwards;
    typedef map<string, AutoPtr<CClassCode> > TClasses;

    CFileCode(const CNcbiRegistry& registry,
              const string& baseName, const string& headerPrefix);
    ~CFileCode(void);

    bool AddType(const ASNType* type);
    CClassCode* GetClassCode(const ASNType* type);

    string Include(const string& s) const;

    void AddHPPInclude(const string& s);
    void AddCPPInclude(const string& s);
    void AddForwardDeclaration(const string& cls,
                               const string& ns = NcbiEmptyString);
    void AddHPPIncludes(const TIncludes& includes);
    void AddCPPIncludes(const TIncludes& includes);
    void AddForwardDeclarations(const TForwards& forwards);

    void GenerateHPP(const string& path) const;
    void GenerateCPP(const string& path) const;

    const CNcbiRegistry& GetRegistry(void) const
        {
            return m_Registry;
        }
    operator const CNcbiRegistry&(void) const
        {
            return m_Registry;
        }
    
private:
    string m_HeaderPrefix;
    string m_HeaderName;
    string m_UserHeaderName;
    string m_CPPName;
    string m_HeaderDefineName;

    TIncludes m_HPPIncludes;
    TIncludes m_CPPIncludes;
    TForwards m_ForwardDeclarations;

    // classes code
    TClasses m_Classes;

    // current type state
    const CNcbiRegistry& m_Registry;
};

class CClassCode
{
public:
    typedef CFileCode::TIncludes TIncludes;
    typedef CFileCode::TForwards TForwards;
    typedef set<string> TEnums;

    CClassCode(CFileCode& file, const ASNType* type);
    ~CClassCode(void);
    
    const CNcbiRegistry& GetRegistry(void) const
        {
            return m_Code.GetRegistry();
        }
    operator const CNcbiRegistry&(void) const
        {
            return m_Code.GetRegistry();
        }

    const ASNType* GetType(void) const
        {
            return m_Type;
        }

    const string& GetVar(const string& value) const;

    const string& GetNamespace(void) const
        {
            return m_Namespace;
        }
    const string& GetClassName(void) const
        {
            return m_ClassName;
        }

    const ASNType* GetParentType(void) const;
    string GetParentClass(void) const;

    void SetAbstract(bool abstract = true)
        {
            m_Abstract = abstract;
        }
    bool IsAbstract(void) const
        {
            return m_Abstract;
        }

    void AddHPPInclude(const string& s)
        {
            m_Code.AddHPPInclude(s);
        }
    void AddCPPInclude(const string& s)
        {
            m_Code.AddCPPInclude(s);
        }
    void AddForwardDeclaration(const string& s, const string& ns)
        {
            m_Code.AddForwardDeclaration(s, ns);
        }
    void AddHPPIncludes(const TIncludes& includes)
        {
            m_Code.AddHPPIncludes(includes);
        }
    void AddCPPIncludes(const TIncludes& includes)
        {
            m_Code.AddCPPIncludes(includes);
        }
    void AddForwardDeclarations(const TForwards& forwards)
        {
            m_Code.AddForwardDeclarations(forwards);
        }
    void AddEnum(const string& enumDef)
        {
            m_Enums.insert(enumDef);
        }

    void AddParentType(CClassCode* parent);

    CNcbiOstream& HPP(void)
        {
            return m_HPP;
        }

    CNcbiOstream& CPP(void)
        {
            return m_CPP;
        }

    CNcbiOstream& GenerateHPP(CNcbiOstream& header) const;
    CNcbiOstream& GenerateCPP(CNcbiOstream& code) const;

private:
    CFileCode& m_Code;
    const ASNType* m_Type;
    set<CClassCode*> m_ParentTypes;
    string m_Namespace;
    string m_ClassName;
    string m_ParentClass;
    bool m_Abstract;

    mutable CNcbiOstrstream m_HPP;
    mutable CNcbiOstrstream m_CPP;
    TEnums m_Enums;
};

class CNamespace
{
public:
    CNamespace(CNcbiOstream& out);
    ~CNamespace(void);
    
    void Set(const string& ns);

    void End(void);

private:
    void Begin(void);

    CNcbiOstream& m_Out;
    string m_Namespace;
};

#endif
