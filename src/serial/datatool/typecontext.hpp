#ifndef TYPECONTEXT_HPP
#define TYPECONTEXT_HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <serial/typeref.hpp>

BEGIN_NCBI_SCOPE

class CNcbiRegistry;

END_NCBI_SCOPE

USING_NCBI_SCOPE;

class ASNModule;
class ASNType;

class CFilePosition
{
public:
    CFilePosition(const string& fileName = "<STDIN>", int line = 0)
        : m_FileName(fileName), m_FileLine(line)
        {
        }
    CFilePosition(const CFilePosition& pos, int line)
        : m_FileName(pos.m_FileName), m_FileLine(line)
        {
        }
    
    const string& GetFileName(void) const
        {
            return m_FileName;
        }
    int GetFileLine(void) const
        {
            return m_FileLine;
        }
    string ToString(void) const;

private:
    string m_FileName;
    int m_FileLine;
};

CNcbiOstream& operator<<(CNcbiOstream& out, const CFilePosition& filePos);

class CConfigPosition
{
public:
    CConfigPosition(void)
        : m_ParentType(0)
        {
        }
    CConfigPosition(const CConfigPosition& base,
                    const string& member);
    CConfigPosition(const CConfigPosition& base,
                    const ASNType* type, const string& member);

    const ASNType* GetParentType(void) const
        {
            return m_ParentType;
        }
    const string& GetCurrentMember(void) const
        {
            return m_CurrentMember;
        }
    const string& GetSection(void) const
        {
            return m_Section;
        }
    const string& GetKeyPrefix(void) const
        {
            return m_KeyPrefix;
        }
    string AppendKey(const string& value) const;

    const string& GetVar(const CNcbiRegistry& reg,
                         const string& defSection, const string& value) const;

private:
    const ASNType* m_ParentType;
    string m_CurrentMember;
    string m_Section;
    string m_KeyPrefix;
};

class CDataTypeContext
{
public:
    CDataTypeContext(const CFilePosition& pos)
        : m_Module(0), m_FilePos(pos)
        {
        }
    CDataTypeContext(const CDataTypeContext& base, ASNModule& module)
        : m_Module(&module), m_FilePos(base), m_ConfigPos(base)
        {
        }
    CDataTypeContext(const CDataTypeContext& base, int line)
        : m_Module(base.m_Module), m_FilePos(base, line), m_ConfigPos(base)
        {
        }
    // start new base type or new member
    CDataTypeContext(const CDataTypeContext& base, const string& member)
        : m_Module(base.m_Module), m_FilePos(base), m_ConfigPos(base, member)
        {
        }
    // start new subtype
    CDataTypeContext(const CDataTypeContext& base, const ASNType* type,
                     const string& member = NcbiEmptyString)
        : m_Module(base.m_Module), m_FilePos(base),
          m_ConfigPos(base, type, member)
        {
        }
    
    ASNModule& GetModule(void) const
        {
            _ASSERT(m_Module);
            return *m_Module;
        }
    operator const CFilePosition&(void) const
        {
            return m_FilePos;
        }
    operator CFilePosition&(void)
        {
            return m_FilePos;
        }
    const CFilePosition& GetFilePos(void) const
        {
            return m_FilePos;
        }
    CFilePosition& GetFilePos(void)
        {
            return m_FilePos;
        }
    operator const CConfigPosition&(void) const
        {
            return m_ConfigPos;
        }
    operator CConfigPosition&(void)
        {
            return m_ConfigPos;
        }
    const CConfigPosition& GetConfigPos(void) const
        {
            return m_ConfigPos;
        }
    CConfigPosition& GetConfigPos(void)
        {
            return m_ConfigPos;
        }
    
private:
    ASNModule* m_Module;
    CFilePosition m_FilePos;
    CConfigPosition m_ConfigPos;
};

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
    CAnyTypeSource(ASNType* type)
        : m_Type(type)
        {
        }

    TTypeInfo GetTypeInfo(void);

private:
    ASNType* m_Type;
};

#endif
