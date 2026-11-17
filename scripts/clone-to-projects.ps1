# Clones / updates this repo into D:\Projects\Github\wireless-battery-g435
$ErrorActionPreference = 'Stop'
$dest = 'D:\Projects\Github\wireless-battery-g435'
$repo = 'https://github.com/rip-en/wireless-battery-g435.git'

New-Item -ItemType Directory -Force -Path 'D:\Projects\Github' | Out-Null

if (Test-Path (Join-Path $dest '.git')) {
    Write-Host "Updating existing clone at $dest"
    git -C $dest fetch origin
    git -C $dest checkout main
    git -C $dest pull --ff-only origin main
} elseif (Test-Path $dest) {
    Write-Error "$dest exists but is not a git repo. Move it aside and re-run."
} else {
    Write-Host "Cloning into $dest"
    git clone $repo $dest
}

Write-Host "Done: $dest"
explorer.exe $dest
