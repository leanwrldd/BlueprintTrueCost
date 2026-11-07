# One-time chain for a fresh checkout: build the editor target, create the FGGameFeatureData
# asset headlessly, then Alpakit Dev (build, cook, install to the game).

$ErrorActionPreference = "Stop"
$sml     = "C:\SF\SML"
$editor  = "C:\Program Files\Unreal Engine - CSS\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$modRoot = Split-Path $PSScriptRoot

& C:\SF\scripts\build-editor.ps1
if ($LASTEXITCODE -ne 0) { Write-Host "Stopping: editor build failed." -ForegroundColor Red; exit 1 }

if (-not (Test-Path "$modRoot\Content\BlueprintTrueCost.uasset")) {
    Write-Host ""
    Write-Host "Creating FGGameFeatureData asset..." -ForegroundColor Cyan
    # The first run exits 1 with Plugin_Missing_GameFeatureData; the asset is still created.
    foreach ($pass in 1, 2) {
        & $editor "$sml\FactoryGame.uproject" -run=pythonscript `
            "-script=$PSScriptRoot\create_game_feature_data.py" `
            -unattended -nosplash -NoLogTimes "-abslog=$sml\Saved\Logs\BlueprintTrueCost-gfd-$pass.log"
        Write-Host "  pass $pass exit $LASTEXITCODE" -ForegroundColor DarkGray
    }
    if (-not (Test-Path "$modRoot\Content\BlueprintTrueCost.uasset")) {
        Write-Host "Stopping: asset not created. See $sml\Saved\Logs\BlueprintTrueCost-gfd-*.log" -ForegroundColor Red
        exit 1
    }
}

Write-Host ""
& C:\SF\scripts\alpakit.ps1 -Mod BlueprintTrueCost
