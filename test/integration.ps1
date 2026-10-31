# Runs the built launcher against a real JDK, on any platform, with PowerShell 7.
# Usage: integration.ps1 <launcher binary> <JDK home>
param([string]$Launcher, [string]$JavaHome)
$ErrorActionPreference = 'Stop'
$failures = 0
$env:BSNATIVE_LAUNCHER_NO_DIALOG = '1'
$env:JAVA_HOME = $JavaHome

$work = Join-Path ([IO.Path]::GetTempPath()) "bsnative-it-$PID"
New-Item -ItemType Directory $work | Out-Null
$work = (Resolve-Path $work).Path
$classes = Join-Path $work 'classes'
& (Join-Path $JavaHome 'bin/javac') --release 11 -d $classes (Join-Path $PSScriptRoot 'app/TestApp.java')
$jar = Join-Path $work 'testapp.jar'
& (Join-Path $JavaHome 'bin/jar') --create --file $jar -C $classes .

function New-App($Dir, $Ini, $Name = 'MyApp') {
	New-Item -ItemType Directory (Join-Path $Dir 'lib') -Force | Out-Null
	Copy-Item $jar (Join-Path $Dir 'lib/testapp.jar')
	$exe = Join-Path $Dir $(if ($IsWindows) { "$Name.exe" } else { $Name })
	Copy-Item $Launcher $exe
	if (-not $IsWindows) { & chmod +x $exe }
	if ($null -ne $Ini) {
		[IO.File]::WriteAllText((Join-Path $Dir "$Name.ini"), $Ini, (New-Object Text.UTF8Encoding($false)))
	}
	return $exe
}

function Invoke-App($Exe, $Arguments = @()) {
	$out = Join-Path $work 'out.txt'
	$err = Join-Path $work 'err.txt'
	# from an unrelated folder, so working-dir tests prove something
	$info = @{ FilePath = $Exe; Wait = $true; PassThru = $true; RedirectStandardOutput = $out; RedirectStandardError = $err; WorkingDirectory = $work }
	if ($Arguments.Count) { $info.ArgumentList = $Arguments }
	$process = Start-Process @info
	return @{
		Code = $process.ExitCode
		Out = [IO.File]::ReadAllText($out, [Text.Encoding]::UTF8)
		Err = [IO.File]::ReadAllText($err, [Text.Encoding]::UTF8)
	}
}

function Check($Name, $Condition, $Detail) {
	if ($Condition) { Write-Host "ok   $Name" } else { Write-Host "FAIL $Name`n$($Detail | Out-String)"; $script:failures++ }
}

function WorkDirIs($Result, $Dir) {
	return $Result.Out -match ('workdir=' + [regex]::Escape($Dir) + '\r?\n')
}

$basic = "main-class=TestApp`nclasspath=lib/*`n"

$exe = New-App (Join-Path $work 'basic') ($basic + "jvm-option=-Dtestprop=fromini`narg=first`n")
$r = Invoke-App $exe @('second', '"with space"')
Check 'runs, passing options and arguments in order' ($r.Code -eq 0 -and $r.Out -match 'testprop=fromini' -and $r.Out -match "arg=first\r?\narg=second\r?\narg=with space\r?\nmain finished") $r

$r = Invoke-App $exe @('exit:7')
Check 'System.exit code passes through' ($r.Code -eq 7) $r

$r = Invoke-App $exe @('throw')
Check 'uncaught exception gives code 1 and a stack trace' ($r.Code -eq 1 -and $r.Err -match 'expected by the test') $r

$r = Invoke-App $exe @('thread')
Check 'non-daemon thread keeps the app alive' ($r.Code -eq 0 -and $r.Out -match 'main finished\r?\nthread finished') $r

$env:BSNATIVE_LAUNCHER_DRY_RUN = '1'
$r = Invoke-App $exe
Check 'dry run reports the selected Java' ($r.Code -eq 0 -and $r.Out -match [regex]::Escape("home=$JavaHome")) $r
Remove-Item env:BSNATIVE_LAUNCHER_DRY_RUN

$exe = New-App (Join-Path $work 'renamed') $basic 'Another Name'
$r = Invoke-App $exe
Check 'finds the ini by its own name, with a space' ($r.Code -eq 0 -and $r.Out -match 'main finished') $r

$exe = New-App (Join-Path $work 'dotted') $basic 'my.app'
$r = Invoke-App $exe
Check 'a dot in the name stays part of it' ($r.Code -eq 0 -and $r.Out -match 'main finished') $r

