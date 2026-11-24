# Maintainer publication procedure for 0.1.0b2

Run these PowerShell blocks in order from the existing FAULTLINE checkout. The
publication candidate is in `build-publication-preflight/kit`. Its source
archive and Linux wheels retain their frozen bytes. Use the Windows wheels from
this kit, which were rebuilt with build paths remapped.

## Review the kit and source

```powershell
$checkout = (Get-Location).Path
$preflight = Join-Path $checkout 'build-publication-preflight'
$kit = Join-Path $preflight 'kit'
$py = (Resolve-Path 'build-scientific-distribution/tools-windows/Scripts/python.exe').Path
$upstream = 'https://github.com/LuCa1506s/FAULTLINE.git'
$repo = 'LuCa1506s/FAULTLINE'
$version = '0.1.0b2'
$tag = "v$version"
$publicBase = 'c88513b5dcd369d3605a2259e1ac207974d072e7'
$ErrorActionPreference = 'Stop'
function Verify-Native { if ($LASTEXITCODE -ne 0) { throw "Command failed with exit code $LASTEXITCODE" } }

git status --short --branch
Verify-Native
git log --oneline "$publicBase..HEAD"
Verify-Native
git diff --stat $publicBase
Verify-Native
$head = git ls-remote $upstream refs/heads/main
Verify-Native
if (($head -split '\s+')[0] -ne $publicBase) { throw 'Public main moved; review and rehearse again against that tip' }
$existingTag = git ls-remote $upstream "refs/tags/$tag"
Verify-Native
if ($existingTag) { throw 'The public tag already exists' }

& $py tools/package_manifest.py $kit --verify
Verify-Native
$packages = @(
    (Join-Path $kit 'source/faultline_sim-0.1.0b2.tar.gz'),
    (Join-Path $kit 'windows/faultline_sim-0.1.0b2-cp312-cp312-win_amd64.whl'),
    (Join-Path $kit 'windows/faultline_sim-0.1.0b2-cp314-cp314-win_amd64.whl'),
    (Join-Path $kit 'linux/faultline_sim-0.1.0b2-cp312-cp312-manylinux_2_39_x86_64.whl'),
    (Join-Path $kit 'linux/faultline_sim-0.1.0b2-cp314-cp314-manylinux_2_39_x86_64.whl')
)
& $py -m twine check --strict @packages
Verify-Native
function Assert-PyPIVersionAvailable {
    $projectStatus = curl.exe -sS -o NUL -w '%{http_code}' https://pypi.org/pypi/faultline-sim/json
    Verify-Native
    $versionStatus = curl.exe -sS -o NUL -w '%{http_code}' "https://pypi.org/pypi/faultline-sim/$version/json"
    Verify-Native
    if ($versionStatus -ne '404' -or $projectStatus -notin @('200','404')) { throw 'Review the live PyPI name and version before continuing' }
    if ($projectStatus -eq '200' -and (Read-Host 'Confirm you own this PyPI project. Type OWNED') -cne 'OWNED') { throw 'PyPI ownership required' }
}
Assert-PyPIVersionAvailable
```

