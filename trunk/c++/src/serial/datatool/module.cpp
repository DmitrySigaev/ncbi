#include <module.hpp>
#include <type.hpp>

void Warning(const string& message)
{
    cerr << message << endl;
}

ASNModule::ASNModule()
{
}

ASNModule::~ASNModule()
{
}

void ASNModule::AddDefinition(const string& def, const AutoPtr<ASNType>& type)
{
    definitions.push_back(make_pair(def, type));
}

ostream& ASNModule::Print(ostream& out) const
{
    out << name << " DEFINITIONS ::=" << endl;
    out << "BEGIN" << endl << endl;

    if ( !exports.empty() ) {
        out << "EXPORTS ";
        for ( TExports::const_iterator i = exports.begin();
              i != exports.end(); ++i ) {
            if ( i != exports.begin() )
                out << ", ";
            out << *i;
        }
        out << ';' << endl << endl;
    }

    if ( !imports.empty() ) {
        out << "IMPORTS ";
        for ( TImports::const_iterator m = imports.begin();
              m != imports.end(); ++m ) {
            if ( m != imports.begin() )
                out << endl << "        ";

            const Import& imp = **m;
            for ( list<string>::const_iterator i = imp.types.begin();
                  i != imp.types.end(); ++i ) {
                if ( i != imp.types.begin() )
                    out << ", ";
                out << *i;
            }
            out << " FROM " << imp.module;
        }
        out << ';' << endl << endl;
    }

    for ( TDefinitions::const_iterator i = definitions.begin();
          i != definitions.end(); ++i ) {
        out << i->first << " ::= "; i->second->Print(cout, 0);
        out << endl << endl;
    }

    return out << "END" << endl << endl;
}

bool ASNModule::Check()
{
    bool ok = CheckNames();
    for ( TDefinitions::const_iterator d = definitions.begin();
          d != definitions.end(); ++d ) {
        if ( !d->second->Check(*this) )
            ok = false;
    }
    return ok;
}

const ASNModule::TypeInfo* ASNModule::FindType(const string& type) const
{
    TTypes::const_iterator t = types.find(type);
    if ( t == types.end() )
        return 0;
    else
        return &t->second;
}

bool ASNModule::CheckNames()
{
    bool ok = true;
    types.clear();
    for ( TDefinitions::const_iterator d = definitions.begin();
          d != definitions.end(); ++d ) {
        const string& n = d->first;
        pair< TTypes::iterator, bool > ins =
            types.insert(TTypes::value_type(n, n));
        if ( !ins.second ) {
            Warning("duplicated type: " + n);
            ok = false;
        }
        else {
            ins.first->second.type = d->second.get();
        }
    }
    for ( TExports::const_iterator e = exports.begin();
          e != exports.end(); ++e ) {
        const string& n = *e;
        if ( types.find(n) == types.end() ) {
            Warning("undefined export type: " + n);
            ok = false;
        }
    }
    for ( TImports::const_iterator i = imports.begin();
          i != imports.end(); ++i ) {
        const Import& imp = **i;
        const string& module = imp.module;
        for ( list<string>::const_iterator t = imp.types.begin();
              t != imp.types.end(); ++t ) {
            const string& n = *t;
            pair< TTypes::iterator, bool > ins =
                types.insert(TTypes::value_type(n, n));
            if ( !ins.second ) {
                Warning("duplicated import: " + n);
                ok = false;
            }
            else {
                ins.first->second.module = module;
            }
        }
    }
    return ok;
}
