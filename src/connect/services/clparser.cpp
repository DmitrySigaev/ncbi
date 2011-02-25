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
 * Authors:  Dmitry Kazimirov
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/clparser.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE

#define VERSION_OPT_ID -1
#define HELP_OPT_ID -2
#define COMMAND_OPT_ID -3

#define HELP_CMD_ID -1

#define DEFAULT_HELP_TEXT_WIDTH 72
#define DEFAULT_CMD_DESCR_INDENT 24
#define DEFAULT_OPT_DESCR_INDENT 32

namespace {
    typedef list<string> TNameVariantList;

    struct SOptionOrCommand : public CObject
    {
        SOptionOrCommand(int id, const string& name_variants) : m_Id(id)
        {
            NStr::Split(name_variants, "|", m_NameVariants);
        }

        const string& GetPrimaryName() const {return m_NameVariants.front();}

        int m_Id;
        TNameVariantList m_NameVariants;
    };

    struct SOptionInfo : public SOptionOrCommand
    {
        SOptionInfo(int opt_id, const string& name_variants,
                CCommandLineParser::EOptionType type,
                const string& description) :
            SOptionOrCommand(opt_id, name_variants),
            m_Type(type),
            m_Description(description)
        {
        }

        static string AddDashes(const string& opt_name)
        {
            return opt_name.length() == 1 ?  '-' + opt_name : "--" + opt_name;
        }

        string GetNameVariants() const
        {
            string result(AddDashes(m_NameVariants.front()));
            if (m_NameVariants.size() > 1) {
                result.append(" [");
                TNameVariantList::const_iterator name(m_NameVariants.begin());
                result.append(AddDashes(*++name));
                while (++name != m_NameVariants.end()) {
                    result.append(", ");
                    result.append(AddDashes(*name));
                }
                result.push_back(']');
            }
            if (m_Type == CCommandLineParser::eOptionWithParameter)
                result.append(" ARG");
            return result;
        }

        int m_Type;
        string m_Description;
    };

    typedef list<const SOptionInfo*> TOptionInfoList;

    struct SCommandInfo : public SOptionOrCommand
    {
        SCommandInfo(int cmd_id, const string& name_variants,
                const string& synopsis, const string& usage) :
            SOptionOrCommand(cmd_id, name_variants),
            m_Synopsis(synopsis),
            m_Usage(usage)
        {
        }

        string GetNameVariants() const
        {
            if (m_NameVariants.size() == 1)
                return m_NameVariants.front();
            TNameVariantList::const_iterator name(m_NameVariants.begin());
            string result(*name);
            result.append(" (");
            result.append(*++name);
            while (++name != m_NameVariants.end()) {
                result.append(", ");
                result.append(*name);
            }
            result.push_back(')');
            return result;
        }

        string m_Synopsis;
        string m_Usage;

        TOptionInfoList m_PositionalArguments;
        TOptionInfoList m_AcceptedOptions;
    };
}

struct SCommandLineParserImpl : public CObject
{
    SCommandLineParserImpl(
        const string& program_name,
        const string& program_version,
        const string& program_description);

    void PrintWordWrapped(int topic_len, int indent,
        const string& text, int cont_indent = -1) const;
    void Help() const;
    void Throw(const string& error, const string& cmd = kEmptyStr);
    int ParseAndValidate(int argc, const char* const *argv);

    // Command and argument definitions.
    string m_ProgramName;
    string m_ProgramVersion;
    string m_ProgramDescription;
    typedef list<const SCommandInfo*> TCommandList;
    typedef map<string, const SOptionInfo*> TOptionNameToOptionInfoMap;
    typedef map<string, const SCommandInfo*> TCommandNameToCommandInfoMap;
    TCommandList m_Commands;
    const SOptionInfo* m_SingleLetterOptions[256];
    TOptionNameToOptionInfoMap m_OptionToOptInfoMap;
    map<int, CRef<SOptionInfo> > m_OptIdToOptionInfoMap;
    TCommandNameToCommandInfoMap m_CommandNameToCommandInfoMap;
    map<int, CRef<SCommandInfo> > m_CmdIdToCommandInfoMap;
    SOptionInfo m_VersionOption;
    SOptionInfo m_HelpOption;
    SOptionInfo m_CommandOption;
    SCommandInfo m_HelpCommand;

