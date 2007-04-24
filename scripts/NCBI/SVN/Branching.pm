# $Id$

package NCBI::SVN::Branching;

use base qw(NCBI::SVN::Base);

use strict;
use warnings;
use Carp qw(confess);

use File::Temp qw/tempfile/;

use NCBI::SVN::Wrapper;
use NCBI::SVN::SwitchMap;

my $TrunkDir = 'trunk/c++';

sub RetrieveRepositoryStructureContainingSubtree
{
    my ($Self, $SVN, $Path, $Subtree) = @_;

    my %Result;

    my $Subdir;

print "ls $Path\:\n";
    for my $Entry ($SVN->ReadSubversionLines('list', $Path))
    {
        $Entry =~ s/\/$//o;

        if (ref($Subdir = $Subtree->{$Entry}))
        {
            $Result{$Entry} = keys %$Subdir ?
                $Self->RetrieveRepositoryStructureContainingSubtree($SVN,
                    "$Path/$Entry", $Subdir) : {}
        }
        else
        {
            $Result{'/moreentries'} = 1
        }
    }

    return \%Result
}

sub GetRmCommandsR
{
    my ($RmCommands, $ExistingStructure, $DirName, $TreeToRemove, $Path) = @_;
print "Calc rm commands for path '$Path'...\n";
    if (my $ThisDirStructure = $ExistingStructure->{$DirName})
    {
        if (%$TreeToRemove)
        {
            my @RmCommands;

            while (my ($SubdirName, $Subdir) = each %$TreeToRemove)
            {
                GetRmCommandsR(\@RmCommands, $ThisDirStructure, $SubdirName, $Subdir, "$Path/$SubdirName")
            }

            if (keys %$ThisDirStructure)
            {
                push @$RmCommands, @RmCommands;
                return
            }
        }

        push @$RmCommands, 'rm', $Path;
        delete $ExistingStructure->{$DirName}
    }
}

sub GetRmCommands
{
    my ($RmCommands, $ExistingStructure, $TreeToRemove) = @_;
print "Calc rm commands for root...\n";
    while (my ($DirName, $Subdir) = each %$TreeToRemove)
    {
        GetRmCommandsR($RmCommands, $ExistingStructure, $DirName, $Subdir, $DirName)
    }
}

sub CreateEntireTree
{
    my ($MkdirCommands, $TreeToCreate, $Path) = @_;
print "Creating entire tree under '$Path'...\n";
    return unless %$TreeToCreate;

    push @$MkdirCommands, 'mkdir', $Path;

    while (my ($DirName, $Subdir) = each %$TreeToCreate)
    {
        CreateEntireTree($MkdirCommands, $Subdir, "$Path/$DirName")
    }
}

sub GetMkdirCommandsR
{
    my ($MkdirCommands, $ExistingStructure, $TreeToCreate, $Path) = @_;
print "Calc mkdir commands for path '$Path'...\n";
print "'$Path' does not exist...\n" unless $ExistingStructure;
    return CreateEntireTree($MkdirCommands, $TreeToCreate, $Path) unless $ExistingStructure;

    while (my ($DirName, $Subdir) = each %$TreeToCreate)
    {
        GetMkdirCommandsR($MkdirCommands, $ExistingStructure->{$DirName}, $Subdir, "$Path/$DirName")
    }
}

sub GetMkdirCommands
{
    my ($MkdirCommands, $ExistingStructure, $TreeToCreate) = @_;
print "Calc mkdir commands for root...\n";
    while (my ($DirName, $Subdir) = each %$TreeToCreate)
    {
        GetMkdirCommandsR($MkdirCommands, $ExistingStructure->{$DirName}, $Subdir, $DirName)
    }
}

sub LayPath
{
    my ($Path, @Subdirs) = @_;

print "Laying path '$Path'...\n";
    for my $DirName (split('/', $Path))
    {
        for my $Subdir (@Subdirs)
        {
            $Subdir = ($Subdir->{$DirName} ||= {})
        }
    }
}

