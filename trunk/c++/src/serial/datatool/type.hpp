#ifndef TYPE_HPP
#define TYPE_HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <serial/typeref.hpp>
#include "autoptr.hpp"
#include <set>

BEGIN_NCBI_SCOPE

class CTypeInfo;

END_NCBI_SCOPE

USING_NCBI_SCOPE;

class CDataType;
class CDataTypeModule;
class CDataValue;
class CChoiceDataType;
class CClassCode;
class CTypeStrings;

typedef int TInteger;

struct AnyType {
    union {
        bool booleanValue;
        TInteger integerValue;
        double realValue;
        void* pointerValue;
    };
    AnyType(void)
        {
            pointerValue = 0;
        }
};

class CAnyTypeSource : public CTypeInfoSource
{
public:
    CAnyTypeSource(CDataType* type)
        : m_Type(type)
        {
        }

    TTypeInfo GetTypeInfo(void);

private:
    CDataType* m_Type;
};

class CDataType {
public:
    typedef void* TObjectPtr;

    CDataType(void);
    virtual ~CDataType(void);

    const CDataType* GetParentType(void) const
        {
            return m_ParentType;
        }
    const CDataTypeModule* GetModule(void) const
        {
            _ASSERT(m_Module != 0);
            return m_Module;
        }

    const string& GetSourceFileName(void) const;
    int GetSourceLine(void) const
        {
            return m_SourceLine;
        }
    void SetSourceLine(int line);
    string LocationString(void) const;

    string GetKeyPrefix(void) const;
    string IdName(void) const;
    bool Skipped(void) const;
    string ClassName(void) const;
    string FileName(void) const;
    string Namespace(void) const;
    string InheritFromClass(void) const;
    const CDataType* InheritFromType(void) const;
    const string& GetVar(const string& value) const;

    bool InChoice(void) const;

    virtual void PrintASN(CNcbiOstream& out, int indent) const = 0;

    virtual const CTypeInfo* GetTypeInfo(void);

    static CNcbiOstream& NewLine(CNcbiOstream& out, int indent);

    void Warning(const string& mess) const;

    virtual void GenerateCode(CClassCode& code) const;

    virtual void GetCType(CTypeStrings& tType, CClassCode& code) const;
    virtual string GetDefaultString(const CDataValue& value) const;

    virtual const CDataType* Resolve(void) const;
    virtual CDataType* Resolve(void);

    bool InSet(void) const
        {
            return m_InSet;
        }
    virtual void SetInSet(void);
    virtual void SetInChoice(const CChoiceDataType* choice);
    const set<const CChoiceDataType*>& GetChoices(void) const
        {
            return m_Choices;
        }

    static string Identifier(const string& typeName, bool capitalize = true);
    static string GetTemplateHeader(const string& tmpl);
    static bool IsSimpleTemplate(const string& tmpl);
    static string GetTemplateNamespace(const string& tmpl);
    static string GetTemplateMacro(const string& tmpl);

    void SetParent(const CDataType* parent, const string& memberName);
    void SetParent(const CDataTypeModule* module, const string& typeName);
    virtual void FixTypeTree(void) const;

    bool Check(void);
    virtual bool CheckType(void) const;
    virtual bool CheckValue(const CDataValue& value) const = 0;
    virtual TObjectPtr CreateDefault(const CDataValue& value) const = 0;
    virtual CTypeInfo* CreateTypeInfo(void);

private:
    const CDataType* m_ParentType;       // parent type
    const CDataTypeModule* m_Module;
    string m_MemberName;
    int m_SourceLine;

    // tree info
    set<const CChoiceDataType*> m_Choices;
    bool m_InSet;
    bool m_Checked;

    AutoPtr<CTypeInfo> m_CreatedTypeInfo;
};

#define CheckValueType(value, type, name) do{ \
if ( dynamic_cast<const type*>(&(value)) == 0 ) { \
    (value).Warning(name " value expected"); return false; \
} } while(0)

#endif