    // Parsing results.
    typedef pair<const SOptionInfo*, const char*> TOptionParameter;
    typedef list<TOptionParameter> TOptionList;
    TOptionList m_Options;
    TOptionList::const_iterator m_NextOption;

    // Help text formatting.
    int m_MaxHelpTextWidth;
    int m_CmdDescrIndent;
    int m_OptDescrIndent;
};

static const string s_Help("help");
static const string s_Version("version");

SCommandLineParserImpl::SCommandLineParserImpl(
        const string& program_name,
        const string& program_version,
        const string& program_description) :
    m_ProgramName(program_name),
    m_ProgramVersion(program_version),
    m_ProgramDescription(program_description),
    m_VersionOption(VERSION_OPT_ID, s_Version,
        CCommandLineParser::eSwitch, kEmptyStr),
    m_HelpOption(HELP_OPT_ID, s_Help,
        CCommandLineParser::eSwitch, kEmptyStr),
    m_CommandOption(COMMAND_OPT_ID, "COMMAND",
        CCommandLineParser::eZeroOrMorePositional, kEmptyStr),
    m_HelpCommand(HELP_CMD_ID, s_Help,
        "Describe the usage of this program or its commands.", kEmptyStr),
    m_MaxHelpTextWidth(DEFAULT_HELP_TEXT_WIDTH),
    m_CmdDescrIndent(DEFAULT_CMD_DESCR_INDENT),
    m_OptDescrIndent(DEFAULT_OPT_DESCR_INDENT)
{
    memset(m_SingleLetterOptions, 0, sizeof(m_SingleLetterOptions));
    m_OptionToOptInfoMap[s_Version] = &m_VersionOption;
    m_OptionToOptInfoMap[s_Help] = &m_HelpOption;
    m_HelpCommand.m_PositionalArguments.push_back(&m_CommandOption);
    m_CommandNameToCommandInfoMap[s_Help] = &m_HelpCommand;
}

void SCommandLineParserImpl::PrintWordWrapped(int topic_len,
    int indent, const string& text, int cont_indent) const
{
    if (text.empty()) {
        printf("\n");
        return;
    }

    const char* line = text.data();
    const char* text_end = line + text.length();

    int offset = indent;
    if (topic_len > 0 && (offset -= topic_len) < 1) {
        offset = indent;
        printf("\n");
    }

    if (cont_indent < 0)
        cont_indent = indent;

    const char* next_line;
    do {
        const char* line_end;
        // Check for verbatim formatting.
        if (*line != ' ') {
            const char* pos = line;
            const char* max_pos = line + m_MaxHelpTextWidth - indent;
            line_end = NULL;
            for (;;) {
                if (*pos == ' ') {
                    line_end = pos;
                    while (pos < text_end && pos[1] == ' ')
                        ++pos;
                    next_line = pos + 1;
                } else if (*pos == '\n') {
                    next_line = (line_end = pos) + 1;
                    break;
                }
                if (++pos > max_pos && line_end != NULL)
                    break;
                if (pos == text_end) {
                    next_line = line_end = pos;
                    break;
                }
            }
        } else {
            // Preformatted text -- do not wrap.
            line_end = strchr(line, '\n');
            next_line = (line_end == NULL ? line_end = text_end : line_end + 1);
        }
        int line_len = int(line_end - line);
        if (line_len > 0)
            printf("%*.*s\n", offset + line_len, line_len, line);
        else
            printf("\n");
        offset = indent = cont_indent;
    } while ((line = next_line) < text_end);
}

