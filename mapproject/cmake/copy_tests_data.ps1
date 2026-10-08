param(
    [Parameter(Mandatory = $true)]
    [string]$Src,

    [Parameter(Mandatory = $true)]
    [string]$Tgt
)

Write-Host "Source Dir: $Src"
Write-Host "Target Dir: $Tgt"
Write-Host "Start Mirroring..."

# 执行robocopy
robocopy $Src $Tgt /MIR /R:1 /W:1 /Z /MT:16 /FFT
$rc = $LASTEXITCODE

# 当返回值小于8时统一返回0
# https://learn.microsoft.com/zh-cn/windows-server/administration/windows-commands/robocopy
if ($rc -lt 8) {
    $rc = 0
}

Write-Host "Completed, Return Code: $rc"
exit $rc
