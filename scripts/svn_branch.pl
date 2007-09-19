#!/usr/bin/perl -w
# $Id$

use strict;

my ($ScriptDir, $ScriptName);

BEGIN
{
    ($ScriptDir, $ScriptName) = $0 =~ m/^(?:(.+)[\\\/])?(.+)$/;
    $ScriptDir ||= '.'
}

use lib $ScriptDir;

use NCBI::SVN::Branching;

use Getopt::Long qw(:config permute no_getopt_compat no_ignore_case);

# Command line options.
my ($Help, $Force, $RootURL, $Revision, $PathList);

my @OptionInfo =
(
    ['help|h|?', help => \$Help, {}],

    ['force|f', force => \$Force,
        {alter => 1, remove => 1}, <<'EOF'],
  -f [--force]          : Force operation to run.
EOF

    ['root-url|root|U=s', root => \$RootURL,
        {list => 1, info => 1, create => 1, alter => 1, remove => 1}, <<'EOF'],
  -U [--root-url] arg   : Override repository root URL otherwise
                          detected from the working copy directory.
EOF

    ['revision|rev|r=i', revision => \$Revision,
        {create => 1, merge_down => 1, merge_up => 1}, <<'EOF'],
  -r [--revision] arg   : Source revision number (HEAD by default).
EOF

    ['list|l=s', list => \$PathList,
        {create => 1, alter => 1}, <<'EOF']
  -l [--list] arg       : Pathname of the file containing newline
                          separated list of branch elements. If this
                          option is used, no branch elements may be
                          specified in the command line.
EOF
);

my %CommandHelp =
(
    help => <<EOF,
help: Describe the usage of this program or its commands.

Usage: $ScriptName help [COMMAND ...]

Examples:
  1. Display help for the 'create' command:

    $ScriptName help create

  2. Display help for both 'merge_down' and 'merge_up' commands:

    $ScriptName help merge_down merge_up
EOF

    list => <<EOF,
list: Print the list of branches that were created using this utility.

Usage: $ScriptName list

Example:
  $ScriptName -U http://example.org/repos/example list
EOF

    info => <<EOF,
info: Display brief information about a branch.

Usage: $ScriptName info BRANCH_PATH

This command displays branch structure, its parent path, and the
history of 'merge_down' and 'merge_up' operations performed on
this branch.

Example:
  $ScriptName -U http://example.org/repos/example info new_feat
EOF
    create => <<EOF,
create: Create a new managed branch.

Usage: $ScriptName create BRANCH_PATH PARENT_PATH [BRANCH_ELEMENT ...]

This command branches out one or more directories and/or files located
under PARENT_PATH into BRANCH_PATH. These directories and files
(further referred to as branch elements) are defined by their
pathnames relative to PARENT_PATH.

BRANCH_PATH is the base path of the branch. It is used to identify the
branch in the repository. If BRANCH_PATH path doesn't exist, it is
created. Otherwise, it is removed and then created anew.

If BRANCH_PATH starts with a slash, 'branches/' or 'trunk/', it is
considered to be an absolute path. Otherwise, it is considered to be
relative to the '/branches' directory of the repository.

Branch elements can be listed either in the command line or in the
file specified by the '--list' option.

Note that PARENT_PATH can point to any part of the repository, in
particular, other branches.

Examples:
    $ScriptName create app_new_feat trunk/app include src

    $ScriptName create app_new_gui branches/app_new_feat src/gui
EOF

    alter => <<EOF,
alter: Modify the structure of an existing branch.

Usage: $ScriptName alter BRANCH_PATH [BRANCH_ELEMENT ...]

This command modifies the structure of an existing managed branch in
such a way that it looks as if it was initially created by a 'create'
command with the same list of branch elements. This implies that the
original source revision number will be used (that is, the revision
number that was used by the branch creation operation). If the branch
was ever synchronized with its parent path, the source revision number
of the last 'merge_down' command will be used instead.

Note that this command will require '--force' to be specified if the
requested change eliminates any of the existing branch elements.

Example:
    $ScriptName create app_new_gui branches/app_new_feat src/gui
    $ScriptName alter app_new_gui src/gui include/gui
EOF

    remove => <<EOF,
remove: Remove a managed branch.

Usage: $ScriptName remove BRANCH_PATH

This command removes the branch identified by the BRANCH_PATH argument
from the repository. Together with the branch elements themselves, all
empty directories that may appear as a result of the removal, will be
removed as well.

Note that this operation will require the '--force' option if not all
recent changes were merged into the parent stream.
EOF

    merge_down => <<EOF,
merge_down: Synchronize the branch with the parent stream.

Usage: $ScriptName merge_down BRANCH_PATH

This command must be executed from a working copy "switched" to
the branch identified by the BRANCH_PATH argument. There must be
no local changes in directories and files belonging to the branch
before running this command.

Once the merge results are reviewed and possible conflicts are
resolved, the results of the 'merge_down' operation can be
committed with the help of the 'commit_merge' command.
EOF

    merge_up => <<EOF,
merge_up: Promote branch changes to the parent stream.

Usage: $ScriptName merge_up BRANCH_PATH

Before the merge, the parent stream must be checked out to the
current working directory and there must be no local changes in it.

The results of the merge can be committed with the help of the
'commit_merge' command.
EOF

    commit_merge => <<EOF,
commit_merge: Commit the results of 'merge_down' or 'merge_up'.

Usage: $ScriptName commit_merge BRANCH_PATH

This command commits local changes made by a merge operation to the
repository. A special format of the commit log message is used.
EOF

    merge_stat => <<EOF,
merge_stat: Sanitize 'svn stat' output after 'merge_down' or 'merge_up'.

Usage: $ScriptName merge_stat BRANCH_PATH

Reviewing the output of 'svn stat' run on the results of a 'merge_down'
or 'merge_up' operation can be very inconvenient because each of these
operations sets a special Subversion property on each and every file and
directory. Thus, the 'svn stat' output gets bloated with property changes.
The 'merge_stat' command leaves out all property changes resulting in a
clearer 'svn stat' output.
EOF

    merge_diff => <<EOF,
merge_diff: Sanitize 'svn diff' output after 'merge_down' or 'merge_up'.

Usage: $ScriptName merge_diff BRANCH_PATH

This command leaves out changes of the 'ncbi:raw' property from the output
of the 'svn diff' command run on the results of a 'merge_down' or 'merge_up'
operation thus making the diff readable.
EOF

    svn => <<EOF,
svn: Execute an arbitrary Subversion command.

Usage: $ScriptName svn SVN_COMMAND -- [SVN_ARG ...] BRANCH_PATH

This directory executes a Subversion command with branch element
pathnames as its trailing arguments.

Example:
  $ScriptName svn revert -- -R new_feat

  In this example, the 'svn revert' command is executed on all files
  and directories that form the 'new_feat' branch.
EOF

    update => <<EOF,
update: Update branch directories from the repository.

Usage: $ScriptName update BRANCH_PATH

This command updates working copy directories of the branch
identified by BRANCH_PATH. It automatically detects if these
directories are switched to the branch or its parent stream
and maintains this property for newly added branch elements.
EOF

    switch => <<EOF,
switch: Switch working copy directories to a branch.

Usage: $ScriptName switch BRANCH_PATH

This command switches working copy directories to the branch
identified by the BRANCH_PATH argument. If the working copy
directories are already switched to that branch, they get
updated to the latest version from the repository.
EOF

    unswitch => <<EOF,
unswitch: Switch WC to the parent stream of a branch.

Usage: $ScriptName unswitch BRANCH_PATH

This command switches working copy directories back to the
parent stream.
EOF
);

