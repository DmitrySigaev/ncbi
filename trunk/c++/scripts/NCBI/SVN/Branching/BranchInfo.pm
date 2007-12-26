# $Id$

package NCBI::SVN::Branching::BranchInfo;

use strict;
use warnings;

use base qw(NCBI::SVN::Base);

use NCBI::SVN::Branching::Util;

sub SetUpstreamAndDownSynchRev
{
    my ($Self, $SourcePath, $BranchDir, $SourceRevisionNumber, $Revision) = @_;

    my $UpstreamPath = $SourcePath;

    length($UpstreamPath) > length($BranchDir) and
        substr($UpstreamPath, -length($BranchDir),
            length($BranchDir), '') eq $BranchDir
        or die 'branch structure does not replicate ' .
            "upstream structure in r$Revision->{Number}";

    $UpstreamPath =~ s/^\/?(.+?)\/?$/$1/so;

    unless (my $SameUpstreamPath = $Self->{UpstreamPath})
    {
        $Self->{UpstreamPath} = $UpstreamPath
    }
    else
    {
        die "inconsistent source paths in r$Revision->{Number}"
            if $UpstreamPath ne $SameUpstreamPath
    }

    $Self->{BranchPathInfo}->{$BranchDir} =
        {
            PathCreationRevision => $Revision,
            SourceRevisionNumber => $SourceRevisionNumber
        };

    $Self->{LastDownSyncRevisionNumber} ||= $SourceRevisionNumber
}

sub MergeDeletedTrees
{
    my ($ObsoletePathTree, $Subdir, $DeletedNode) = @_;

    $ObsoletePathTree->{$Subdir} = {'/' => 1}
        if defined delete $DeletedNode->{'/'};

    $ObsoletePathTree = ($ObsoletePathTree->{$Subdir} ||= {});

    while (my ($Subdir, $Subtree) = each %$DeletedNode)
    {
        MergeDeletedTrees($ObsoletePathTree, $Subdir, $Subtree)
    }
}

sub ClearDeletedTree
{
    my ($ObsoletePathTree, $BranchStructure) = @_;

    while (my ($Subdir, $Subtree) = each %$BranchStructure)
    {
        if ($Subtree->{'/'})
        {
            delete $ObsoletePathTree->{$Subdir};

            next
        }

        if (my $ObsoletePathSubtree = $ObsoletePathTree->{$Subdir})
        {
            ClearDeletedTree($ObsoletePathSubtree, $Subtree)
        }
    }

    return 0
}

sub ModelBranchStructure
{
    my ($Self, $BranchStructure, $Revision, $CommonTarget) = @_;

    for my $Change (@{$Revision->{ChangedPaths}})
    {
        my ($ChangeType, $BranchDir,
            $SourcePath, $SourceRevisionNumber) = @$Change;

        next if substr($BranchDir, 0, length($CommonTarget), '')
            ne $CommonTarget;

        if ($ChangeType eq 'D' || $ChangeType eq 'R')
        {
            my $ParentNode = $BranchStructure;

            my @Subdirs = split('/', $BranchDir);
            my $Name = pop(@Subdirs);

            for my $Subdir (@Subdirs)
            {
                $ParentNode = ($ParentNode->{$Subdir} ||= {})
            }

            if ($ChangeType eq 'R' && $SourcePath)
            {
                $ParentNode->{$Name} = {'/' => 1};

                $Self->SetUpstreamAndDownSynchRev($SourcePath,
                    $BranchDir, $SourceRevisionNumber, $Revision)
            }
            elsif (my $DeletedNode = delete $ParentNode->{$Name})
            {
                my $ObsoletePathTree = ($Self->{ObsoleteBranchPaths} ||= {});

                for my $Subdir (@Subdirs)
                {
                    $ObsoletePathTree = ($ObsoletePathTree->{$Subdir} ||= {})
                }

                MergeDeletedTrees($ObsoletePathTree, $Name, $DeletedNode)
            }
        }
        elsif ($ChangeType eq 'A' && $SourcePath)
        {
            NCBI::SVN::Branching::Util::FindTreeNode($BranchStructure,
                $BranchDir)->{'/'} = 1;

            $Self->SetUpstreamAndDownSynchRev($SourcePath,
                $BranchDir, $SourceRevisionNumber, $Revision)
        }
    }
}