void SCommandLineParserImpl::Help() const
{
    if (m_Options.empty()) {
        printf("Usage: %s <command> [options] [args]\n", m_ProgramName.c_str());
        PrintWordWrapped(0, 0, m_ProgramDescription);
        printf("Type '%s help <command>' for help on a specific command.\n"
            "Type '%s --version' to see the program version.\n\n"
            "Available commands:\n\n",
            m_ProgramName.c_str(), m_ProgramName.c_str());
        ITERATE(TCommandList, cmd, m_Commands)
            PrintWordWrapped(printf("  %s", (*cmd)->GetNameVariants().c_str()),
                m_CmdDescrIndent - 2, "- " + (*cmd)->m_Synopsis,
                m_CmdDescrIndent);
        PrintWordWrapped(printf("  help"), m_CmdDescrIndent - 2,
            "- " + m_HelpCommand.m_Synopsis, m_CmdDescrIndent);
        printf("\n");
    } else {
        ITERATE(TOptionList, cmd_name, m_Options) {
            TCommandNameToCommandInfoMap::const_iterator cmd =
                m_CommandNameToCommandInfoMap.find(cmd_name->second);
            if (cmd == m_CommandNameToCommandInfoMap.end()) {
                printf("'%s': unknown command.\n\n", cmd_name->second);
                continue;
            }

            const SCommandInfo* command_info = cmd->second;
            string command_name(command_info->GetNameVariants());
            PrintWordWrapped(printf("%s:", command_name.c_str()),
                command_name.length() + 2, command_info->m_Synopsis);
            printf("\n");

            string args;
            ITERATE(TOptionInfoList, arg, command_info->m_PositionalArguments) {
                if (!args.empty())
                    args.push_back(' ');
                switch ((*arg)->m_Type) {
                case CCommandLineParser::ePositionalArgument:
                    args.append((*arg)->GetPrimaryName());
                    break;
                case CCommandLineParser::eOptionalPositional:
                    args.push_back('[');
                    args.append((*arg)->GetPrimaryName());
                    args.push_back(']');
                    break;
                case CCommandLineParser::eZeroOrMorePositional:
                    args.push_back('[');
                    args.append((*arg)->GetPrimaryName());
                    args.append("...]");
                    break;
                default: // always CCommandLineParser::eOneOrMorePositional
                    args.append((*arg)->GetPrimaryName());
                    args.append("...");
                }
            }
            int usage_len = printf("Usage: %s %s",
                m_ProgramName.c_str(), command_info->GetPrimaryName().c_str());
            PrintWordWrapped(usage_len, usage_len + 1, args);

            if (!command_info->m_Usage.empty()) {
                printf("\n");
                PrintWordWrapped(0, 0, command_info->m_Usage);
            }

            if (!command_info->m_AcceptedOptions.empty()) {
                printf("\nValid options:\n");
                ITERATE(TOptionInfoList, opt, command_info->m_AcceptedOptions)
                    PrintWordWrapped(printf("  %-*s :", m_OptDescrIndent - 5,
                        (*opt)->GetNameVariants().c_str()),
                            m_OptDescrIndent, (*opt)->m_Description);
            }
            printf("\n");
        }
    }
}

void SCommandLineParserImpl::Throw(const string& error, const string& cmd)
{
    string message(kEmptyStr);
    if (error.empty())
        message.append(m_ProgramDescription);
    else {
        message.append(m_ProgramName);
        message.append(": ");
        message.append(error);
    }
    message.append("\nType '");
    message.append(m_ProgramName);
    if (cmd.empty())
        message.append(" help' for usage.\n");
    else {
        message.append(" help ");
        message.append(cmd);
        message.append("' for usage.\n");
    }
    throw runtime_error(message);
}