sub Help
{
    my (@Commands) = @_;

    unless (@Commands)
    {
        print <<EOF
Usage: $ScriptName <command> [options] [args]
NCBI branching and merging helper for Subversion.
Type '$ScriptName help <command>' for help on a specific command.

Available commands:

  Getting information about branches:
    list          - Print the list of managed branches.
    info          - Display brief information about a branch.

  Branch structure modification:
    create        - Create a new managed branch.
    alter         - Restructure an existing branch.
    remove        - Remove a branch.

  Operations on working copy:
    switch        - Switch working copy directories to a branch.
    unswitch      - Switch WC to the parent stream of a branch.
    update        - Update branch directories from the repository.

  Merging:
    merge_down    - Merge parent stream changes into a branch.
    merge_up      - Promote branch changes to the parent stream.
    commit_merge  - Commit either of the above merge command results.
    merge_stat    - Omit prop. changes from 'svn stat' on merge results.
    merge_diff    - Omit 'ncbi:raw' props from 'svn diff' on merge results.

  Miscellaneous:
    svn           - Run any Subversion command on branch directories.
    help          - Describe the usage of this program or its commands.

For additional information, see http://mini.ncbi.nih.gov/2jj
EOF
    }
    else
    {
        for my $Command (@Commands)
        {
            if (my $Help = $CommandHelp{$Command})
            {
                print "$Help\n";

                my $OptionHelp = '';

                for my $OptionInfo (@OptionInfo)
                {
                    $OptionHelp .= $OptionInfo->[4]
                        if $OptionInfo->[3]->{$Command}
                }

                print "Valid options:\n$OptionHelp\n" if $OptionHelp
            }
            else
            {
                print "'$Command': unknown command.\n\n"
            }
        }
    }

    exit 0
}

sub UsageError
{
    my ($Error) = @_;

    print STDERR ($Error ? "$ScriptName\: $Error\n" : '') .
        "Type '$ScriptName help' for usage.\n";

    exit 1
}

sub TooManyArgs
{
    my ($Command) = @_;

    UsageError("too many arguments for '$Command'")
}

# Parse the command line.
GetOptions(map {@$_[0, 2]} @OptionInfo) or UsageError();

# Command is the first non-option argument.
my $Command = shift @ARGV;

