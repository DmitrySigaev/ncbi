#ifndef MSVC_PRJ_GENERATOR_HEADER
#define MSVC_PRJ_GENERATOR_HEADER

#include <app/project_tree_builder/proj_item.hpp>
#include "VisualStudioProject.hpp"
#include <app/project_tree_builder/msvc_project_context.hpp>

#include <corelib/ncbienv.hpp>

BEGIN_NCBI_SCOPE
//------------------------------------------------------------------------------

class CMsvcProjectGenerator
{
public:
    CMsvcProjectGenerator(const list<string>& configs);
    ~CMsvcProjectGenerator();
    
    bool Generate(const CProjItem& prj);

private:

    list<string> m_Configs;

    const string m_Platform;
    const string m_ProjectType;
    const string m_Version;
    const string m_Keyword;

    /// Helpers:
    static void CollectSources(const CProjItem& project,
                               const CMsvcPrjProjectContext& context,
                               list<string> * pRelPathes);


    static void CollectHeaders(const CProjItem& project,
                               const CMsvcPrjProjectContext& context,
                               list<string> * pRelPathes);

    static void CollectHeaderDirs(const CProjItem& project,
                               const CMsvcPrjProjectContext& context,
                               list<string> * pRelDirs);

    static void CollectInlines(const CProjItem& project,
                               const CMsvcPrjProjectContext& context,
                               list<string> * pRelPathes);

    /// Prohibited to.
    CMsvcProjectGenerator(void);
    CMsvcProjectGenerator(const CMsvcProjectGenerator&);
    CMsvcProjectGenerator& operator = (const CMsvcProjectGenerator&);

};

//------------------------------------------------------------------------------
END_NCBI_SCOPE

#endif // MSVC_PRJ_GENERATOR_HEADER