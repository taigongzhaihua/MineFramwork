$src = "g:\Documents\Visual Studio Code\MineFramwork\src\mine\paint\src\RhiRenderer.cpp"
$lines = [System.IO.File]::ReadAllLines($src, [System.Text.UTF8Encoding]::new($false))
Write-Output "总行数: $($lines.Count)"
$seg1s=$seg1e=$seg2s=$seg2e=-1
for ($i=0;$i -lt $lines.Count;$i++) {
    $l = $lines[$i].Trim()
    if ($seg1s -lt 0 -and $lines[$i] -like '*k_sdf_pixel_shader_hlsl*') { $seg1s=$i }
    if ($seg1s -ge 0 -and $seg1e -lt 0 -and $l -eq ')hlsl"') { $seg1e=$i }
    if ($seg1e -ge 0 -and $seg2s -lt 0 -and $l -eq 'R"hlsl(') { $seg2s=$i }
    if ($seg2s -ge 0 -and $seg2e -lt 0 -and $l -eq ')hlsl";') { $seg2e=$i }
}
Write-Output "段1: 行$($seg1s+1)-$($seg1e+1)"
Write-Output "段2: 行$($seg2s+1)-$($seg2e+1)"
$enc = [System.Text.UTF8Encoding]::new($false)
$s1 = [string]::Join("`n", $lines[$seg1s..$seg1e])
$s2 = [string]::Join("`n", $lines[$seg2s..$seg2e])
Write-Output "段1字节: $($enc.GetByteCount($s1))"
Write-Output "段2字节: $($enc.GetByteCount($s2))"
