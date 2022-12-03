// Copyright Epic Games, Inc. All Rights Reserved.

#include "PatchDemoTestGameMode.h"
#include "PatchDemoTestCharacter.h"
#include "UObject/ConstructorHelpers.h"

APatchDemoTestGameMode::APatchDemoTestGameMode()
{
	bUseSeamlessTravel = true;
}

void APatchDemoTestGameMode::TryServerTravel(const FString& URL)
{
	GetWorld()->ServerTravel(URL, true, true);
}
