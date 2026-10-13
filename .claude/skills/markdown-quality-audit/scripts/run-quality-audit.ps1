param(
    [Parameter(Mandatory = $true)]
    [string]$Target,

    [switch]$FailOnFindings,

    [string]$TocFile = "table-of-contents.md",

    [string]$BacklinkTarget = "table-of-contents.md",

    [string]$BacklinkLabel = ""
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..\..\..\..")).Path
$resolvedTarget = (Resolve-Path (Join-Path $repoRoot $Target)).Path

$venvPython = Join-Path $repoRoot ".venv\Scripts\python.exe"
$python = if (Test-Path $venvPython) { $venvPython } else { "python" }

function Invoke-RepoPythonTool {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ToolPath,

        [Parameter()]
        [string[]]$Arguments = @()
    )

    Write-Host "" 
    Write-Host "> $ToolPath $($Arguments -join ' ')" -ForegroundColor Cyan
    & $python $ToolPath @Arguments
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

Write-Host "Repository root: $repoRoot" -ForegroundColor DarkGray
Write-Host "Target: $resolvedTarget" -ForegroundColor DarkGray

Invoke-RepoPythonTool -ToolPath (Join-Path $repoRoot "tools\check_markdown_links.py") -Arguments @($resolvedTarget)
Invoke-RepoPythonTool -ToolPath (Join-Path $repoRoot "tools\check_unclosed_code_fences.py") -Arguments @($resolvedTarget)

if (Test-Path $resolvedTarget -PathType Container) {
    Invoke-RepoPythonTool -ToolPath (Join-Path $repoRoot "tools\check_code_fences_dir.py") -Arguments @($resolvedTarget)

    $reviewArgs = @($resolvedTarget, "--toc", $TocFile, "--backlink-target", $BacklinkTarget)
    if ($BacklinkLabel -ne "") {
        $reviewArgs += @("--backlink-label", $BacklinkLabel)
    }
    if ($FailOnFindings) {
        $reviewArgs += "--fail"
    }

    Invoke-RepoPythonTool -ToolPath (Join-Path $repoRoot "tools\review_slim_markdown.py") -Arguments $reviewArgs
}
else {
    Write-Host "Skipping directory-only checks because Target is a file." -ForegroundColor Yellow
}