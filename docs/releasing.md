# Releasing splc

A release is a git tag on `main` plus a GitHub Release whose description is the
matching `docs/release-notes-X.Y.Z.md`. CI (`.github/workflows/ci.yml`) runs on every
push to `main` and on every tag, so a release is only cut once the last push is green.

## 1. Update the version and the notes

```
# in ~/Code/spl on main, with a clean tree
git status
```

Edit `CMakeLists.txt` so that `project(splc VERSION X.Y.Z ...)` carries the new
version; `splc --version` prints it. Write `docs/release-notes-X.Y.Z.md` (copy the
previous one and describe what changed; keep it to what a user would want to know).
Check that `README.md` still describes the tree, then commit:

```
git add CMakeLists.txt docs/release-notes-X.Y.Z.md README.md
git commit -m "Release X.Y.Z"
```

## 2. Verify locally

```
./build_mac.sh                      # configure, build and run the tests with Homebrew LLVM
sh test/calibration.sh build-mac/splc
build-mac/splc --version            # prints splc X.Y.Z (LLVM ...)
```

## 3. Push and wait for CI

```
git push
```

Open https://github.com/bakkertj/splc/actions and wait for both jobs (Ubuntu with
LLVM 18, macOS with Homebrew LLVM) to pass on that commit. If either fails, fix it on
`main` with a follow-up commit and push again; do not tag until it is green.

## 4. Tag

```
git tag -a vX.Y.Z -m "splc X.Y.Z"
git push origin vX.Y.Z
```

The tag push starts one more CI run; it should be green since it is the same commit.

## 5. Create the GitHub Release

Either in the browser: https://github.com/bakkertj/splc/releases/new, choose the tag
`vX.Y.Z`, title `splc X.Y.Z`, paste the contents of `docs/release-notes-X.Y.Z.md` into
the description, and publish.

Or with the GitHub CLI (`brew install gh`, then `gh auth login` once):

```
gh release create vX.Y.Z --title "splc X.Y.Z" --notes-file docs/release-notes-X.Y.Z.md
```

## Fixing a release after the fact

The release description is a copy, not a link: editing the notes file and pushing does
not change what the release page shows. To update it:

```
gh release edit vX.Y.Z --notes-file docs/release-notes-X.Y.Z.md
```

or use the pencil icon on the release page. Moving a tag that has already been
published (`git tag -f vX.Y.Z && git push --force origin vX.Y.Z`) is best avoided once
anyone may have fetched it; prefer a new patch version.

## For this update (0.1.1)

The verse generator rewrite and the regenerated `balcony_verse.spl` are committed on
`main` after `v0.1.0`. `CMakeLists.txt` and `docs/release-notes-0.1.1.md` are already
prepared, so the steps are:

```
git push
# wait for https://github.com/bakkertj/splc/actions to go green
git tag -a v0.1.1 -m "splc 0.1.1"
git push origin v0.1.1
gh release create v0.1.1 --title "splc 0.1.1" --notes-file docs/release-notes-0.1.1.md
```

While there, replace the v0.1.0 description, which still says thirteen tests:

```
gh release edit v0.1.0 --notes-file docs/release-notes-0.1.0.md
```
