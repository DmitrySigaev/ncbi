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

sub Usage
{
    my ($Stream, $Status) = @_;

    print $Stream <<EOF;
Usage:
    $ScriptName list

    $ScriptName info <branch_path>

    $ScriptName create <branch_path> <upstream_path>[:<rev>] <branch_dirs>

    $ScriptName alter <branch_path> <branch_dirs>

    $ScriptName remove <branch_path>

    $ScriptName merge_down <branch_path> [<trunk_rev>]

    $ScriptName merge_down_commit <branch_path>

    $ScriptName merge_up <branch_path> [<branch_rev>]

    $ScriptName merge_up_commit <branch_path>

    $ScriptName svn <branch_path> <svn_command_with_args>

    $ScriptName switch <branch_path>

    $ScriptName unswitch <branch_path>

Where:
    branch_path         Path in the repository (relative to /branches)
                        that identifies the branch.

    trunk_rev           Trunk revision to operate with (HEAD by default).

Commands:
    list                Prints the list of branches that were created
                        with the help of this utility.

    info                Displays brief information about the branch
                        specified by the <branch_path> argument.

    create              Creates a new branch defined by its base path
                        <branch_path>, upstream path, and directory
                        listing <branch_dirs>.

    alter               Modifies branch structure bringing it to the state
                        as if it was initially created with directory
                        listing <branch_dirs>.

    remove              Removes branch identified by the path <branch_path>
                        from the repository.

    merge_down          Merges the latest changes from the trunk into the
                        branch identified by <branch_path>. The branch must
                        be checked out to the current working directory and
                        there must be no local changes in it.

    merge_up            Merges the latest changes from the branch back to the
                        trunk. The trunk stream must be checked out to the
                        current working directory and there must be no local
                        changes in it.

    commit_merge        Commits the results of merge_down or merge_up.
                        A special format of the commit log message is used.

    svn                 Executes an arbitrary Subversion command with branch
                        directory names as its arguments.

    switch              Switches working copy directories to the branch
                        identified by <branch_path>.

    unswitch            "Unswitches" working copy directories from the
                        branch identified by <branch_path>.

Description:
    This script facilitates branch handling using Subversion.

EOF

    exit $Status
}

my ($Command, @Params) = @ARGV;

my $Module = NCBI::SVN::Branching->new(MyName => $ScriptName);
my $Method;

unless (defined $Command)
{
    Usage(\*STDERR, 1)
}
elsif ($Command eq '--help' || $Command eq '-h')
{
    Usage(\*STDOUT, 0)
}
elsif ($Command eq 'list')
{
    $Method = 'List'
}
elsif ($Command eq 'info')
{
    $Method = 'Info'
}
elsif ($Command eq 'create')
{
    $Method = 'Create'
}
elsif ($Command eq 'alter')
{
    $Method = 'Alter'
}
elsif ($Command eq 'remove')
{
    $Method = 'Remove'
}
elsif ($Command eq 'merge_down')
{
    $Method = 'MergeDown'
}
elsif ($Command eq 'merge_up')
{
    $Method = 'MergeUp'
}
elsif ($Command eq 'commit_merge')
{
    $Method = 'CommitMerge'
}
elsif ($Command eq 'svn')
{
    $Method = 'Svn'
}
elsif ($Command eq 'switch')
{
    $Method = 'Switch'
}
elsif ($Command eq 'unswitch')
{
    $Method = 'Unswitch'
}
else
{
    die "$ScriptName\: unknown command '$Command'\n"
}

$Module->$Method(@Params);

exit 0