int SCommandLineParserImpl::ParseAndValidate(int argc, const char* const *argv)
{
    if (m_ProgramName.empty()) {
        m_ProgramName = *argv;
        string::size_type basename_pos = m_ProgramName.find_last_of("/\\:");
        if (basename_pos != string::npos)
            m_ProgramName = m_ProgramName.substr(basename_pos + 1);
    }

    typedef list<const char*> TPositionalArgumentList;
    TPositionalArgumentList positional_arguments;

    // Part one: parsing.
    while (--argc > 0) {
        const char* arg = *++argv;

        // Check if the current argument is a positional argument.
        if (*arg != '-' || arg[1] == '\0')
            positional_arguments.push_back(arg);
        else {
            // No, it's an option. Check whether it's a
            // single-letter option or a long option.
            const SOptionInfo* option_info;
            const char* opt_param;
            if (*++arg == '-') {
                // It's a long option.
                // If it's a free standing double dash marker,
                // treat the rest of arguments as positional.
                if (*++arg == '\0') {
                    while (--argc > 0)
                        positional_arguments.push_back(*++argv);
                    break;
                }
                // Check if a parameter is specified for this option.
                opt_param = strchr(arg, '=');
                string opt_name;
                if (opt_param != NULL)
                    opt_name.assign(arg, opt_param++);
                else
                    opt_name.assign(arg);
                TOptionNameToOptionInfoMap::const_iterator opt_info(
                    m_OptionToOptInfoMap.find(opt_name));
                if (opt_info == m_OptionToOptInfoMap.end())
                    Throw("unknown option '--" + opt_name + "'");
                option_info = opt_info->second;
                // Check if this option must have a parameter.
                if (option_info->m_Type == CCommandLineParser::eSwitch) {
                    // No, it's a switch; it's not supposed to have a parameter.
                    if (opt_param != NULL)
                        Throw("option '--" + opt_name +
                            "' does not expect a parameter");
                    opt_param = "yes";
                } else
                    // The option expects a parameter.
                    if (opt_param == NULL) {
                        // Parameter is not specified; use the next
                        // command line argument as a parameter for
                        // this option.
                        if (--argc == 0)
                            Throw("option '--" + opt_name +
                                "' requires a parameter");
                        opt_param = *++argv;
                    }
            } else {
                // The current argument is a (series of) one-letter option(s).
                for (;;) {
                    char opt_letter = *arg++;
                    option_info = m_SingleLetterOptions[
                        (unsigned char) opt_letter];
                    if (option_info == NULL)
                        Throw(string("unknown option '-") + opt_letter + "'");

                    // Check if this option must have a parameter.
                    if (option_info->m_Type == CCommandLineParser::eSwitch) {
                        // It's a switch; it's not supposed to have a parameter.
                        opt_param = "yes";
                        if (*arg == '\0')
                            break;
                    } else {
                        // It's an option that expects a parameter.
                        if (*arg == '\0') {
                            // Use the next command line argument
                            // as a parameter for this option.
                            if (--argc == 0)
                                Throw(string("option '-") + opt_letter +
                                    "' requires a parameter");
                            opt_param = *++argv;
                        } else
                            opt_param = arg;
                        break;
                    }

                    m_Options.push_back(TOptionParameter(
                        option_info, opt_param));
                }
            }

            m_Options.push_back(TOptionParameter(option_info, opt_param));
        }
    }

    // Part two: validation.
    TOptionList::iterator option(m_Options.begin());
    while (option != m_Options.end())
        switch (option->first->m_Id) {
        case VERSION_OPT_ID:
            printf("%s version %s\n", m_ProgramName.c_str(),
                m_ProgramVersion.c_str());
            return HELP_CMD_ID;

        case HELP_OPT_ID:
            positional_arguments.push_front("help");
            m_Options.erase(option++);
            break;

        default:
            ++option;
        }

    if (positional_arguments.empty())
        if (!m_Options.empty())
            Throw("a command is required");
        else
            Throw(kEmptyStr);

    string command_name(positional_arguments.front());
    positional_arguments.pop_front();

    TCommandNameToCommandInfoMap::const_iterator command =
        m_CommandNameToCommandInfoMap.find(command_name);

    if (command == m_CommandNameToCommandInfoMap.end())
        Throw("unknown command '" + command_name + "'");

    const SCommandInfo* command_info = command->second;

    for (option = m_Options.begin(); option != m_Options.end(); ++option)
        if (find(command_info->m_AcceptedOptions.begin(),
                command_info->m_AcceptedOptions.end(), option->first) ==
                    command_info->m_AcceptedOptions.end())
            Throw("command '" + command_name + "' doesn't accept option '" +
                option->first->GetNameVariants() + "'", command_name);

    TPositionalArgumentList::const_iterator arg_value =
        positional_arguments.begin();
    TOptionInfoList::const_iterator expected_arg =
        command_info->m_PositionalArguments.begin();

    for (;;) {
        if (expected_arg != command_info->m_PositionalArguments.end())
            if (arg_value == positional_arguments.end())
                switch ((*expected_arg)->m_Type) {
                case CCommandLineParser::ePositionalArgument:
                case CCommandLineParser::eOneOrMorePositional:
                    Throw("missing argument '" +
                        (*expected_arg)->GetPrimaryName() + "'",
                            command_info->GetPrimaryName());
                }
            else
                switch ((*expected_arg)->m_Type) {
                case CCommandLineParser::ePositionalArgument:
                case CCommandLineParser::eOptionalPositional:
                    m_Options.push_back(TOptionParameter(
                        *expected_arg, *arg_value));
                    ++arg_value;
                    ++expected_arg;
                    continue;
                default:
                    do
                        m_Options.push_back(TOptionParameter(
                            *expected_arg, *arg_value));
                    while (++arg_value != positional_arguments.end());
                }
        else
            if (arg_value != positional_arguments.end())
                Throw("too many positional arguments");
        break;
    }

    m_NextOption = m_Options.begin();

    if (command_info->m_Id == HELP_CMD_ID) {
        Help();
        return HELP_CMD_ID;
    }

    return command_info->m_Id;
}

