#Requires -Version 7.4
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('Build', 'Package', 'Verify')][string]$Stage,
    [Parameter(Mandatory)][string]$Version,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$WixVersion,
    [string]$PreviousMsi
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if (-not $IsWindows -or [Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture -ne 'X64' -or [Runtime.InteropServices.RuntimeInformation]::OSArchitecture -ne 'X64') {
    throw 'Use a native Windows x86-64 runner.'
}
if ($Version -notmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') {
    throw 'A stable X.Y.Z version is required.'
}
$parts = $Version.Split('.') | ForEach-Object { [long]$_ }
if ($parts[0] -gt 255 -or $parts[1] -gt 255 -or $parts[2] -gt 65535) {
    throw 'MSI version limits are 255.255.65535.'
}
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$exe = Join-Path $OutputDirectory 'exe/glasses-broadcaster.exe'
$msi = Join-Path $OutputDirectory "msi/glasses-broadcaster-$Version-windows-10-11-x86_64.msi"
$upgradeCode = '{167CB7C3-243A-41D2-8E59-D28BEEC9F785}'
$pathCode = '{342BC5F4-C0AE-49C2-860B-10E43D11D739}'

function Invoke-Checked([string]$Command, [string[]]$Arguments) {
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Command failed with exit code $LASTEXITCODE" }
}

function Get-ComValue($Object, [string]$Name, [object[]]$Arguments = @()) {
    return ,($Object.GetType().InvokeMember($Name, [Reflection.BindingFlags]::GetProperty, $null, $Object, $Arguments))
}

function Invoke-ComCall($Object, [string]$Name, [object[]]$Arguments = @()) {
    return ,($Object.GetType().InvokeMember($Name, [Reflection.BindingFlags]::InvokeMethod, $null, $Object, $Arguments))
}

function Close-ComObject($Object) {
    if ($null -ne $Object -and [Runtime.InteropServices.Marshal]::IsComObject($Object)) {
        [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($Object)
    }
}

function Read-MsiMetadata([string]$Path) {
    $installer = New-Object -ComObject WindowsInstaller.Installer
    $database = $null
    $summary = $null
    try {
        $database = Invoke-ComCall $installer 'OpenDatabase' @($Path, 0)
        $values = @{}
        foreach ($property in @('ProductVersion', 'ProductCode', 'UpgradeCode')) {
            $sql = 'SELECT `Value` FROM `Property` WHERE `Property`=''{0}''' -f $property
            $view = Invoke-ComCall $database 'OpenView' @($sql)
            $record = $null
            try {
                $null = Invoke-ComCall $view 'Execute'
                $record = Invoke-ComCall $view 'Fetch'
                if ($null -eq $record) { throw "Missing MSI property: $property" }
                $values[$property] = Get-ComValue $record 'StringData' @(1)
            } finally {
                Close-ComObject $record
                Close-ComObject $view
            }
        }
        $view = Invoke-ComCall $database 'OpenView' @('SELECT `ComponentId` FROM `Component` WHERE `Component`=''Path''')
        $record = $null
        try {
            $null = Invoke-ComCall $view 'Execute'
            $record = Invoke-ComCall $view 'Fetch'
            $values.PathCode = Get-ComValue $record 'StringData' @(1)
        } finally { Close-ComObject $record; Close-ComObject $view }
        $summary = Get-ComValue $installer 'SummaryInformation' @($Path, 0)
        $values.Template = Get-ComValue $summary 'Property' @(7)
        return [pscustomobject]$values
    } finally {
        Close-ComObject $summary
        Close-ComObject $database
        Close-ComObject $installer
    }
}

function Invoke-Msi([string[]]$Arguments, [string]$LogName) {
    $log = Join-Path $OutputDirectory "$LogName.log"
    $process = Start-Process msiexec.exe -ArgumentList ($Arguments + @('/qn', '/norestart', '/L*v', "`"$log`"")) -Wait -PassThru
    if ($process.ExitCode -notin @(0, 3010)) { throw "MSI failed ($($process.ExitCode)); see $log" }
}

function Test-InstalledCatalog($Installer, [string]$ProductCode, [bool]$ExpectPath) {
    $installedRoot = Join-Path $env:ProgramFiles 'glasses-broadcaster'
    $bin = Join-Path $installedRoot 'bin'
    $installedExe = Join-Path $bin 'glasses-broadcaster.exe'
    if ((Get-FileHash $installedExe).Hash -ne (Get-FileHash $exe).Hash) { throw 'Installed executable differs from the built executable.' }
    $sourceData = Join-Path $root 'data'
    $installedData = Join-Path $bin 'data'
    $sourceFiles = @(Get-ChildItem -LiteralPath $sourceData -Recurse -File)
    $installedFiles = @(Get-ChildItem -LiteralPath $installedData -Recurse -File)
    if ($sourceFiles.Count -ne $installedFiles.Count) { throw 'Installed data file count mismatch.' }
    foreach ($file in $sourceFiles) {
        $relative = [IO.Path]::GetRelativePath($sourceData, $file.FullName)
        $destination = Join-Path $installedData $relative
        if ((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath $destination).Hash) { throw "Data mismatch: $relative" }
    }
    $launcher = Join-Path $bin 'launch.cmd'
    if ((Get-FileHash $launcher).Hash -ne (Get-FileHash (Join-Path $PSScriptRoot 'launch.cmd')).Hash) { throw 'Installed launcher mismatch.' }
    Push-Location $env:SystemRoot
    try {
        $null = Invoke-Checked $installedExe @('--help')
        $reportedVersion = (Invoke-Checked $installedExe @('--version') | Out-String).Trim()
        if ($reportedVersion -ne "glasses-broadcaster $Version") { throw 'Installed executable version mismatch.' }
        Invoke-Checked $installedExe @('--check-data')
        Invoke-Checked $launcher @('--check-data')
    } finally { Pop-Location }
    $shortcut = Join-Path ([Environment]::GetFolderPath('CommonPrograms')) 'Glasses Broadcaster.lnk'
    if (-not (Test-Path -LiteralPath $shortcut)) { throw 'Start-menu shortcut missing.' }
    $target = Get-ComValue $Installer 'ShortcutTarget' @($shortcut)
    try {
        $shortcutProduct = Get-ComValue $target 'StringData' @(1)
        $shortcutComponent = Get-ComValue $target 'StringData' @(3)
        $resolved = Get-ComValue $Installer 'ComponentPath' @($shortcutProduct, $shortcutComponent)
        if ($shortcutProduct -ne $ProductCode -or $resolved -ne $launcher) { throw 'Start-menu shortcut points at the wrong launcher.' }
    } finally { Close-ComObject $target }
    $machinePath = [Environment]::GetEnvironmentVariable('Path', 'Machine')
    $hasPath = @($machinePath.Split(';') | ForEach-Object { $_.TrimEnd('\') }) -contains $bin.TrimEnd('\')
    if ($hasPath -ne $ExpectPath) { throw 'PATH feature state was not preserved.' }
}

Push-Location $root
try {
    switch ($Stage) {
        Build {
            New-Item -ItemType Directory -Force (Split-Path $exe) | Out-Null
            $env:CARGO_TARGET_DIR = Join-Path $OutputDirectory 'cargo-target'
            $env:RUSTFLAGS = '-C target-feature=+crt-static'
            Remove-Item Env:CARGO_ENCODED_RUSTFLAGS -ErrorAction SilentlyContinue
            $metadata = Invoke-Checked cargo @('metadata', '--locked', '--no-deps', '--format-version', '1') | Out-String | ConvertFrom-Json
            $crate = $metadata.packages | Where-Object name -eq 'glasses-broadcaster'
            if ($crate.version -ne $Version) { throw 'Installer version must exactly match Cargo.toml.' }
            Invoke-Checked cargo @('test', '--locked', '--all-features', '--target', 'x86_64-pc-windows-msvc')
            Invoke-Checked cargo @('build', '--locked', '--release', '--all-features', '--target', 'x86_64-pc-windows-msvc')
            Copy-Item -LiteralPath (Join-Path $env:CARGO_TARGET_DIR 'x86_64-pc-windows-msvc/release/glasses-broadcaster.exe') -Destination $exe
            $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
            $dumpbin = @(Invoke-Checked $vswhere @('-latest', '-products', '*', '-find', 'VC\Tools\MSVC\**\bin\Hostx64\x64\dumpbin.exe')) | Select-Object -First 1
            if (-not $dumpbin) { throw 'MSVC dumpbin is required for static-runtime verification.' }
            $dependencies = Invoke-Checked $dumpbin @('/dependents', $exe) | Out-String
            if ($dependencies -match '(?i)(vcruntime|msvcp|ucrtbase)[a-z0-9_]*\.dll') { throw 'The executable still depends on a dynamic MSVC runtime.' }
        }
        Package {
            if ([string]::IsNullOrWhiteSpace($WixVersion)) { throw 'Pass the pinned WiX version.' }
            $toolDir = Join-Path $OutputDirectory 'wix-tools'
            New-Item -ItemType Directory -Force $toolDir, (Split-Path $msi) | Out-Null
            $wix = Join-Path $toolDir 'wix.exe'
            if (-not (Test-Path $wix)) { Invoke-Checked dotnet @('tool', 'install', 'wix', '--version', $WixVersion, '--tool-path', $toolDir) }
            $reported = (Invoke-Checked $wix @('--version') | Out-String).Trim()
            if ($reported -notmatch ('^' + [regex]::Escape($WixVersion) + '(\+.*)?$')) { throw "Unexpected WiX version: $reported" }
            Push-Location $toolDir
            try {
                Invoke-Checked $wix @('extension', 'add', "WixToolset.UI.wixext/$WixVersion")
                Invoke-Checked $wix @('build', (Join-Path $root 'wix/main.wxs'), '-arch', 'x64', '-ext', 'WixToolset.UI.wixext', '-d', "Version=$Version", '-d', "CargoTargetBinDir=$(Split-Path $exe)", '-d', "DataDir=$(Join-Path $root 'data')", '-d', "LauncherPath=$(Join-Path $PSScriptRoot 'launch.cmd')", '-o', $msi)
            } finally { Pop-Location }
        }
        Verify {
            $info = Read-MsiMetadata $msi
            if ($info.ProductVersion -ne $Version -or $info.UpgradeCode -ne $upgradeCode -or $info.PathCode -ne $pathCode -or $info.Template.Split(';')[0] -ne 'x64') { throw 'MSI version, architecture, or upgrade identity mismatch.' }
            $installer = New-Object -ComObject WindowsInstaller.Installer
            $related = Get-ComValue $installer 'RelatedProducts' @($upgradeCode)
            $installedRoot = Join-Path $env:ProgramFiles 'glasses-broadcaster'
            $shortcut = Join-Path ([Environment]::GetFolderPath('CommonPrograms')) 'Glasses Broadcaster.lnk'
            $machinePathBefore = [Environment]::GetEnvironmentVariable('Path', 'Machine')
            $ownedProducts = [Collections.Generic.List[string]]::new()
            try {
                if ((Get-ComValue $related 'Count') -ne 0 -or (Test-Path $installedRoot) -or (Test-Path $shortcut)) { throw 'Installation tests require a clean runner; refusing to overwrite an existing installation.' }
                $ownedProducts.Add($info.ProductCode)
                Invoke-Msi @('/i', "`"$msi`"") 'fresh-install'
                Test-InstalledCatalog $installer $info.ProductCode $true
                Invoke-Msi @('/x', $info.ProductCode) 'fresh-uninstall'
                if ((Test-Path $installedRoot) -or (Test-Path $shortcut) -or [Environment]::GetEnvironmentVariable('Path', 'Machine') -ne $machinePathBefore) { throw 'Uninstallation left files, shortcuts, or PATH changes.' }
                $upgradeChecked = $false
                if (-not [string]::IsNullOrWhiteSpace($PreviousMsi)) {
                    $PreviousMsi = [IO.Path]::GetFullPath($PreviousMsi)
                    $old = Read-MsiMetadata $PreviousMsi
                    if ($old.UpgradeCode -ne $upgradeCode -or [version]$old.ProductVersion -ge [version]$Version) { throw 'The prior MSI must be an older version of this product.' }
                    $ownedProducts.Add($old.ProductCode)
                    Invoke-Msi @('/i', "`"$PreviousMsi`"", 'REMOVE=Environment') 'previous-install'
                    Invoke-Msi @('/i', "`"$msi`"") 'upgrade-install'
                    Test-InstalledCatalog $installer $info.ProductCode $false
                    if ((Get-ComValue $installer 'ProductState' @($old.ProductCode)) -ne -1) { throw 'The old product remained registered after upgrade.' }
                    $upgradeChecked = $true
                }
            } finally {
                foreach ($code in $ownedProducts) {
                    if ((Get-ComValue $installer 'ProductState' @($code)) -ne -1) { Invoke-Msi @('/x', $code) 'cleanup-uninstall' }
                }
                Close-ComObject $related
                Close-ComObject $installer
            }
            if ((Test-Path $installedRoot) -or (Test-Path $shortcut) -or [Environment]::GetEnvironmentVariable('Path', 'Machine') -ne $machinePathBefore) { throw 'Final removal verification failed.' }
            @{ version = $Version; architecture = 'x86_64'; signed = $false; installed = $true; uninstalled = $true; upgrade_checked = $upgradeChecked; sha256 = (Get-FileHash -Algorithm SHA256 $msi).Hash.ToLowerInvariant() } | ConvertTo-Json | Set-Content -Encoding utf8 (Join-Path $OutputDirectory 'windows-verification.json')
        }
    }
} finally { Pop-Location }