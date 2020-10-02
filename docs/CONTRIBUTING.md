# Code contribution guidelines

To make sure code changes remain easy to organize, find and troubleshoot, please use the following guidelines when contributing code to the Pale Moon project (and UXP). These guidelines may, for some of you, sound tedious or more work than you'd prefer to put in, but keep in mind that a single paragraph of explanation with a code commit can save hours of work later on, and code comments can save your hide when trying to debug or improve code.

## General guideline for issues

Issues are the main drivers behind getting code changes accepted into our source tree. Issues should be opened, and properly labeled, for any of the following:
- Code bugs (broken functionality)
- Regressions (functionality that worked before but has either stopped working, is working in a different way than intended, or e.g. has become poorly performing)
- Enhancements (new code/features)
- Improvements (better/improved way of existing functionality)
- General development direction goals (META issues)

Labels should mostly be assigned to issues, not pull requests (PRs).

Issues opened should, where possible, only address one thing at a time. Never roll multiple bugs or enhancements that aren't directly required to be done at the same time into a single issue.

Issues should include:
### For bugs
- Description of the bug
- Affected versions/features
- Steps to reproduce the bug
- Expected result when performing the steps
- Actual result when performing the steps
### For regressions
- Description of the regression
- Affected versions/features
- What the previous behavior was
- What the current (incorrect) behavior is
### For enhancements and improvements
- Description of the enhancement
- Background: why is this needed or desired?
- Impact of the enhancement/improvement
### META issues
- A general description, including **what** and **why** should be tackled
- Links from the individual components of the META goal (by referencing from the actual individual issues)
For META issues it is important that discussion about specific implementation parts are in the individual component issues and not in the META issue, so it can keep a quick overview of linked components that need to be tackled.


## General guideline for submitting pull requests

Most of our code changes will likely be submitted by collaborators from our community. This will be ported from other projects (e.g. Mozilla-based code) or new implementations, or a mix. The standard way of getting your code into Pale Moon or UXP is by way of creating a pull request against the master branch (trunk).

A good pull request will:
- **Have an issue associated with it!** Even though GitHub offers very similar tools for PRs as it does for issues, any reasoning behind code commits in a PR have to be explained in an accompanying issue.
- Describe in detail **what** the code change attempts to achieve, and **how** it achieves this. If you have references to external sources, e.g. if your code is based on specific code from Mozilla, then including them is of course excellent, but it's important to provide a clear description alongside it.
- Not just be a list of links or references. See the previous point. References are OK but do not provide a reason by themselves for a code change. Especially with porting code across from other projects, it is important to keep PRs and explanations in our tree and not dependent on third-party sources.
- Verify that the code is exactly catered to our tree and in line with our feature set(s) and direction. Just 'porting something across because it is in other browsers" is not a reason or justification by itself. If needed, discuss the **why** for a code change in the attached issue before putting work into a PR that may otherwise be rejected.
- Only address one bug, issue or enhancement. It is extremely important to keep individual code commits separate so that in case of back-outs, this can be done in a controlled and isolated manner.
- If there are dependencies on other code being checked in, it is important that the PR lists exactly which other PRs need to be checked in first before the one in question can be merged. If at all possible, keep PR interdependency to a minimum.

The odd small minor one-liner pull request may not need an issue dedicated to it, but when it comes to modifying and refactoring chunks of many files, then yes it would be not only nice, but required, to have a chance to have discussion about it before it is accepted into the tree.

PRs that encompass major code changes or touch many files or functions should preferably get a review. Reviewing can be volunteered or requested.

## Code Style

Because our software encompasses many components of which a good portion is third-party, code style can vary significantly throughout our code base. It's therefore important to keep code as consistent as possible.

Important when editing/adding code:

1. Stick to the style of surrounding code. If you're editing code in a module that deviates from our generally-used code style, don't mix in generally-used code style with the deviating code style. E.g. if a source file uses tabs everywhere, and you edit this code, also use tabs. If you feel code style should be rewritten to improve legibility, preferably create a separate issue to change it and do not roll it in with functional code changes!
2. Use the following guideline where there's no clear deviating style in use:
    - Put opening function/scope braces at the end of the line.
    - Align closing braces with the indentation of the first character of the function/scope
    - Use a line cap of 80 characters. Put continuations on the next line if it overflows.
    - Use spaces for indentation, and indent scopes 2 spaces.  
    Example:
    ```C++
    nsSVGFilterFrame*
    nsSVGFilterFrame::GetReferencedFilterIfNotInUse() {
      nsSVGFilterFrame* referenced = GetReferencedFilter();
      if (!referenced)
        return nullptr;
    
      if (referenced->mLoopFlag) {
        // XXXjwatt: we should really send an error to the JavaScript Console here:
        NS_WARNING("Filter reference loop detected while inheriting attribute!");
        return nullptr;
      }
    
      return referenced;
    }
    ```
3. Comment your code where prudent! Documenting what you're doing, especially in a complex piece of code, is very important to keep it easy to debug.
4. Try not to write "as compact as possible" source code. Many languages have facilities to write very compact source code by gluing a lot of statements together on single lines, but you should avoid this to keep it readable. Other people than yourself will be looking at and trying to understand your code, and it's important to keep proper paragraphing, whitespace and above all logical names for variables and functions.

## Commit Message Style

With rare exception, it is advisable to use the following style for commit messages. This ensures proper tracking and linking of commits to issues.

### Commits with existing issue

This would directly apply to anyone making a pull request.

Single commits are where issues can be reasonably resolved in a single commit.
- Single Commit: `Issue #xxx - Cited issue title or appropriate direct description of changes`
  - *`Issue #1083 - Deprecate FUEL extension helper javascript library`*

Multi-part commits would be used for complex issues. However, an exception exists for multi-part commits where the issue is anticipated or ends up being long term such as "Stop using unified compilation of sources". In these instances the multi-part form is not required.
- Multi-Part Form: `Issue #xxx - Part Number: Appropriate direct description of changes`
  - *`Issue #492 - Part 1: Remove files`*
  - *`Issue #492 - Part 2: Build system, Installer/Packaging`*
  - *`Issue #492 - Part 3: nsUpdateService.js, updater.cpp, nsUpdateDriver.cpp`*
  - *`Issue #492 - Part 4: Remove superfluous brackets in nsUpdateService.js and updater.cpp`*

Occasionally a resolved (and shipped) issue requires further changes to fix bugs. However, If follow-ups are complex enough to be multi-part it should be considered a new issue.
- Follow-up Form: *`Issue #xxx - Follow-up: Appropriate direct description of changes`*
  - *`Issue #1643 - Follow-up: Make sure things aren't changed while iterating.`*

### Commits with no issue

Sometimes developers with direct push access will commit without an issue being created. You should, where applicable, prefix the commit message with the application, component, or lib you are working on in square brackets.

- `[COMPONENT] Description of what is being changed`
  - *`[TychoAM] Give the second <hbox> an ID so it can be targeted in extensions.xul`*
  - *`[Pale Moon] Use generic application icon for external applications in about:feeds`*

Multi-part changes by definition are not trival and should have a corresponding issue.

### Additional information in commit messages

It may be benefical to include supplementary information such as a longer description, caveats, specific Mozilla bug numbers, links to forum posts or other such references. If you do add additional information, you should seperate it from the main commit message by a blank line.

*Example: (This happens to also be an example of the long term multi-part exception)*
```
Issue #80 - Stop building /accessible unified and fix deprot

Note: excludes changes to Mac-specific code because I can't build
for OS X to check and fix deprot there.
```

In general, if you have a question about commit message form or content, please ask.