$r = Invoke-App (New-App (Join-Path $work 'noini') $null)
Check 'missing ini is reported' ($r.Code -eq 2 -and $r.Err -match 'Configuration file not found') $r

$r = Invoke-App (New-App (Join-Path $work 'badkey') ($basic + "mistake=1`n"))
Check 'unknown key is reported' ($r.Code -eq 2 -and $r.Err -match 'unknown key: mistake') $r

$r = Invoke-App (New-App (Join-Path $work 'noclass') "main-class=no.such.Main`nclasspath=lib/*`n")
Check 'missing main class is reported' ($r.Code -eq 2 -and $r.Err -match 'no.such.Main') $r

$r = Invoke-App (New-App (Join-Path $work 'badopt') ($basic + "jvm-option=-XX:NoSuchOption`n"))
Check 'bad JVM option is reported' ($r.Code -eq 2 -and $r.Err -match 'Java failed to start') $r

$r = Invoke-App (New-App (Join-Path $work 'toonew') ($basic + "min-java=99`n"))
Check 'no suitable Java lists what was tried' ($r.Code -eq 2 -and $r.Err -match 'No suitable Java' -and $r.Err -match 'JAVA_HOME' -and $r.Err -match 'older than min-java 99') $r

$dir = Join-Path $work 'explicit'
$r = Invoke-App (New-App $dir ($basic + "runtime=$JavaHome`n"))
Check 'explicit runtime path' ($r.Code -eq 0) $r

$dir = Join-Path $work 'bundled'
$exe = New-App $dir $basic
$itemType = if ($IsWindows) { 'Junction' } else { 'SymbolicLink' }
New-Item -ItemType $itemType (Join-Path $dir 'runtime') -Target $JavaHome | Out-Null
$saved = $env:JAVA_HOME
$env:JAVA_HOME = (Join-Path $work 'no-such-java')
$env:BSNATIVE_LAUNCHER_DRY_RUN = '1'
$r = Invoke-App $exe
Check 'bundled runtime wins over a broken JAVA_HOME' ($r.Code -eq 0 -and $r.Out -match [regex]::Escape('home=' + (Join-Path $dir 'runtime'))) $r
Remove-Item env:BSNATIVE_LAUNCHER_DRY_RUN
$r = Invoke-App $exe
Check 'and it really runs' ($r.Code -eq 0 -and $r.Out -match 'main finished') $r
$env:JAVA_HOME = $saved

# Characters within the Windows-1250 code page on Czech systems; on other systems the launcher must
# either run or say clearly why it can't.
$dir = Join-Path $work ("p" + [char]0x0159 + [char]0x00ED + "li" + [char]0x0161 + " app")
$r = Invoke-App (New-App $dir ($basic + "arg=" + [char]0x010D + "esky`n"))
$clear = $r.Code -eq 0 -or $r.Err -match 'code page'
Check 'non-ASCII path runs or fails clearly' $clear $r
if ($r.Code -eq 0) { Check 'non-ASCII argument arrives' ($r.Out -match 'main finished') $r }

$dir = Join-Path $work 'wd-default'
$r = Invoke-App (New-App $dir $basic)
Check 'working directory defaults to the app folder' (WorkDirIs $r $dir) $r

$dir = Join-Path $work 'wd-inherit'
$r = Invoke-App (New-App $dir ($basic + "working-dir=inherit`n"))
Check 'working-dir=inherit keeps the caller''s folder' (WorkDirIs $r $work) $r

$dir = Join-Path $work 'wd-relative'
New-Item -ItemType Directory (Join-Path $dir 'data') | Out-Null
$r = Invoke-App (New-App $dir ($basic + "working-dir=data`n"))
Check 'a relative working-dir is under the app folder' (WorkDirIs $r (Join-Path $dir 'data')) $r

$r = Invoke-App (New-App (Join-Path $work 'wd-bad') ($basic + "working-dir=no-such-folder`n"))
Check 'a missing working-dir is reported' ($r.Code -eq 2 -and $r.Err -match 'working-dir') $r

$log = Join-Path $work 'logged'
$r = Invoke-App (New-App $log ($basic + "log-file=logs/deep/app.log`n"))
$logFile = Join-Path $log 'logs/deep/app.log'
Check 'log-file receives output, creating folders' ((Test-Path $logFile) -and ((Get-Content $logFile -Raw) -match 'main finished')) $r

Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
if ($failures) { Write-Host "$failures FAILED"; exit 1 }
Write-Host 'all passed'