sub new
{
    my ($Class, $RootURL, $BranchPath) = @_;

    my @BranchRevisions;
    my @MergeDownRevisions;

    my $Self = $Class->SUPER::new(
        BranchPath => $BranchPath,
        RootURL => $RootURL,
        BranchRevisions => \@BranchRevisions,
        MergeDownRevisions => \@MergeDownRevisions
    );

    print STDERR "Gathering information about $BranchPath...\n";

    my $RevisionLog = eval {$Self->{SVN}->ReadLog('--stop-on-copy',
        "-rHEAD\:1", $RootURL, $BranchPath)};

    # Interpret a misleading error message issued when accessing the
    # repository via HTTPS and the branch identified by $BranchPath
    # couldn't be found.
    if ($@)
    {
        die $@ =~ m/Secure connection truncated/s ?
            "$BranchPath\: no such branch\n" : $@
    }

    my %BranchStructure;
    my $CommonTarget = "/$BranchPath/";

    for my $Revision (reverse @$RevisionLog)
    {
        unshift @BranchRevisions, $Revision;

        my $LogMessage = $Revision->{LogMessage};

        if (my ($SourceRevisionNumber, $MergeDescription) =
            NCBI::SVN::Branching::Util::DetectMergeRevision($LogMessage,
                $Self->{UpstreamPath}, $BranchPath))
        {
            $Revision->{SourceRevisionNumber} =
                $Self->{LastDownSyncRevisionNumber} = $SourceRevisionNumber;

            $Revision->{MergeDirection} = 'down';
            $Revision->{MergeDescription} = $MergeDescription;

            unshift @MergeDownRevisions, $Revision
        }
        elsif ($LogMessage =~ m/^(Created|Modified) branch '(.+?)'.$/o &&
            $2 eq $BranchPath)
        {
            if ($1 eq 'Created' || !$Self->{BranchCreationRevision})
            {
                $Self->{BranchCreationRevision} = $Revision;

                @BranchRevisions = ($Revision);
                @MergeDownRevisions = ();
                %BranchStructure = ();

                $Self->ModelBranchStructure(\%BranchStructure,
                    $Revision, $CommonTarget);

                $Self->{BranchSourceRevision} =
                    $Self->{LastDownSyncRevisionNumber}
            }
            else
            {
                $Self->ModelBranchStructure(\%BranchStructure,
                    $Revision, $CommonTarget)
            }
        }
    }

    unless ($Self->{BranchCreationRevision} && $Self->{UpstreamPath})
    {
        die 'unable to determine branch source'
    }

    my @BranchPaths =
        sort @{NCBI::SVN::Branching::Util::FindPathsInTree(\%BranchStructure)};

    $Self->{BranchPaths} = \@BranchPaths;

    if ($Self->{ObsoleteBranchPaths})
    {
        ClearDeletedTree($Self->{ObsoleteBranchPaths}, \%BranchStructure);

        $Self->{ObsoleteBranchPaths} =
            NCBI::SVN::Branching::Util::FindPathsInTree(
                $Self->{ObsoleteBranchPaths})
    }

    return $Self
}

package NCBI::SVN::Branching::UpstreamInfo;

use base qw(NCBI::SVN::Base);

sub new
{
    my ($Class, $BranchInfo) = @_;

    my $Self = $Class->SUPER::new(BranchInfo => $BranchInfo);

    my ($BranchPath, $UpstreamPath) = @$BranchInfo{qw(BranchPath UpstreamPath)};

    print STDERR "Gathering information about $UpstreamPath...\n";

    $Self->{LastUpSyncRevisionNumber} =
        $BranchInfo->{BranchCreationRevision}->{Number};

    my $UpstreamRevisions = $Self->{SVN}->ReadLog('--stop-on-copy',
        "-rHEAD\:$BranchInfo->{BranchRevisions}->[-1]->{Number}",
        $BranchInfo->{RootURL},
        map {"$UpstreamPath/$_"} @{$BranchInfo->{BranchPaths}});

    my @MergeUpRevisions;

    for my $Revision (reverse @$UpstreamRevisions)
    {
        if (my ($SourceRevisionNumber, $MergeDescription) =
            NCBI::SVN::Branching::Util::DetectMergeRevision(
                $Revision->{LogMessage}, $BranchPath, $UpstreamPath))
        {
            $Revision->{SourceRevisionNumber} =
                $Self->{LastUpSyncRevisionNumber} = $SourceRevisionNumber;

            $Revision->{MergeDirection} = 'up';
            $Revision->{MergeDescription} = $MergeDescription;

            unshift @MergeUpRevisions, $Revision
        }
    }

    @$Self{qw(UpstreamRevisions MergeUpRevisions)} =
        ($UpstreamRevisions, \@MergeUpRevisions);

    $Self->{MergeRevisions} = [sort {$b->{Number} <=> $a->{Number}}
        @MergeUpRevisions, @{$BranchInfo->{MergeDownRevisions}}];

    return $Self
}

1
