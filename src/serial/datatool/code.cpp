#include "code.hpp"
#include "filecode.hpp"
#include "type.hpp"
#include "blocktype.hpp"

static inline
void
Write(CNcbiOstream& out, const CNcbiOstrstream& source)
{
    size_t size = source.pcount();
    if ( size != 0 ) {
        out.write(const_cast<CNcbiOstrstream&>(source).str(), size);
        const_cast<CNcbiOstrstream&>(source).freeze(false);
    }
}

CClassCode::CClassCode(CFileCode& code,
                       const string& typeName, const CDataType* type)
    : m_Code(code), m_TypeName(typeName), m_Type(type), m_ClassType(eNormal)
{
    m_Namespace = type->Namespace();
    m_ClassName = type->ClassName();
    string include = type->GetVar("_include");
    if ( !include.empty() )
        code.AddHPPInclude(include);
    type->GenerateCode(*this);
}

CClassCode::~CClassCode(void)
{
}

void CClassCode::SetClassType(EClassType type)
{
    _ASSERT(GetClassType() == eNormal);
    m_ClassType = type;
}

void CClassCode::AddHPPInclude(const string& s)
{
    m_Code.AddHPPInclude(s);
}

void CClassCode::AddCPPInclude(const string& s)
{
    m_Code.AddCPPInclude(s);
}

void CClassCode::AddForwardDeclaration(const string& s, const string& ns)
{
    m_Code.AddForwardDeclaration(s, ns);
}

void CClassCode::AddHPPIncludes(const TIncludes& includes)
{
    m_Code.AddHPPIncludes(includes);
}

void CClassCode::AddCPPIncludes(const TIncludes& includes)
{
    m_Code.AddCPPIncludes(includes);
}

void CClassCode::AddForwardDeclarations(const TForwards& forwards)
{
    m_Code.AddForwardDeclarations(forwards);
}

const CDataType* CClassCode::GetParentType(void) const
{
    const CDataType* t = GetType();
    const CDataType* parent = t->InheritFromType();
    if ( !parent ) {
        switch ( t->GetChoices().size() ) {
        case 0:
            // no parent type
            return 0;
        case 1:
            parent = *(t->GetChoices().begin());
            break;
        default:
            return 0;
        }
    }
    if ( parent ) {
        m_Code.AddHPPInclude(parent->FileName());
    }
    return parent;
}

string CClassCode::GetParentClass(void) const
{
    const CDataType* parent = GetParentType();
    if ( parent )
        return parent->ClassName();
    else
        return GetType()->InheritFromClass();
}

CNcbiOstream& CClassCode::GenerateHPP(CNcbiOstream& header) const
{
    if ( GetClassType() == eEnum ) {
        // enum
        Write(header, m_ClassPublic);
        return header;
    }
    header <<
        "class " << GetClassName() << "_Base";
    if ( !GetParentClass().empty() )
        header << " : public " << GetParentClass();
    header << NcbiEndl <<
        '{' << NcbiEndl <<
        "public:" << NcbiEndl <<
        "    " << GetClassName() << "_Base(void);" << NcbiEndl <<
        "    " << (GetClassType() == eAbstract? "virtual ": "") << '~' <<
        GetClassName() << "_Base(void);" << NcbiEndl <<
        NcbiEndl;
    if ( GetClassType() != eAlias ) {
        header <<
            "    static const NCBI_NS_NCBI::CTypeInfo* GetTypeInfo(void);" <<
            NcbiEndl;
    }
    Write(header, m_ClassPublic);
    header << NcbiEndl <<
        "private:" << NcbiEndl <<
        "    friend class " << GetClassName() << ';' << NcbiEndl <<
        NcbiEndl;
    Write(header, m_ClassPrivate);
    header <<
        "};" << NcbiEndl;
    return header;
}

CNcbiOstream& CClassCode::GenerateCPP(CNcbiOstream& code) const
{
    if ( GetClassType() == eEnum ) {
        // enum
        Write(code, m_Methods);
        return code;
    }
    code <<
        GetClassName() << "_Base::" <<
        GetClassName() << "_Base(void)" << NcbiEndl <<
        '{' << NcbiEndl <<
        '}' << NcbiEndl <<
        NcbiEndl <<
        GetClassName() << "_Base::~" <<
        GetClassName() << "_Base(void)" << NcbiEndl <<
        '{' << NcbiEndl <<
        '}' << NcbiEndl <<
        NcbiEndl;
    Write(code, m_Methods);
    if ( GetClassType() != eAlias ) {
        if ( GetClassType() == eAbstract )
            code << "BEGIN_ABSTRACT_CLASS_INFO3(\"";
        else
            code << "BEGIN_CLASS_INFO3(\"";
        code << GetType()->IdName() << "\", " <<
            GetClassName() << ", " << GetClassName() << "_Base)" << NcbiEndl <<
            '{' << NcbiEndl;
        if ( GetParentType() ) {
            code << "    SET_PARENT_CLASS(" <<
                GetParentClass() << ")->SetOptional();" << NcbiEndl;
        }
        code << NcbiEndl;
        Write(code, m_TypeInfoBody);
        code <<
            '}' << NcbiEndl <<
            "END_CLASS_INFO" << NcbiEndl;
    }
    return code;
}

CNcbiOstream& CClassCode::GenerateUserHPP(CNcbiOstream& header) const
{
    if ( GetClassType() == eEnum ) {
        // enum
        return header;
    }
    header <<
        "class " << GetClassName() << " : public " <<
        GetClassName() << "_Base" << NcbiEndl <<
        '{' << NcbiEndl <<
        "public:" << NcbiEndl <<
        "    " << GetClassName() << "();" << NcbiEndl <<
        "    " << '~' << GetClassName() << "();" << NcbiEndl <<
        NcbiEndl <<
        "};" << NcbiEndl;
    return header;
}

CNcbiOstream& CClassCode::GenerateUserCPP(CNcbiOstream& code) const
{
    if ( GetClassType() == eEnum ) {
        // enum
        return code;
    }
    code <<
        GetClassName() << "::" <<
        GetClassName() << "()" << NcbiEndl <<
        '{' << NcbiEndl <<
        '}' << NcbiEndl <<
        NcbiEndl <<
        GetClassName() << "::~" <<
        GetClassName() << "()" << NcbiEndl <<
        '{' << NcbiEndl <<
        '}' << NcbiEndl <<
        NcbiEndl;
    return code;
}