CCommandLineParser::CCommandLineParser(
        const string& program_name,
        const string& program_version,
        const string& program_description) :
    m_Impl(new SCommandLineParserImpl(
        program_name, program_version, program_description))
{
}

void CCommandLineParser::SetHelpTextMargins(int help_text_width,
    int cmd_descr_indent, int opt_descr_indent)
{
    m_Impl->m_MaxHelpTextWidth = help_text_width;
    m_Impl->m_CmdDescrIndent = cmd_descr_indent;
    m_Impl->m_OptDescrIndent = opt_descr_indent;
}

void CCommandLineParser::AddOption(CCommandLineParser::EOptionType type,
    int opt_id, const string& name_variants, const string& description)
{
    _ASSERT(opt_id >= 0 && m_Impl->m_OptIdToOptionInfoMap.find(opt_id) ==
        m_Impl->m_OptIdToOptionInfoMap.end() && "Option IDs must be unique");

    SOptionInfo* option_info = m_Impl->m_OptIdToOptionInfoMap[opt_id] =
        new SOptionInfo(opt_id, name_variants, type, description);

    switch (type) {
    default:
        _ASSERT(option_info->m_NameVariants.size() == 1 &&
            "Positional arguments do not allow name variants");
        break;

    case eSwitch:
    case eOptionWithParameter:
        ITERATE(TNameVariantList, name, option_info->m_NameVariants)
            if (name->length() == 1)
                m_Impl->m_SingleLetterOptions[
                    (unsigned char) name->at(0)] = option_info;
            else
                m_Impl->m_OptionToOptInfoMap[*name] = option_info;
    }
}

void CCommandLineParser::AddCommand(int cmd_id,
    const string& name_variants, const string& synopsis, const string& usage)
{
    _ASSERT(cmd_id >= 0 && m_Impl->m_CmdIdToCommandInfoMap.find(cmd_id) ==
        m_Impl->m_CmdIdToCommandInfoMap.end() && "Command IDs must be unique");

    SCommandInfo* command_info = m_Impl->m_CmdIdToCommandInfoMap[cmd_id] =
        new SCommandInfo(cmd_id, name_variants, synopsis, usage);

    m_Impl->m_Commands.push_back(command_info);

    ITERATE(TNameVariantList, name, command_info->m_NameVariants)
        m_Impl->m_CommandNameToCommandInfoMap[*name] = command_info;
}

void CCommandLineParser::AddAssociation(int cmd_id, int opt_id)
{
    _ASSERT(m_Impl->m_CmdIdToCommandInfoMap.find(cmd_id) !=
        m_Impl->m_CmdIdToCommandInfoMap.end() && "No such command ID");

    _ASSERT(m_Impl->m_OptIdToOptionInfoMap.find(opt_id) !=
        m_Impl->m_OptIdToOptionInfoMap.end() && "No such option ID");

    SCommandInfo* cmd_info = m_Impl->m_CmdIdToCommandInfoMap[cmd_id];
    SOptionInfo* opt_info = m_Impl->m_OptIdToOptionInfoMap[opt_id];

    switch (opt_info->m_Type) {
    case eSwitch:
    case eOptionWithParameter:
        cmd_info->m_AcceptedOptions.push_back(opt_info);
        break;

    default:
        _ASSERT("Invalid sequence of optional positional arguments" &&
            cmd_info->m_PositionalArguments.empty() ||
            cmd_info->m_PositionalArguments.back()->m_Type ==
                ePositionalArgument ||
            (cmd_info->m_PositionalArguments.back()->m_Type ==
                eOptionalPositional &&
                    opt_info->m_Type != ePositionalArgument));

        cmd_info->m_PositionalArguments.push_back(opt_info);
    }
}

int CCommandLineParser::Parse(int argc, const char* const *argv)
{
    return m_Impl->ParseAndValidate(argc, argv);
}

const string& CCommandLineParser::GetProgramName() const
{
    return m_Impl->m_ProgramName;
}

bool CCommandLineParser::NextOption(int* opt_id, const char** opt_value)
{
    if (m_Impl->m_NextOption == m_Impl->m_Options.end())
        return false;

    *opt_id = m_Impl->m_NextOption->first->m_Id;
    *opt_value = m_Impl->m_NextOption->second;

    ++m_Impl->m_NextOption;

    return true;
}

END_NCBI_SCOPE
