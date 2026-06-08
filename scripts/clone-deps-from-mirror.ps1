#!/usr/bin/env pwsh
<#
.SYNOPSIS
    从多个镜像站点依次尝试克隆项目依赖到 third_party/
.DESCRIPTION
    按 scripts/deps.json 中配置的 mirrors 优先级依次尝试 git clone，
    第一个成功的镜像会被使用。若目标目录已存在则跳过（-ForceUpdate 强制更新）。
.PARAMETER DepsFile
    依赖清单 JSON 文件路径，默认 "scripts/deps.json"
.PARAMETER ForceUpdate
    若目标目录已存在，是否强制执行 git pull 更新
.EXAMPLE
    .\scripts\clone-deps-from-mirror.ps1
    .\scripts\clone-deps-from-mirror.ps1 -ForceUpdate
#>
Param(
    [string]$DepsFile = "scripts/deps.json",
    [switch]$ForceUpdate
)

function Write-Log([string]$msg) { Write-Host "[clone-deps] $msg" }

if (-not (Test-Path $DepsFile)) {
    Write-Error "Deps file not found: $DepsFile"
    exit 2
}

$json = Get-Content -Raw $DepsFile | ConvertFrom-Json

foreach ($dep in $json.dependencies) {
    $name   = $dep.name
    $branch = $dep.branch
    $dest   = $dep.dest
    if ([string]::IsNullOrWhiteSpace($dest)) { $dest = "third_party/$name" }
    $destPath = Join-Path (Get-Location) $dest

    if (Test-Path $destPath) {
        if (-not $ForceUpdate) {
            Write-Log "Already exists: $name -> $destPath (use -ForceUpdate to refresh)"
            continue
        }
        Write-Log "Updating existing: $name"
        Push-Location $destPath
        try {
            git fetch --all --prune
            git checkout $branch -q
            git pull --rebase -q
            Write-Log "Updated: $name"
        } catch {
            Write-Log "Update failed for $name : $($_.Exception.Message)"
        }
        Pop-Location
        continue
    }

    $cloned = $false
    foreach ($mirror in $dep.mirrors) {
        $url = $mirror
        if ($dep.repo -and ($mirror -notmatch [regex]::Escape($dep.repo))) {
            $url = ($mirror.TrimEnd('/') + '/' + $dep.repo.TrimStart('/'))
        }
        Write-Log "Trying mirror: $name <- $url (branch: $branch)"
        try {
            git clone --depth 1 --branch $branch $url $destPath 2>&1 | Out-Null
            if ($LASTEXITCODE -eq 0) {
                Write-Log "Cloned OK: $name"
                $cloned = $true
                break
            }
        } catch {
            Write-Log "Mirror failed: $url -- $($_.Exception.Message)"
        }
    }

    if (-not $cloned) {
        Write-Error "All mirrors failed for: $name"
    }
}

Write-Log "Done."
