# Publishes a GitHub release's binaries to Maven Central. Run it locally after the release workflow
# has finished, in PowerShell, so that GPG can ask for its passphrase. It signs and uploads the exact
# files GitHub Actions built and tested, leaving the deployment for approval on the Central website.
#
# Usage: scripts/publish-central.ps1 <version>, or add -DryRun to build and sign nothing, deploying
# to a temporary folder instead.
param(
	[Parameter(Mandatory = $true)][string]$Version,
	[switch]$DryRun
)
$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')

$revision = ([xml](Get-Content pom.xml -Raw)).project.properties.revision
if ($revision -ne $Version) {
	throw "pom.xml is at $revision, not $Version. Check out the tag v$Version."
}

$dist = 'dist'
if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item -ItemType Directory $dist | Out-Null
gh release download "v$Version" --repo jelinj8/Java-BSNative-launcher --dir $dist
if ($LASTEXITCODE -ne 0) { throw 'Release download failed' }

foreach ($line in Get-Content (Join-Path $dist 'SHA256SUMS.txt')) {
	$expected, $file = $line -split '\s+\*?', 2
	$actual = (Get-FileHash (Join-Path $dist $file) -Algorithm SHA256).Hash
	if ($actual -ne $expected) { throw "Checksum mismatch: $file" }
	Write-Host "checksum ok: $file"
}

if ($DryRun) {
	$target = Join-Path ([IO.Path]::GetTempPath()) "bsnative-publish-$PID"
	$uri = 'file:///' + ($target -replace '\\', '/')
	mvn clean deploy --no-transfer-progress "-DaltDeploymentRepository=dry::$uri"
	if ($LASTEXITCODE -ne 0) { throw 'Dry run failed' }
	Get-ChildItem $target -Recurse -File | Where-Object { $_.Extension -notin '.md5', '.sha1', '.sha256', '.sha512' -and $_.Name -notlike 'maven-metadata*' } | ForEach-Object { $_.Name }
	Write-Host "Deployed to $target"
	exit 0
}

mvn clean deploy -P release --no-transfer-progress
if ($LASTEXITCODE -ne 0) { throw 'Deploy failed' }
Write-Host 'Uploaded. Approve it at https://central.sonatype.com/publishing/deployments'