sub Create
{
    my ($Self, $BranchPath, $BranchMapFile, $Revision) = @_;

    for ([branch_path => $BranchPath], [branch_map_file => $BranchMapFile])
    {
        die "$Self->{MyName}: <$_->[0]> parameter is missing\n" unless $_->[1]
    }

    $Revision ||= 'HEAD';

    my $SwitchMap = NCBI::SVN::SwitchMap->new(MyName => $Self->{MyName},
        MapFileName => $BranchMapFile);

    my $SVN = NCBI::SVN::Wrapper->new(MyName => $Self->{MyName});

    my @RmCommands;
    my @MkdirCommands;
    my @CopyCommands;
    my @PutCommands;

    my $BranchListFN;

    # Read branch_list, if it exists.
    my @ExistingBranches = eval {$SVN->ReadFileLines('branches/branch_list')};

    # Unless branch_list contains our branch path already, add a MUCC
    # command to (re-)create branch_list.
    if ($@ || !grep {$_ eq $BranchPath} @ExistingBranches)
    {
        my $BranchListFH;

        ($BranchListFH, $BranchListFN) = tempfile();

        print $BranchListFH "$_\n" for @ExistingBranches, $BranchPath;

        close $BranchListFH;

        push @PutCommands, 'put', $BranchListFN, 'branches/branch_list'
    }

    my %MkDirTree;
    my %RmDirTree;
    my %CommonTree;

    my $ExistingBranch;
    # Read the old branch_map, if it exists.
    my $BranchMapRepoPath = "branches/$BranchPath/branch_map";
print "Reading $BranchMapRepoPath\n";
    my @OldBranchMapLines = eval {$SVN->ReadFileLines($BranchMapRepoPath)};

    unless ($@)
    {
        $ExistingBranch = 1;

        my $OldSwitchMap = NCBI::SVN::SwitchMap->new(MyName => $Self->{MyName},
            MapFileLines => \@OldBranchMapLines);

        # Compare the old and the new branch maps, find which dirs to remove
        # and which to copy from trunk.
        my @BranchMapDiff = $OldSwitchMap->DiffSwitchPlan($SwitchMap);

        if (@BranchMapDiff)
        {
            for my $DiffLine (@BranchMapDiff)
            {
                if ($DiffLine->[1])
                {
                    print "Will remove $DiffLine->[0]\n";
                    LayPath($DiffLine->[0], \%RmDirTree, \%MkDirTree, \%CommonTree);
                    push @CopyCommands, 'cp', 'HEAD', "$TrunkDir/$DiffLine->[1]", $DiffLine->[0]
                }
                else
                {
                    print "Will remove $DiffLine->[0]\n";
                    LayPath($DiffLine->[0], \%RmDirTree, \%CommonTree)
                }
            }
            push @PutCommands, 'put', $BranchMapFile, $BranchMapRepoPath
        }
    }
    else
    {
        for (@{$SwitchMap->GetSwitchPlan()})
        {
            LayPath($_->[1], \%RmDirTree, \%MkDirTree, \%CommonTree);
            push @CopyCommands, 'cp', 'HEAD', "$TrunkDir/$_->[0]", $_->[1]
        }
        LayPath($BranchMapRepoPath, \%MkDirTree, \%CommonTree);
        push @PutCommands, 'put', $BranchMapFile, $BranchMapRepoPath
    }

    my $ExistingStructure = $Self->RetrieveRepositoryStructureContainingSubtree($SVN, $SVN->GetRepos(), \%CommonTree);
    GetRmCommands(\@RmCommands, $ExistingStructure, \%RmDirTree);
    GetMkdirCommands(\@MkdirCommands, $ExistingStructure, \%MkDirTree);

    my @Commands = (@RmCommands, @MkdirCommands, @CopyCommands, @PutCommands);

    # Unless there are no changes, commit a revision using mucc.
    if (@Commands)
    {
        print(($ExistingBranch ? 'Updating' : 'Creating') .
            " branch '$BranchPath'...\n");

        for my $Command (@Commands)
        {
            print "[$Command]\n"
        }

        system('mucc', '--message', ($ExistingBranch ? 'Updated' : 'Created') .
            " branch '$BranchPath'.", '--root-url', $SVN->{Repos}, @Commands)
    }
    else
    {
        print "Nothing to do.\n"
    }

    unlink $BranchListFN if $BranchListFN;
}

sub Remove
{
    my ($Self, $BranchPath) = @_;

    die "$Self->{MyName}: <branch_path> parameter is missing\n" unless $BranchPath;

    my $SVN = NCBI::SVN::Wrapper->new(MyName => $Self->{MyName});

    my @Commands;

    my $BranchListFN;

    # Read branch_list, if it exists.
    my @Branches = eval {$SVN->ReadFileLines('branches/branch_list')};

    if ($@)
    {
        warn "Warning: 'branches/branch_list' was not found.\n"
    }
    else
    {
        my @NewBranches = grep {$_ ne $BranchPath} @Branches;

        if (int(@NewBranches) != int(@Branches))
        {
            my $BranchListFH;

            ($BranchListFH, $BranchListFN) = tempfile();

            print $BranchListFH "$_\n" for @NewBranches;

            close $BranchListFH;

            push @Commands, 'put', $BranchListFN, 'branches/branch_list'
        }
        else
        {
            warn "Warning: Branch '$BranchPath' was not found in branch_list.\n";
        }
    }

    my %RmDirTree;

    # Read the old branch_map, if it exists.
    my $BranchMapRepoPath = "branches/$BranchPath/branch_map";

    my @BranchMapLines = eval {$SVN->ReadFileLines($BranchMapRepoPath)};

    unless ($@)
    {
        LayPath($BranchMapRepoPath, \%RmDirTree);

        my $SwitchMap = NCBI::SVN::SwitchMap->new(MyName => $Self->{MyName},
            MapFileLines => \@BranchMapLines);

        for (@{$SwitchMap->GetSwitchPlan()})
        {
            LayPath($_->[1], \%RmDirTree)
        }
    }
    else
    {
        warn "Warning: unable to retrieve '$BranchMapRepoPath'\n"
    }

    GetRmCommands(\@Commands, $Self->RetrieveRepositoryStructureContainingSubtree($SVN, $SVN->GetRepos(), \%RmDirTree), \%RmDirTree);

    # Unless there are no changes, commit a revision using mucc.
    if (@Commands)
    {
        print("Removing branch '$BranchPath'...\n");

        for my $Command (@Commands)
        {
            print "[$Command]\n"
        }

        system('mucc', '--message', "Removed branch '$BranchPath'.",
            '--root-url', $SVN->{Repos}, @Commands)
    }
    else
    {
        print "Nothing to do.\n"
    }

    unlink $BranchListFN if $BranchListFN;
}

1