unless (defined $Command)
{
    $Help ? Help() : UsageError()
}
else
{
    # Allow only options accepted by $Command to be
    # specified in the command line.
    for (@OptionInfo)
    {
        my (undef, $Option, $OptionValueRev, $CommandsThatAcceptIt) = @$_;

        UsageError("command '$Command' doesn't accept option '--$Option'")
            if defined $$OptionValueRev && !$CommandsThatAcceptIt->{$Command}
    }
}

# Instantiate the Branching module, which actually does the work.
my $Module = NCBI::SVN::Branching->new(MyName => $ScriptName);

sub TrimSlashes
{
    my ($PathRef) = @_;

    $$PathRef =~ s/\/+$//o;
    $$PathRef =~ s/^\/+//o
}

sub AdjustBranchPath
{
    my ($BranchPath) = @_;

    unless ($BranchPath =~ m/^(?:\/|trunk|branches)/)
    {
        $BranchPath = 'branches/' . $BranchPath
    }

    TrimSlashes(\$BranchPath);

    return $BranchPath
}

sub ExtractBranchPathArg
{
    my ($ArgName) = @_;

    return AdjustBranchPath(shift(@ARGV) or
        UsageError(q(') . ($ArgName || 'branch_path') .
            q(' argument is missing)))
}

sub RequireRootURL
{
    return $RootURL || $Module->{SVN}->GetRootURL() ||
        die "$ScriptName\: chdir to a working copy or use --root-url\n"
}

sub AcceptOnlyBranchPathArg
{
    my ($Command) = @_;

    my $BranchPath = ExtractBranchPathArg();

    TooManyArgs($Command) if @ARGV;

    return $BranchPath
}

sub ReadFileLines
{
    my ($FH) = @_;

    my $Line;
    my @Lines;

    while (defined($Line = readline($FH)))
    {
        chomp $Line;
        next unless $Line;
        push @Lines, $Line
    }

    return @Lines
}

sub GetBranchDirArgs
{
    my ($Command, @AdditionalPathArgs) = @_;

    my @BranchPaths;

    unless ($PathList)
    {
        UsageError('directory listing is missing') unless @ARGV;

        @BranchPaths = @ARGV
    }
    else
    {
        TooManyArgs($Command) if @ARGV;

        if ($PathList eq '-')
        {
            @BranchPaths = ReadFileLines(\*STDIN)
        }
        else
        {
            open FILE, '<', $PathList or die "$ScriptName\: $PathList\: $!\n";

            @BranchPaths = ReadFileLines(\*FILE);

            close FILE
        }
    }

    for my $Path (@BranchPaths)
    {
        TrimSlashes(\$Path)
    }

    return (@AdditionalPathArgs, @BranchPaths)
}

if ($Command eq 'help')
{
    Help(@ARGV)
}
elsif ($Command eq 'list')
{
    UsageError(q('list' doesn't accept arguments)) if @ARGV;

    $Module->List(RequireRootURL())
}
elsif ($Command eq 'info')
{
    $Module->Info(RequireRootURL(), AcceptOnlyBranchPathArg($Command))
}
elsif ($Command eq 'create')
{
    my $BranchPath = ExtractBranchPathArg();
    my $UpstreamPath = ExtractBranchPathArg('upstream_path');

    $Module->Create(RequireRootURL(), $BranchPath,
        $UpstreamPath, $Revision, GetBranchDirArgs($Command))
}
elsif ($Command eq 'alter')
{
    my $BranchPath = ExtractBranchPathArg();

    $Module->Alter($Force, RequireRootURL(), $BranchPath,
        GetBranchDirArgs($Command))
}
elsif ($Command eq 'remove')
{
    $Module->Remove($Force, RequireRootURL(), AcceptOnlyBranchPathArg($Command))
}
elsif ($Command eq 'merge_down')
{
    $Module->MergeDown(AcceptOnlyBranchPathArg($Command), $Revision)
}
elsif ($Command eq 'merge_up')
{
    $Module->MergeUp(AcceptOnlyBranchPathArg($Command), $Revision)
}
elsif ($Command eq 'commit_merge')
{
    $Module->CommitMerge(AcceptOnlyBranchPathArg($Command))
}
elsif ($Command eq 'merge_diff')
{
    $Module->MergeDiff(AcceptOnlyBranchPathArg($Command))
}
elsif ($Command eq 'merge_stat')
{
    $Module->MergeStat(AcceptOnlyBranchPathArg($Command))
}
elsif ($Command eq 'update')
{
    $Module->Update(AcceptOnlyBranchPathArg($Command))
}
elsif ($Command eq 'switch')
{
    $Module->Switch(AcceptOnlyBranchPathArg($Command))
}
elsif ($Command eq 'unswitch')
{
    $Module->Unswitch(AcceptOnlyBranchPathArg($Command))
}
elsif ($Command eq 'svn')
{
    UsageError(q(not enough arguments for 'svn')) if @ARGV < 2;

    my $BranchPath = AdjustBranchPath(pop @ARGV);

    $Module->Svn($BranchPath, @ARGV)
}
else
{
    UsageError("unknown command '$Command'")
}

exit 0
