# Releasing

Every outward step (push, tag push, GitHub release) needs the maintainer's
explicit go-ahead.

## Versioning

`0.MINOR.PATCH`. Bump MINOR when behaviour changes for users (for example
matching that requires re-enrollment); bump PATCH for fixes and docs.

The version lives in three places that must match; CI checks this:

- `kernel/dkms.conf` (`PACKAGE_VERSION`)
- `scripts/install.sh` (`VERSION`)
- `scripts/uninstall.sh` (`VERSION`)

## Checklist

1. `main` is green on CI (build on Arch, Fedora, Ubuntu/DKMS, checkpatch, CodeQL, sandbox tests).
2. Write the release notes. `CHANGELOG.md` is a verbatim copy of the published
   release notes: move "Unreleased" under the new version heading.
3. Bump the version in the three files above. Commit.
4. Note in the notes if existing users must re-enroll or re-provision.
5. Tag: `git tag -a vX.Y.Z -m "vX.Y.Z"`; push the tag.
6. Build the tarball from the tag: `sh scripts/make-release.sh vX.Y.Z`.
   It archives tracked files only. Check it before upload: `tar -tzf ../GXFP5130-Linux.tar.gz | less`.
7. Create the GitHub release with the notes, the tarball and its `.sha256`.
8. Add any newly reported kernel to `docs/kernel-compat.md`.

## Careful with

- Stage files by name. `git add -A` / `git add docs/` would pick up local files
  that are deliberately untracked (recordings, `plan-*.md`, `send-*.py`).
- The module version in `dkms.conf` has stayed at `0.1.0` while the project
  moved to v0.2.0. Decide the number for the next release before step 3.