The PyPI name and version checks are point-in-time registry reads. A 404
provides no reservation. If PyPI rejects registration, resolve the name with
the maintainer and rebuild under the accepted distribution metadata.
[PyPI name selection](https://docs.pypi.org/project-management/choosing-a-name/)

## Stage and commit in an isolated worktree

The local checkout contains an unpushed community-template commit whose
identity uses a non-no-reply address. Use its reviewed file changes and
start the release commit at public `main`. This preserves the original
checkout, index, branch and probes. Review the full staged patch before
creating the release commit. The compiled source includes the existing game
CMake cleanup recorded in the [final review](FINAL_REVIEW.md).

```powershell
$review = Join-Path $preflight 'release-worktree'
$pathsFile = Join-Path $preflight 'release-source-paths.txt'
$expectedTree = (Get-Content (Join-Path $preflight 'reviewed-tree.txt') -Raw).Trim()
$paths = @(Get-Content $pathsFile)
git worktree add --detach $review $publicBase
Verify-Native
foreach ($rel in $paths) {
    $destination = Join-Path $review $rel
    New-Item -ItemType Directory -Force (Split-Path -Parent $destination) | Out-Null
    Copy-Item -LiteralPath (Join-Path $checkout $rel) -Destination $destination
}
git -C $review add "--pathspec-from-file=$pathsFile"
Verify-Native
git -C $review diff --cached --check
Verify-Native
git -C $review diff --cached --stat
Verify-Native
git -C $review diff --cached
Verify-Native
$prepared = git -C $review write-tree
Verify-Native
if ($prepared -ne $expectedTree) { throw 'The staged tree differs from the reviewed tree' }

$name = Read-Host 'Public GitHub display name for the release commit'
$noreply = Read-Host 'Exact GitHub no-reply address from GitHub email settings'
if (!$name -or $noreply -notmatch '^[A-Za-z0-9+_.-]+@users\.noreply\.github\.com$') { throw 'A GitHub no-reply identity is required' }
git -C $review -c "user.name=$name" -c "user.email=$noreply" commit -m "Release FAULTLINE $version"
Verify-Native
$commit = git -C $review rev-parse HEAD
Verify-Native
$commitTree = git -C $review rev-parse 'HEAD^{tree}'
Verify-Native
$identities = @(git -C $review show -s --format='%ae%n%ce' $commit)
Verify-Native
if ($commitTree -ne $expectedTree -or $identities.Count -ne 2 -or ($identities | Where-Object { $_ -ne $noreply })) { throw 'Review release tree and public commit identity' }
```

## Push and verify hosted CI

The maintainer authorizes this step. Use GitHub CLI browser authentication
with the OS credential store. Install the CLI with `winget install --id GitHub.cli --exact`
if needed. Use the secure credential store for this procedure. The push starts the game and framework CI
workflows. If `main` is protected, use the repository's approved pull request
process. After merging, use the hosted `main` SHA as `$commit` and verify its
tree equals `$expectedTree` before following the remaining instructions.
[GitHub CLI authentication](https://cli.github.com/manual/gh_auth_login)

```powershell
gh auth login --hostname github.com --web --git-protocol https
Verify-Native
gh auth setup-git --hostname github.com
Verify-Native
git -C $review push $upstream "${commit}:refs/heads/main"
Verify-Native
```

If direct push is rejected by branch rules, use an authorized release branch
and open a pull request with these commands. After review and merge, continue
with the hosted checks.

```powershell
git -C $review push $upstream "${commit}:refs/heads/release/$tag"
Verify-Native
gh pr create --repo $repo --base main --head "release/$tag" --title "Release FAULTLINE $version" --body 'Reviewed beta source and publication preflight.'
Verify-Native
```

After a direct push or an approved merge, resolve the exact public commit and
wait for both hosted CI workflows.

```powershell
$releaseTip = gh api "repos/$repo/commits/main" --jq '.sha + " " + .commit.tree.sha'
Verify-Native
$commit, $hostedTree = $releaseTip -split ' '
if ($hostedTree -ne $expectedTree) { throw 'Public main differs from the reviewed tree' }
git -C $review fetch $upstream $commit
Verify-Native
foreach ($workflow in 'ci.yml','framework.yml') {
    $runs = @(gh run list --repo $repo --workflow $workflow --commit $commit --branch main --event push --limit 10 --json 'databaseId,headSha,status,conclusion,url' | ConvertFrom-Json)
    Verify-Native
    if (!$runs.Count) { throw "Wait for $workflow to appear for $commit, then rerun this loop" }
    gh run watch $runs[0].databaseId --repo $repo --exit-status
    Verify-Native
}
```

The dispatch-only distribution review workflow builds review artifacts with
read-only repository permissions. The public beta uses the local verified
glibc 2.39 kit. The optional hosted `manylinux_2_28` review is separate.
[GitHub CLI run list](https://cli.github.com/manual/gh_run_list) · [GitHub CLI run watch](https://cli.github.com/manual/gh_run_watch)

## Tag and attach the reviewed files

```powershell
$remote = gh api "repos/$repo/commits/main" --jq '.sha + " " + .commit.tree.sha'
Verify-Native
if ($remote -ne "$commit $expectedTree") { throw 'Remote main is not the reviewed release commit' }
git -C $review -c "user.name=$name" -c "user.email=$noreply" tag -a $tag $commit -m "FAULTLINE $version"
Verify-Native
$tagEmail = git -C $review for-each-ref --format='%(taggeremail:trim)' "refs/tags/$tag"
Verify-Native
if ($tagEmail -ne $noreply) { throw 'The tag identity differs from the reviewed public identity' }
git -C $review push $upstream "refs/tags/$tag"
Verify-Native
gh release create $tag --repo $repo --verify-tag --draft --prerelease --title "FAULTLINE $version" --notes-file (Join-Path $kit 'RELEASE_NOTES.md')
Verify-Native
$assets = $packages + @('SHA256SUMS','manifest.json','sbom.cdx.json','review-evidence.zip','RELEASE_NOTES.md' | ForEach-Object { Join-Path $kit $_ })
gh release upload $tag @assets --repo $repo
Verify-Native
$uploaded = gh release view $tag --repo $repo --json 'assets,isDraft,isPrerelease,tagName' | ConvertFrom-Json
Verify-Native
if (!$uploaded.isDraft -or !$uploaded.isPrerelease -or $uploaded.tagName -ne $tag -or
    (Compare-Object @($assets | ForEach-Object { Split-Path -Leaf $_ } | Sort-Object) @($uploaded.assets.name | Sort-Object))) { throw 'Review the draft assets' }
```

The GitHub assets include the four wheels, source archive, release notes,
evidence ZIP, manifest, SHA-256 list and SBOM. To run the kit checksum check
after downloading flat GitHub assets, put the `.tar.gz` in `source/` and
the platform wheels in `windows/` or `linux/` as listed in `manifest.json`.
[GitHub CLI release create](https://cli.github.com/manual/gh_release_create) · [GitHub CLI release upload](https://cli.github.com/manual/gh_release_upload)

## Upload to PyPI and verify installation

Use a PyPI account authorized for the name. Enter an API token at Twine's
interactive password prompt or use the platform keyring. Leave `TWINE_PASSWORD`
unset for interactive entry. A first upload may require an account-scoped
token. Use a project-scoped token for subsequent manual uploads.
[PyPI publishing guide](https://docs.pypi.org/publishing/publishing-to-pypi/) · [Twine](https://twine.readthedocs.io/en/stable/)

```powershell
Assert-PyPIVersionAvailable
& $py -m twine upload --repository-url https://upload.pypi.org/legacy/ --username __token__ @packages
Verify-Native
$published = curl.exe -fsS "https://pypi.org/pypi/faultline-sim/$version/json" | ConvertFrom-Json
Verify-Native
if ($published.urls.Count -ne $packages.Count) { throw 'PyPI file count differs from the kit' }
foreach ($package in $packages) {
    $filename = Split-Path -Leaf $package
    $record = @($published.urls | Where-Object filename -eq $filename)
    if ($record.Count -ne 1 -or $record[0].digests.sha256 -ne (Get-FileHash $package -Algorithm SHA256).Hash) { throw "PyPI checksum differs for $filename" }
}

$clean = New-Item -ItemType Directory (Join-Path $preflight ('pypi-check-' + [guid]::NewGuid().ToString('N')))
Push-Location $clean
try {
    py -3.12 -m venv venv
    Verify-Native
    $check = Join-Path $clean 'venv/Scripts/python.exe'
    & $check -m pip --isolated install --index-url https://pypi.org/simple --only-binary=:all: "faultline-sim[gym]==$version"
    Verify-Native
    & $check -m pip check
    Verify-Native
    & $check -I -m faultline_sim.scientific run --scenario navigation --level EASY --agent planning --seed 42 --episodes 1 --traces traces
    Verify-Native
    & $check -I -m faultline_sim.scientific replay traces/navigation-EASY-planning-42.json
    Verify-Native
} finally { Pop-Location }
gh release edit $tag --repo $repo --draft=false --prerelease
Verify-Native
gh release view $tag --repo $repo --json 'isDraft,isPrerelease,url,assets'
Verify-Native
```

If an upload fails partway through, check the exact PyPI filenames and hashes
before any retry. The frozen Phase 19 kit and local build directories remain
review inputs. The GitHub release excludes probes, model weights, binaries
used for model inference, raw wheels, local logs, draft applications and
authentication material. The PyPI upload contains the five package files.
