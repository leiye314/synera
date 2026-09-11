param(
    [string]$BuildDir = 'build',
    [string]$Configuration = 'Release',
    [string]$Executable,
    [string]$ReportPath
)
$ErrorActionPreference = 'Stop'
try {
    if (-not [IO.Path]::IsPathRooted($BuildDir)) { $BuildDir = Join-Path $PSScriptRoot $BuildDir }
    $BuildDir = [IO.Path]::GetFullPath($BuildDir)
    if (-not $Executable) {
        $candidates = @(
            (Join-Path $BuildDir 'synera_selftest.exe'),
            (Join-Path $BuildDir "$Configuration/synera_selftest.exe"),
            (Join-Path $BuildDir 'synera.exe'),
            (Join-Path $BuildDir "$Configuration/synera.exe")
        )
        $Executable = $candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
    } elseif (-not [IO.Path]::IsPathRooted($Executable)) {
        $Executable = Join-Path $PSScriptRoot $Executable
    }
    if (-not $Executable -or -not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
        throw 'Build the project first, or specify -BuildDir / -Executable.'
    }
    $Executable = [IO.Path]::GetFullPath($Executable)
    if (-not $ReportPath) { $ReportPath = Join-Path $BuildDir 'selftest_report.txt' }
    if (-not [IO.Path]::IsPathRooted($ReportPath)) { $ReportPath = Join-Path $PSScriptRoot $ReportPath }
    $ReportPath = [IO.Path]::GetFullPath($ReportPath)
    # Remove only this generated report so a failed launch cannot display an old success.
    if (Test-Path -LiteralPath $ReportPath) { Remove-Item -LiteralPath $ReportPath }
    $arguments = @('--report', ('"' + $ReportPath + '"'))
    if ([IO.Path]::GetFileNameWithoutExtension($Executable) -eq 'synera') {
        $arguments = @('--selftest') + $arguments
    }
    $process = Start-Process -FilePath $Executable -ArgumentList $arguments -WorkingDirectory $BuildDir -WindowStyle Hidden -Wait -PassThru
    $code = $process.ExitCode
    if (Test-Path -LiteralPath $ReportPath) { Get-Content -LiteralPath $ReportPath -Encoding UTF8 }
    elseif ($code -eq 0) { throw 'The process exited without writing a self-test report.' }
    exit $code
} catch {
    Write-Error $_
    exit 1
}
