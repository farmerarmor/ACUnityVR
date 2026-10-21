param(
 [string]$BinaryDirectory=(Join-Path $PSScriptRoot 'build/bin'),
 [Parameter(Mandatory=$true)][string]$LoaderExecutable,
 [string]$Version='0.1.0'
)
$ErrorActionPreference='Stop'
if($Version -notmatch '^\d+\.\d+\.\d+(?:-[a-zA-Z0-9.]+)?$'){throw 'Invalid version.'}
$dist=Join-Path $PSScriptRoot 'dist'
$stage=Join-Path $dist ('ACUnityVR-'+$Version)
if(Test-Path -LiteralPath $stage){throw "Package directory exists: $stage"}
$package=Join-Path $stage 'ACUnityVR'
New-Item -ItemType Directory -Path $package -Force | Out-Null
foreach($name in @('ACUnityVRNative.dll','ACUnityShaderRegistry.dll','ACUnityVRHost.exe')){
 Copy-Item -LiteralPath (Join-Path $BinaryDirectory $name) -Destination (Join-Path $package $name)
}
Copy-Item -LiteralPath $LoaderExecutable -Destination (Join-Path $package 'ACUnityProbeLoader.exe')
foreach($name in @('README.md','BUILDING.md','THIRD_PARTY.md','LICENSE','ACUnityVR.ini','Start-ACUnityVR.cmd','Start-ACUnityVR.ps1')){
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $package $name)
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'licenses') -Destination $package -Recurse
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'docs') -Destination $package -Recurse
$files=[ordered]@{}
Get-ChildItem -LiteralPath $package -File -Recurse | Sort-Object FullName | ForEach-Object {
 $files[$_.FullName.Substring($package.Length+1).Replace('\','/')]=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
}
$sourceCommit=(& git -c "safe.directory=$($PSScriptRoot.Replace('\','/'))" -C $PSScriptRoot rev-parse HEAD 2>$null)
if($LASTEXITCODE -ne 0){$sourceCommit='uncommitted source build'}
[ordered]@{version=$Version;baseline='r29';abi=9;experiment=76;sourceCommit=$sourceCommit;sha256=$files} |
 ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $package 'payload-manifest.json') -Encoding utf8
$checksums=Get-ChildItem -LiteralPath $package -Recurse -File | Sort-Object FullName | ForEach-Object {
 (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()+'  '+$_.FullName.Substring($package.Length+1).Replace('\','/')
}
$checksums | Set-Content -LiteralPath (Join-Path $package 'checksums.sha256') -Encoding utf8
$archive=Join-Path $dist ('ACUnityVR-'+$Version+'.zip')
if(Test-Path -LiteralPath $archive){throw "Archive exists: $archive"}
Compress-Archive -LiteralPath $package -DestinationPath $archive
$hash=(Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
($hash+'  '+[IO.Path]::GetFileName($archive)) | Set-Content -LiteralPath ($archive+'.sha256') -Encoding utf8
Write-Output "Packaged $archive"
Write-Output "SHA256 $hash"
