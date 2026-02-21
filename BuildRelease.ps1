param(
    [string]$preset = "release",
    [int]$threads = 8
)

$vsDevShellPath = "C:/Program Files/Microsoft Visual Studio/2022/Professional/Common7/Tools/Launch-VsDevShell.ps1"

# Load in local variable overrides
# local developer copy of Build_Config_Template.ps1 to override $vsDevShellPath or other variables if needed
if (Test-Path .\Build_Config_Local.ps1) {
    . .\Build_Config_Local.ps1
}


# Set up Visual Studio x64 environment
# Override with env var if set
if ($env:VS_DEV_SHELL_PATH) {
    if (Test-Path $env:VS_DEV_SHELL_PATH) {
        $vsDevShellPath = $env:VS_DEV_SHELL_PATH
        Write-Host "Using VS Dev Shell path from environment: $vsDevShellPath"
    }
}
# Verify the path exists
if (-not (Test-Path $vsDevShellPath)) {
    Write-Error "Visual Studio Dev Shell script not found at '$vsDevShellPath'. Please check the path."
    exit 1
}
# Save current directory, launch VS dev shell, and return to original directory
$currentDirectory = $PWD.Path
& $vsDevShellPath -Arch amd64; Set-Location -Path "${currentDirectory}"

Write-Host "Running preset $preset"

# Build cmake configure arguments
$cmakeArgs = @("-S", ".", "--preset=$preset", "-DCMAKE_COMPILE_JOBS=$threads", "-Wno-dev")

& cmake $cmakeArgs
if ($LASTEXITCODE -ne 0) { exit 1 }

& cmake --build --preset=$preset --parallel $threads
if ($LASTEXITCODE -ne 0) { exit 1 }