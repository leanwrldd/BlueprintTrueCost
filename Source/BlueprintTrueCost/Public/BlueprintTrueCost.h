#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBlueprintTrueCost, Log, All)

class AFGBlueprintHologram;

class FBlueprintTrueCostModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/// Replaces the selected blueprint's stored cost with one counted from its loaded buildings.
	static void RecountCost(AFGBlueprintHologram* hologram);
};
