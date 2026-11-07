#include "BlueprintTrueCost.h"

#include "Buildables/FGBuildable.h"
#include "FGBlueprintSubsystem.h"
#include "FGFactoryBlueprintTypes.h"
#include "Hologram/FGBlueprintHologram.h"
#include "ItemAmount.h"
#include "Patching/NativeHookManager.h"
#include "Resources/FGItemDescriptor.h"

DEFINE_LOG_CATEGORY(LogBlueprintTrueCost)

// How vanilla 1.2 prices a blueprint, read from the shipping game's disassembly (CL 517554):
//
//  - The cost is saved in the .sbp header. UFGBlueprintDescriptor::mCost is filled from it, and
//    AFGBlueprintHologram::GetBaseCost and AFGBuildableBlueprintDesigner::RemoveCostToLoad both
//    read mCost. Neither looks at recipes, so changing a recipe never reprices a blueprint.
//  - AFGBlueprintSubsystem::CalculateBlueprintCost is what produces the number on save. It sums
//    each building's dismantle refund, which reads current recipe ingredients and customization.
//  - LoadBlueprintToOtherWorld spawns the blueprint's real buildings into the blueprint world
//    and leaves them in mBlueprintWorldBuildables, so the same calculation can run on them.
//  - The refund's world lookups (parts cost multiplier, recipe manager) null-check, so they are
//    safe in the blueprint world, which has no game state. The parts cost multiplier from
//    advanced game settings may therefore be left out of the recount.
//
// Fields are read by reflection on purpose. Game 1.2.5 moved AFGBlueprintSubsystem members 8
// bytes from where the SML 3.12.0 headers put them (mBlueprintWorldBuildables is at 0x378 in the
// game), so the inline GetBlueprintWorldBuildables could read the wrong memory. Reflection uses
// the live class layout. CalculateBlueprintCost is exported, so calling it runs the game's code.

namespace
{
	FString DescribeCost(const TArray<FItemAmount>& cost)
	{
		TArray<FString> parts;
		for (const FItemAmount& item : cost)
			parts.Add(FString::Printf(TEXT("%s x%d"), *GetNameSafe(item.ItemClass.Get()), item.Amount));
		parts.Sort();
		return parts.Num() > 0 ? FString::Join(parts, TEXT(", ")) : FString(TEXT("free"));
	}
}

void FBlueprintTrueCostModule::StartupModule()
{
	// In editor builds FactoryGame's functions are link stubs too short to patch, and a failed
	// hook is fatal - it kills the editor and Alpakit's cook step.
#if !WITH_EDITOR
	SUBSCRIBE_METHOD_AFTER(AFGBlueprintHologram::LoadBlueprintToOtherWorld, [](AFGBlueprintHologram* hologram)
	{
		RecountCost(hologram);
	});
	UE_LOG(LogBlueprintTrueCost, Log, TEXT("Hooks registered."));
#endif
}

void FBlueprintTrueCostModule::ShutdownModule()
{
}

void FBlueprintTrueCostModule::RecountCost(AFGBlueprintHologram* hologram)
{
	static const FObjectPropertyBase* descriptorProperty = FindFProperty<FObjectPropertyBase>(AFGBlueprintHologram::StaticClass(), TEXT("mBlueprintDescriptor"));
	static const FStrProperty* nameProperty = FindFProperty<FStrProperty>(AFGBlueprintHologram::StaticClass(), TEXT("mBlueprintDescName"));
	static const FArrayProperty* buildablesProperty = FindFProperty<FArrayProperty>(AFGBlueprintSubsystem::StaticClass(), TEXT("mBlueprintWorldBuildables"));
	static const FArrayProperty* costProperty = FindFProperty<FArrayProperty>(UFGBlueprintDescriptor::StaticClass(), TEXT("mCost"));

	if (!descriptorProperty || !nameProperty || !buildablesProperty || !costProperty)
	{
		UE_LOG(LogBlueprintTrueCost, Warning, TEXT("Skipped: a game property is missing (descriptor %d, name %d, buildables %d, cost %d)."),
			descriptorProperty != nullptr, nameProperty != nullptr, buildablesProperty != nullptr, costProperty != nullptr);
		return;
	}

	const FString& blueprintName = *nameProperty->ContainerPtrToValuePtr<FString>(hologram);

	UFGBlueprintDescriptor* descriptor = Cast<UFGBlueprintDescriptor>(descriptorProperty->GetObjectPropertyValue_InContainer(hologram));
	AFGBlueprintSubsystem* subsystem = AFGBlueprintSubsystem::Get(hologram->GetWorld());
	if (!IsValid(descriptor) || !subsystem)
	{
		UE_LOG(LogBlueprintTrueCost, Warning, TEXT("'%s' skipped: descriptor valid %d, subsystem valid %d."), *blueprintName, IsValid(descriptor), subsystem != nullptr);
		return;
	}

	const FObjectPropertyBase* buildableProperty = CastField<FObjectPropertyBase>(buildablesProperty->Inner);
	if (!buildableProperty)
	{
		UE_LOG(LogBlueprintTrueCost, Warning, TEXT("'%s' skipped: mBlueprintWorldBuildables does not hold objects."), *blueprintName);
		return;
	}

	FScriptArrayHelper buildablesHelper(buildablesProperty, buildablesProperty->ContainerPtrToValuePtr<void>(subsystem));
	TArray<AFGBuildable*> buildables;
	buildables.Reserve(buildablesHelper.Num());
	for (int32 i = 0; i < buildablesHelper.Num(); i++)
	{
		if (AFGBuildable* buildable = Cast<AFGBuildable>(buildableProperty->GetObjectPropertyValue(buildablesHelper.GetRawPtr(i))))
			buildables.Add(buildable);
	}

	// Nothing loaded means nothing was counted, not that the blueprint is free.
	if (buildables.Num() == 0)
	{
		UE_LOG(LogBlueprintTrueCost, Warning, TEXT("'%s' skipped: no buildings loaded, keeping the stored cost."), *blueprintName);
		return;
	}

	TArray<FItemAmount> recounted;
	subsystem->CalculateBlueprintCost(buildables, recounted);

	TArray<FItemAmount>& storedCost = *costProperty->ContainerPtrToValuePtr<TArray<FItemAmount>>(descriptor);
	const FString storedText = DescribeCost(storedCost);
	const FString recountedText = DescribeCost(recounted);

	if (storedText == recountedText)
	{
		UE_LOG(LogBlueprintTrueCost, Log, TEXT("'%s' unchanged (%d buildings): %s"), *blueprintName, buildables.Num(), *storedText);
		return;
	}

	UE_LOG(LogBlueprintTrueCost, Log, TEXT("'%s' repriced (%d buildings): %s -> %s"), *blueprintName, buildables.Num(), *storedText, *recountedText);
	storedCost = MoveTemp(recounted);
}

IMPLEMENT_MODULE(FBlueprintTrueCostModule, BlueprintTrueCost)
