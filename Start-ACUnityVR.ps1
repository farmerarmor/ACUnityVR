param([string]$GameExecutable='', [switch]$WaitForGame, [switch]$ValidateOnly)
$ErrorActionPreference='Stop'
$folder=$PSScriptRoot
$manifest=Get-Content -LiteralPath (Join-Path $folder 'payload-manifest.json') -Raw | ConvertFrom-Json
foreach($name in @('ACUnityVRNative.dll','ACUnityShaderRegistry.dll','ACUnityVRHost.exe','ACUnityProbeLoader.exe')) {
 $file=Join-Path $folder $name
 if(-not (Test-Path -LiteralPath $file -PathType Leaf) -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $manifest.sha256.$name){throw "Missing or changed package file: $name. Extract the complete mod again."}
}
if(-not $GameExecutable){foreach($candidate in @((Join-Path $folder 'ACU.exe'),(Join-Path (Split-Path $folder -Parent) 'ACU.exe'))){if(Test-Path -LiteralPath $candidate -PathType Leaf){$GameExecutable=$candidate;break}}}
if(-not $GameExecutable){throw 'Extract the ACUnityVR folder beside ACU.exe, or supply -GameExecutable with its full path.'}
$GameExecutable=(Resolve-Path -LiteralPath $GameExecutable).Path
if((Get-FileHash -LiteralPath $GameExecutable -Algorithm SHA256).Hash -ne '748ABDC6456397FE7D8080F975F2A5ED3123BCE548E7BF1C68B23F47E8B27E40'){throw 'This Unity executable does not match the supported build.'}
if($ValidateOnly){Write-Output 'PASS portable payload and supported game hashes. No game, settings or headset changes.';return}
if(@(Get-Process -Name ACU -ErrorAction SilentlyContinue).Count){throw 'Close Unity first, then start it through this launcher. Shader identification must begin during startup.'}
$launcherMutex=New-Object System.Threading.Mutex($false,'Local\ACUnityVRLauncher')
if(-not $launcherMutex.WaitOne(0)){throw 'The Unity VR launcher is already running.'}
try {
 $hostExe=Join-Path $folder 'ACUnityVRHost.exe'
 $query=& $hostExe --display-query
 if($LASTEXITCODE -ne 0 -or ($query -join "`n") -notmatch 'OpenXR eye=(\d+)x(\d+) refresh=([0-9.]+) Hz'){throw 'Connect your headset and activate its OpenXR connection, then restart this launcher.'}
 $width=[int]$Matches[1];$height=[int]$Matches[2];$refresh=[double]::Parse($Matches[3],[Globalization.CultureInfo]::InvariantCulture)
 if($width -lt 1 -or $width -gt 16384 -or $height -lt 1 -or $height -gt 8192 -or [double]::IsNaN($refresh) -or [double]::IsInfinity($refresh) -or $refresh -lt 0 -or $refresh -gt 1000){throw 'Invalid headset display settings.'}
 if(-not ('ACUnityVRProfile92' -as [type])){Add-Type -TypeDefinition 'using System.Runtime.InteropServices; public static class ACUnityVRProfile92 { [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern bool WritePrivateProfileString(string section,string key,string value,string path); [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] public static extern uint GetPrivateProfileInt(string section,string key,int fallback,string path); }'}
 $settings=Join-Path $folder 'ACUnityVR.ini'
 $gameConfig=Join-Path ([Environment]::GetFolderPath('MyDocuments')) "Assassin's Creed Unity\ACU.ini"
 if(-not (Test-Path -LiteralPath $gameConfig -PathType Leaf)){throw 'Unity settings were not found. Run the game normally once, close it, then use this launcher.'}
 $display=@{WindowMode='0';DisplayWidth=[string]$width;DisplayHeight=[string]$height;VSync='0'}
 if($refresh -gt 1){$display.RefreshRate=[string][int][Math]::Round($refresh)}
 $changed=$false
 foreach($key in $display.Keys){if([ACUnityVRProfile92]::GetPrivateProfileInt('Graphics',$key,-1,$gameConfig) -ne [int]$display[$key]){$changed=$true}}
 if($changed){$backups=Join-Path $folder 'display-backups';New-Item -ItemType Directory -Path $backups -Force | Out-Null;Copy-Item -LiteralPath $gameConfig -Destination (Join-Path $backups ('ACU-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmssfff')+'.ini'))
  foreach($key in $display.Keys){if(-not [ACUnityVRProfile92]::WritePrivateProfileString('Graphics',$key,$display[$key],$gameConfig)){throw "Could not save Unity display setting $key."}}
 }
 $values=@{EyeWidth=[string]$width;EyeHeight=[string]$height;RefreshHz=$refresh.ToString('R',[Globalization.CultureInfo]::InvariantCulture)}
 foreach($key in $values.Keys){if(-not [ACUnityVRProfile92]::WritePrivateProfileString('VR',$key,$values[$key],$settings)){throw "Could not save headset setting $key."}}
 Write-Host 'Load your save normally. VR will activate automatically in gameplay. No collector keys or second launcher are needed.'
 if($WaitForGame){Write-Host 'Waiting for Unity. Launch it normally through Steam or Ubisoft Connect.'}else{Start-Process -FilePath $GameExecutable -WorkingDirectory (Split-Path $GameExecutable -Parent) | Out-Null}
 $attempted=@{};$loaded=@{};$started=Get-Date
 $loader=Join-Path $folder 'ACUnityProbeLoader.exe';$registry=Join-Path $folder 'ACUnityShaderRegistry.dll';$startup=Join-Path $folder 'ACUnityVR-startup.log'
 while($true){
  foreach($game in @(Get-Process -Name ACU -ErrorAction SilentlyContinue)){
   if($attempted.ContainsKey($game.Id)){continue}
   try{$path=$game.Path;$launchTime=$game.StartTime;$modules=@($game.Modules)}catch{continue}
   if(-not $path -or $path -ne $GameExecutable -or $launchTime -lt $started.AddSeconds(-1)){continue}
   if(@($modules | Where-Object {$_.ModuleName -like 'ACUnityTracked*.dll' -or $_.ModuleName -in @('ACUnityVRNative.dll','ACUnityShaderRegistry.dll','ACUnityShaderObserve75.dll')}).Count){throw 'A Unity VR component is already loaded. Restart the game; a second load will not be attempted.'}
   if($game.HasExited){continue}
   $attempted[$game.Id]=$true
   & $loader $game.Id $registry
   if($LASTEXITCODE -ne 0){if(-not (Get-Process -Id $game.Id -ErrorAction SilentlyContinue)){continue};throw 'VR startup could not attach. Restart the game before retrying; no second load is attempted.'}
   $loaded[$game.Id]=$true
  }
  foreach($gameId in @($loaded.Keys)){
   if(-not (Get-Process -Id $gameId -ErrorAction SilentlyContinue)){$loaded.Remove($gameId);continue}
   if(Test-Path -LiteralPath $startup){$status=Get-Content -LiteralPath $startup -Raw
    if($status -match ('Automatic startup pid='+$gameId+';')){
     if($status -match '(?m)^ERROR '){throw "Automatic VR startup failed. See $startup. Restart Unity before retrying."}
     if($status -match 'VR host started pid='){Write-Host 'Unity VR is running. F9 recenters. Close Unity normally when finished.';return}
    }
   }
  }
  # The game can replace its first process during startup. Continue watching
  # fresh processes; never inject the same process twice.
  Start-Sleep -Milliseconds 100
 }
} finally {$launcherMutex.ReleaseMutex();$launcherMutex.Dispose()}

