// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PatchDemoTestGameMode.generated.h"

UCLASS(minimalapi)
class APatchDemoTestGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APatchDemoTestGameMode();

public:
	UFUNCTION(BlueprintCallable, Category = "DLC")
	void TryServerTravel(const FString& URL);
};



