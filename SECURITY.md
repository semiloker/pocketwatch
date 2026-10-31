# Security policy

Pocketwatch's game plugin runs in kernel mode on the PSP, with full access to the device. A problem
that could damage a Memory Stick or the PSP's flash, or that could run code from a file a user
receives (a save state, a translation, a settings file, a picture, a tag in a music file), is
worth reporting.

## Reporting

Please use GitHub's private reporting: **[Report a vulnerability](https://github.com/semiloker/pocketwatch/security/advisories/new)**.
Do not open a public issue for it.

Include the version (first lines of `seplugins/pocketwatch/pocketwatch.log`), your PSP model and
ARK-4 version, and the steps to reproduce it. This is a one-person project, so a reply can take a
while.

## Supported versions

Only the newest pre-release gets fixes.

## Data loss

A save state that corrupts a game's saves or the Memory Stick is a serious bug, but an ordinary one:
report it as a [normal issue](https://github.com/semiloker/pocketwatch/issues/new/choose) with the log.
