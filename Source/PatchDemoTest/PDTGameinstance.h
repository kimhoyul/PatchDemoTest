// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Runtime/Online/HTTP/Public/Http.h"
#include "PDTGameinstance.generated.h"

// ��Ī ��������Ʈ
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPatchCompleteDelegate, bool, Succeeded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FChunkMountedDelegate, int32, ChunkID, bool, Succeeded);

// ���� ��Ī ���� ���� �ǵ���� ���� ���
USTRUCT(BlueprintType)
struct FPPatchStats
{
	GENERATED_BODY()

		UPROPERTY(BlueprintReadOnly)
		int32 FilesDownloaded;

	UPROPERTY(BlueprintReadOnly)
		int32 TotalFilesToDownload;

	UPROPERTY(BlueprintReadOnly)
		float DownloadPercent;

	UPROPERTY(BlueprintReadOnly)
		int32 MBDownloaded;

	UPROPERTY(BlueprintReadOnly)
		int32 TotalMBToDownload;

	UPROPERTY(BlueprintReadOnly)
		FText LastError;
};

UCLASS()
class PATCHDEMOTEST_API UPDTGameinstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
	/** �����ν��Ͻ� �������̵� */
	virtual void Init() override;
	virtual void Shutdown() override;
		
	/** ���޵� ���� �̸����� ��ġ �ý����� �ʱ�ȭ ���� */
	UFUNCTION(BlueprintCallable, Category = "Patching")
	void InitPatching(const FString& VariantName);

	/*
	* InitPatching ���� ����� �ݹ� �Լ�  
	* HTTP ������Ʈ�� ���� ���� ����
	*/
	void OnPatchVersionResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	/*
	* ���� ��ġ ���μ����� �����մϴ�. 
	* ��ġ �Ŵ��佺Ʈ�� �ֽ��� �ƴ� ��� false�� ��ȯ�մϴ�. 
	*/
	UFUNCTION(BlueprintCallable, Category = "Patching")
	bool PatchGame();

	/** ����� ǥ���� ���� ä��� �� ����� �� �ִ� ��ġ ���� ����ü�� ��ȯ�մϴ�. */
	UFUNCTION(BlueprintPure, Category = "Patching")
	FPPatchStats GetPatchStatus();

	/** ��ġ �Ŵ��佺Ʈ�� �����ǰ� ������ ��ġ���� ���θ� ������ �غ� �Ǹ� ���۵˴ϴ�.*/
	UPROPERTY(BlueprintAssignable, Category = "Patching");
	FPatchCompleteDelegate OnPatchReady;

	/** ��ġ ���μ����� �����ϰų� ������ �� �߻� */
	UPROPERTY(BlueprintAssignable, Category = "Patching");
	FPatchCompleteDelegate OnPatchComplete;

protected:

	FString PatchPlatform;

	/*
	* ��ġ ������ ������ URL
	* ��ġ ������ �ö� �ִ� ��ġ ����ID ���Ͽ��� �����´�.
	*/
	UPROPERTY(EditDefaultsOnly, Category = "Patching")
	FString PatchVersionURL;

	/*
	* �ٿ�ε带 �õ��� ûũID ��� 
	* �� ������Ʈ���� BlueprintOffice Level �� UI�� �ʼ������� �����´�.
	*/
	UPROPERTY(EditDefaultsOnly, Category = "Patching")
	TArray<int32> ChunkDownloadList;

	/** ��ġ manifest���� ������Ʈ�� Ȯ�εǸ� true�� �����մϴ�. */
	bool bIsDownloadManifestUpToDate = false;

	/** �⺻ ��ġ ���μ����� ���� ���� ��� true�� ���� */
	bool bIsPatchingGame = false;

	/** ûũ �ٿ�ε� ���μ����� �Ϸ�Ǹ� ȣ��˴ϴ�. */
	void OnDownloadComplete(bool bSuccess);

public:

	/** �־��� ûũ�� �ٿ�ε�ǰ� ����Ʈ�Ǹ� true�� ��ȯ�մϴ�. */
	UFUNCTION(BlueprintPure, Category = "Patching")
	bool IsChunkLoaded(int32 ChunkID);

	/** ��Ī ���μ����� �����ϰų� ������ �� �߻� */
	UPROPERTY(BlueprintAssignable, Category = "Patching");
	FPatchCompleteDelegate OnSingleChunkPatchComplete;

	/** �⺻ ��ġ ���μ����� ������ �ٿ�ε��� ���� ûũ ��� */
	TArray<int32> SingleChunkDownloadList;

	/** ���̸� �ý����� ���� ���� ûũ�� �ٿ�ε��ϰ� �ֽ��ϴ�. */
	bool bIsDownloadingSingleChunks = false;

	/** �ý����� ���� ���� ûũ�� �ٿ�ε��ϰ� �ִ��� ���θ� ��ȯ�մϴ�. */
	UFUNCTION(BlueprintPure, Category = "Patching")
		bool IsDownloadingSingleChunks()
	{
		return bIsDownloadingSingleChunks;
	}

	/** �ٿ�ε� ��Ͽ� ���� ûũ�� �߰��ϰ� �ε� �� ����Ʈ ���μ����� �����մϴ�. */
	UFUNCTION(BlueprintCallable, Category = "Patching")
	bool DownloadSingleChunk(int32 ChunkID);

	/** ���� ûũ �ٿ�ε� ���μ����� �Ϸ�Ǹ� ȣ��� */
	void OnSingleChunkDownloadComplete(bool bSuccess);
};
