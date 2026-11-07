# Creates /BlueprintTrueCost/BlueprintTrueCost, the FGGameFeatureData asset a game feature mod
# needs. Run through the editor's Python commandlet after the editor target is built:
#   UnrealEditor-Cmd.exe C:\SF\SML\FactoryGame.uproject -run=pythonscript
#     -script=C:\SF\Mods\BlueprintTrueCost\Scripts\create_game_feature_data.py
#     -unattended -nosplash -NoLogTimes -abslog=<log path>
# The first run logs Plugin_Missing_GameFeatureData and exits 1 because the plugin registers
# before the asset exists; that is expected. A second run should report the feature Active.
import unreal

ROOT = "/BlueprintTrueCost"
NAME = "BlueprintTrueCost"
PATH = f"{ROOT}/{NAME}"

# The commandlet has not scanned plugin content yet, so does_asset_exist alone says False for
# an asset already on disk and create_asset then fails unattended.
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([ROOT], True)

lib = unreal.EditorAssetLibrary
if lib.does_asset_exist(PATH):
    unreal.log(f"BTC: {PATH} already exists, nothing to do")
else:
    cls = unreal.load_class(None, "/Script/FactoryGame.FGGameFeatureData")
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, ROOT, cls, factory)
    if asset is None:
        unreal.log_error(f"BTC: failed to create {PATH}")
    else:
        lib.save_loaded_asset(asset)
        unreal.log(f"BTC: created {PATH}")
