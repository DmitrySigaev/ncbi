#ifndef __COL_VALIDATOR_HPP_INCLUDED__
#define __COL_VALIDATOR_HPP_INCLUDED__

#include <corelib/ncbiutil.hpp>

BEGIN_NCBI_SCOPE

class CTempString;

class CColumnValidator
{
protected:
    CColumnValidator();
public:
    virtual ~CColumnValidator();
    virtual bool DoValidate(const CTempString& value, string& error) const = 0;
};

class CColumnValidatorRegistry
{
protected:
    CColumnValidatorRegistry();
    ~CColumnValidatorRegistry();
public:
    static CColumnValidatorRegistry& GetInstance();
    void Register(const CTempString& name, CColumnValidator* val);
    void Register(const CTempString& name, const CTempString& alias);
    void UnRegister(CColumnValidator* val);

    bool IsSupported(const string& datatype) const; 
    bool DoValidate(const string& datatype, const CTempString& value, string& error) const;
private:
    map<string, CColumnValidator*> m_registry;
};


END_NCBI_SCOPE

#endif
