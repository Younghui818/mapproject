param(
    [Parameter(Mandatory = $true)]
    [string]$Src,

    [Parameter(Mandatory = $true)]
    [string]$Tgt,

    # 可选参数，接收用空格或逗号分隔的多个通配符
    [Parameter(Mandatory = $false)]
    [string]$Exclude
)

Write-Host "------------------------------------------"
Write-Host "Source Directory: $Src"
Write-Host "Target Directory: $Tgt"

# 构造 robocopy 参数数组
$robocopyArgs = @($Src, $Tgt, "/E", "/R:1", "/W:1", "/MT:16")

# 如果提供了过滤条件，则动态添加 /XF 参数
if ($Exclude) {
    # 拆分成数组，支持空格或逗号分隔
    $ExcludeList = $Exclude -split '[ ,]+'

    Write-Host "Excluding Files:"
    $ExcludeList | ForEach-Object { Write-Host "  $_" }

    $robocopyArgs += "/XF"
    $robocopyArgs += $ExcludeList
}

Write-Host "Start Copying..."
Write-Host "------------------------------------------"

# 执行 robocopy
& robocopy @robocopyArgs

# 获取退出码
$rc = $LASTEXITCODE

<#
  Robocopy 退出码说明:
  0-7: 成功 (0: 无变化, 1: 拷贝成功, 2: 额外文件, 4: 覆盖/更新)
  8+: 存在失败或严重错误
#>
if ($rc -lt 8) {
    $finalStatus = 0
    Write-Host "Completed Successfully." -ForegroundColor Green
} else {
    $finalStatus = $rc
    Write-Error "Robocopy failed with exit code $rc."
}

exit $finalStatus
