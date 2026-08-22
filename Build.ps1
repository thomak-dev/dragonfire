param(
    [string] $Target = 'Build',
    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Debug'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$install = & $vswhere -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -latest -property installationPath
if (-not $install) { 
    throw 'No Visual Studio instance with the C++ toolset found.' 
}

Import-Module (Join-Path $install 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $install -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64 -no_logo'

msbuild (Join-Path $PSScriptRoot 'dragonfire.sln') -nologo -maxCpuCount -nodeReuse:false `
    "-target:$Target" "-property:Configuration=$Configuration" -property:Platform=x64

exit $LASTEXITCODE
