# Git Branching and Version Control Guide

Complete guide for managing multiple branches, production builds, and safe development in the ESP-Miner project.

## Table of Contents
- [Creating Separate Production Build](#creating-separate-production-build)
- [Reverting Changes During Development](#reverting-changes-during-development)
- [Branch Management](#branch-management)
- [Safe Development Workflow](#safe-development-workflow)
- [Common Scenarios](#common-scenarios)

---

## Creating Separate Production Build

### Option A: Clone Repository to Separate Directory (Recommended)

Use this when you want completely isolated directories for production and development.

```bash
# Navigate to your work directory
cd ~/Work

# Clone the repository to a new directory
git clone https://github.com/yourusername/ESP-Miner.git ESP-Miner-production

# Enter the new directory
cd ESP-Miner-production

# Checkout the production branch
git checkout feature/voltage-monitoring-advanced

# Verify you're on the correct branch
git branch
```

**Result**: You now have two completely separate directories:
- `~/Work/ESP-Miner/` - Development branch (`feature/custom-branding`)
- `~/Work/ESP-Miner-production/` - Production branch (`feature/voltage-monitoring-advanced`)

**Benefits**:
- Complete isolation between development and production
- Can build/flash from production without affecting development
- Simple and easy to understand
- Each directory has its own build artifacts

### Option B: Use Git Worktree (Advanced)

Use this when you want to save disk space by sharing the `.git` repository.

```bash
# From your current ESP-Miner directory
cd ~/Work/ESP-Miner

# Create a worktree for production
git worktree add ../ESP-Miner-production feature/voltage-monitoring-advanced

# Verify
git worktree list
```

**Benefits**:
- Shares the same `.git` repository (saves disk space)
- Both directories stay in sync with the same repository
- Can push/pull from either directory
- More advanced Git feature

**To remove a worktree later**:
```bash
git worktree remove ../ESP-Miner-production
```

### Building from Production Directory

```bash
# Navigate to production directory
cd ~/Work/ESP-Miner-production

# Set up ESP-IDF environment
cd ~/esp/esp-idf
. ./export.sh

# Return to production directory
cd ~/Work/ESP-Miner-production

# Build and flash
./build.sh -f -m
```

---

## Reverting Changes During Development

### Revert a Single File

Restore a specific file to its last committed state:

```bash
# Discard changes to a specific file
git checkout -- path/to/file

# Examples:
git checkout -- src/styles.scss
git checkout -- src/assets/logo.svg
git checkout -- main/http_server/axe-os/src/app/app.component.ts
```

### Revert Multiple Files

```bash
# Restore all SCSS files
git checkout -- "*.scss"

# Restore entire directory
git checkout -- src/assets/
```

### Revert All Uncommitted Changes

**Warning**: This discards ALL changes since the last commit!

```bash
# Discard all changes (nuclear option)
git reset --hard HEAD

# Remove untracked files and directories as well
git clean -fd
```

### Unstage Changes (Keep Modifications)

If you've staged files with `git add` but want to unstage them:

```bash
# Unstage all files (keep changes)
git reset HEAD

# Unstage specific file (keep changes)
git reset HEAD path/to/file
```

### Revert to a Specific Commit

```bash
# View commit history
git log --oneline

# Example output:
# a1b2c3d (HEAD -> feature/custom-branding) Change logo colours
# e4f5g6h Replace main logo
# i7j8k9l Initial branding setup

# Revert to specific commit (keep changes as uncommitted)
git reset e4f5g6h

# Revert to specific commit (discard everything)
git reset --hard e4f5g6h

# Revert to previous commit
git reset --hard HEAD~1    # Go back 1 commit
git reset --hard HEAD~3    # Go back 3 commits
```

### View Changes Before Reverting

Always check what will be reverted:

```bash
# See what files have changed
git status

# See detailed changes in all files
git diff

# See changes in a specific file
git diff src/styles.scss

# See changes in staged files
git diff --cached
```

---

## Branch Management

### Current Branch Structure

Your repository has these branches:
- `master` - Main branch
- `feature/voltage-monitoring` - Voltage monitoring feature
- `feature/voltage-monitoring-advanced` - Advanced voltage monitoring (production)
- `feature/custom-branding` - Custom branding work (development)

### Creating New Branches

```bash
# Create and switch to new branch
git checkout -b feature/new-feature-name

# Create branch from specific starting point
git checkout -b feature/new-feature feature/voltage-monitoring-advanced
```

### Switching Between Branches

```bash
# Switch to existing branch
git checkout feature/custom-branding

# Switch back to production branch
git checkout feature/voltage-monitoring-advanced

# List all branches
git branch

# List all branches including remote
git branch -a
```

### Deleting Branches

```bash
# Delete local branch (safe - won't delete if unmerged)
git branch -d feature/old-branch

# Force delete local branch
git branch -D feature/old-branch

# Delete remote branch
git push origin --delete feature/old-branch
```

### Merging Branches

```bash
# Switch to the branch you want to merge INTO
git checkout feature/voltage-monitoring-advanced

# Merge another branch into current branch
git merge feature/custom-branding

# If conflicts occur, resolve them then:
git add .
git commit -m "Merge custom branding into advanced monitoring"
```

---

## Safe Development Workflow

### The Golden Rule: Commit Early and Often

Create save points throughout your development:

```bash
# After each successful change, commit immediately
git add .
git commit -m "Change primary colour to #1a2b3c"

# After replacing a file
git add src/assets/logo.svg
git commit -m "Replace logo with custom branding"

# After testing a feature
git add .
git commit -m "Add custom favicon - tested and working"
```

### Experimental Changes: Use Temporary Branches

```bash
# Create experimental branch
git checkout -b experiment/blue-theme

# Make changes...
# ... edit files ...

# If you like it, merge back:
git checkout feature/custom-branding
git merge experiment/blue-theme

# If you don't like it, delete the experiment:
git checkout feature/custom-branding
git branch -D experiment/blue-theme
```

### Regular Backups to GitHub

```bash
# Push your branch regularly (creates remote backup)
git push origin feature/custom-branding

# If branch doesn't exist on remote yet
git push -u origin feature/custom-branding

# Push all branches
git push --all
```

### Working with Uncommitted Changes

```bash
# Save current work without committing (stash)
git stash save "WIP: testing new colour scheme"

# Do something else (switch branches, etc.)
git checkout master

# Return and restore your work
git checkout feature/custom-branding
git stash pop

# List all stashes
git stash list

# Apply specific stash
git stash apply stash@{0}
```

---

## Common Scenarios

### Scenario 1: Customisation Completely Broken - Start Over

```bash
# Nuclear option: restore everything to last commit
git reset --hard HEAD
git clean -fd

# Or go back to when it was working (find commit first)
git log --oneline
git reset --hard <working-commit-hash>
```

### Scenario 2: Need to Flash Production While Developing

```bash
# In development directory
cd ~/Work/ESP-Miner

# Commit current work (even if incomplete)
git add .
git commit -m "WIP: customising colours - not ready"
git push origin feature/custom-branding

# Switch to production directory
cd ~/Work/ESP-Miner-production

# Build and flash production firmware
./build.sh -f

# Return to development
cd ~/Work/ESP-Miner
```

### Scenario 3: Made Changes But Want to Try Different Approach

```bash
# Save current changes
git stash save "First attempt at logo redesign"

# Try new approach
# ... make new changes ...

# If new approach is better, commit it
git add .
git commit -m "Better logo design approach"

# If you want the old changes back
git stash pop
```

### Scenario 4: Accidentally Committed to Wrong Branch

```bash
# On wrong branch with committed changes
git log --oneline  # Note the commit hash

# Switch to correct branch
git checkout feature/custom-branding

# Cherry-pick the commit
git cherry-pick <commit-hash>

# Go back to wrong branch and remove the commit
git checkout wrong-branch
git reset --hard HEAD~1
```

### Scenario 5: Need to Update Production Branch with New Fixes

```bash
# Pull latest changes in development
cd ~/Work/ESP-Miner
git pull origin feature/custom-branding

# Update production directory
cd ~/Work/ESP-Miner-production
git fetch origin
git checkout feature/voltage-monitoring-advanced
git pull origin feature/voltage-monitoring-advanced
```

### Scenario 6: Want to See What Changed Since Last Successful Build

```bash
# View commit history with changes
git log --stat

# View detailed history
git log -p

# Compare current state with specific commit
git diff <commit-hash>

# Compare two commits
git diff <old-commit> <new-commit>
```

### Scenario 7: Merge Customisation into Production

When customisation is complete and tested:

```bash
# Ensure custom-branding is committed and pushed
cd ~/Work/ESP-Miner
git add .
git commit -m "Final customisation complete and tested"
git push origin feature/custom-branding

# Switch to production branch
git checkout feature/voltage-monitoring-advanced

# Merge customisation
git merge feature/custom-branding

# Resolve any conflicts if they occur
# ... fix conflicts ...
git add .
git commit -m "Merge custom branding into production"

# Push merged branch
git push origin feature/voltage-monitoring-advanced
```

---

## Quick Reference Commands

### Save Your Work
```bash
git add .
git commit -m "Description of changes"
git push origin feature/custom-branding
```

### Undo Mistakes
```bash
# Single file
git checkout -- filename

# Everything
git reset --hard HEAD
```

### Check Status
```bash
git status                  # What's changed
git diff                    # Detailed changes
git log --oneline          # Commit history
git branch                 # Current branch
```

### Emergency Recovery
```bash
# Go back to last known good state
git log --oneline          # Find good commit
git reset --hard <hash>    # Reset to that commit

# Or just reset to last commit
git reset --hard HEAD
git clean -fd
```

---

## Best Practices

1. **Commit frequently** - Small commits are easier to revert
2. **Use descriptive commit messages** - "Fix logo" is better than "changes"
3. **Test before committing** - Build and test your changes
4. **Push regularly** - Back up to GitHub daily
5. **Use separate directories** - Keep production and development isolated
6. **Branch for experiments** - Create temporary branches for risky changes
7. **Never force push** - Unless you absolutely know what you're doing
8. **Keep production stable** - Only merge tested, working code

---

## Troubleshooting

### "Detached HEAD" State

If you see "HEAD detached at...":

```bash
# Create a new branch from current state
git checkout -b recovery-branch

# Or discard changes and return to a branch
git checkout feature/custom-branding
```

### Merge Conflicts

When merging causes conflicts:

```bash
# View conflicted files
git status

# Edit files to resolve conflicts (look for <<<<<<< markers)
nano conflicted-file.txt

# After resolving
git add conflicted-file.txt
git commit -m "Resolved merge conflicts"
```

### Lost Commits

If you accidentally reset and lost commits:

```bash
# View reflog (history of HEAD positions)
git reflog

# Find your lost commit
git reflog | grep "commit message"

# Recover it
git checkout -b recovery <commit-hash>
```

---

## Additional Resources

- Official Git Documentation: https://git-scm.com/doc
- Interactive Git Tutorial: https://learngitbranching.js.org/
- Git Cheat Sheet: https://education.github.com/git-cheat-sheet-education.pdf

---

**Remember**: Git is designed to preserve your work. When in doubt, commit your changes before trying anything destructive. You can always revert a commit, but you can't recover uncommitted work that's been reset!
