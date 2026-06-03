# MineFramework 依赖预下载脚本
# 用于离线构建准备，将所有第三方库克隆到 third_party/ 目录

param(
    [switch]$Shallow = $true,
    [switch]$UseGitHub = $false  # 默认使用 GitCode，设置此参数使用 GitHub
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "MineFramework 依赖预下载工具" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 创建 third_party 目录
$ThirdPartyDir = Join-Path $PSScriptRoot "..\third_party"
if (-not (Test-Path $ThirdPartyDir)) {
    Write-Host "创建 third_party 目录..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path $ThirdPartyDir | Out-Null
}

# 依赖配置（按需选择镜像源）
$dependencies = @(
    @{
        Name = "freetype"
        GitHub = "https://github.com/freetype/freetype.git"
        GitCode = "https://gitcode.com/gh_mirrors/fr/freetype.git"
        Branch = "master"
    },
    @{
        Name = "harfbuzz"
        GitHub = "https://github.com/harfbuzz/harfbuzz.git"
        GitCode = "https://gitcode.com/gh_mirrors/ha/harfbuzz.git"
        Branch = "main"
    },
    @{
        Name = "utf8cpp"
        GitHub = "https://github.com/nemtrif/utfcpp.git"
        GitCode = "https://gitcode.com/gh_mirrors/ut/utfcpp.git"
        Branch = "master"
    },
    @{
        Name = "stb"
        GitHub = "https://github.com/nothings/stb.git"
        GitCode = "https://gitcode.com/GitHub_Trending/st/stb.git"
        Branch = "master"
    },
    @{
        Name = "zlib"
        GitHub = "https://github.com/madler/zlib.git"
        GitCode = "https://gitcode.com/gh_mirrors/zl/zlib.git"
        Branch = "master"
    },
    @{
        Name = "libpng"
        GitHub = "https://github.com/pnggroup/libpng.git"
        GitCode = "https://gitcode.com/gh_mirrors/li/libpng.git"
        Branch = "libpng16"
    },
    @{
        Name = "sqlite"
        GitHub = "https://github.com/sqlite/sqlite.git"
        GitCode = "https://gitcode.com/gh_mirrors/sq/sqlite.git"
        Branch = "master"
    },
    @{
        Name = "mbedtls"
        GitHub = "https://github.com/Mbed-TLS/mbedtls.git"
        GitCode = "https://gitcode.com/gh_mirrors/mb/mbedtls.git"
        Branch = "master"
    },
    @{
        Name = "doctest"
        GitHub = "https://github.com/doctest/doctest.git"
        GitCode = "https://gitcode.com/gh_mirrors/do/doctest.git"
        Branch = "master"
    }
)

$SuccessCount = 0
$FailedDeps = @()

foreach ($dep in $dependencies) {
    $depPath = Join-Path $ThirdPartyDir $dep.Name
    
    if (Test-Path $depPath) {
        Write-Host "[$($dep.Name)] 已存在，跳过" -ForegroundColor Gray
        $SuccessCount++
        continue
    }
    
    Write-Host "[$($dep.Name)] 下载中..." -ForegroundColor Yellow
    
    # 选择镜像源（默认使用 GitCode 国内镜像）
    $url = if ($UseGitHub) { $dep.GitHub } else { $dep.GitCode }
    
    # 构建 git clone 命令
    $gitArgs = @("clone", $url, $depPath, "--branch", $dep.Branch)
    if ($Shallow) {
        $gitArgs += "--depth", "1"
    }
    $gitArgs += "--progress"
    
    try {
        # 执行克隆
        $process = Start-Process -FilePath "git" -ArgumentList $gitArgs -NoNewWindow -Wait -PassThru
        
        if ($process.ExitCode -eq 0) {
            Write-Host "      ✓ $($dep.Name) 下载完成" -ForegroundColor Green
            $SuccessCount++
        } else {
            Write-Host "      ✗ $($dep.Name) 下载失败（退出码: $($process.ExitCode)）" -ForegroundColor Red
            $FailedDeps += $dep.Name
        }
    }
    catch {
        Write-Host "      ✗ $($dep.Name) 克隆异常: $_" -ForegroundColor Red
        $FailedDeps += $dep.Name
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "下载完成统计" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "成功: $SuccessCount / $($dependencies.Count)" -ForegroundColor Green

if ($FailedDeps.Count -gt 0) {
    Write-Host "失败: $($FailedDeps.Count)" -ForegroundColor Red
    Write-Host "失败列表: $($FailedDeps -join ', ')" -ForegroundColor Red
    Write-Host ""
    Write-Host "提示：" -ForegroundColor Yellow
    Write-Host "  1. 检查网络连接" -ForegroundColor Yellow
    Write-Host "  2. 默认使用 GitCode 国内镜像，失败时尝试 GitHub：./download_deps.ps1 -UseGitHub" -ForegroundColor Yellow
    Write-Host "  3. 手动克隆失败的库到 third_party/<name>/" -ForegroundColor Yellow
} else {
    Write-Host ""
    Write-Host "所有依赖下载成功！" -ForegroundColor Green
    Write-Host "现在可以使用离线构建：" -ForegroundColor Green
    Write-Host "  cmake -B build -DMINE_OFFLINE_BUILD=ON" -ForegroundColor Cyan
}

Write-Host ""
