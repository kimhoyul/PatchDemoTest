// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Runtime/Online/HTTP/Public/Http.h"
#include "PDTGameinstance.generated.h"

// 패칭 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPatchCompleteDelegate, bool, Succeeded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FChunkMountedDelegate, int32, ChunkID, bool, Succeeded);

// 게임 패칭 진행 상태 피드백을 위해 사용
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
	/** 게임인스턴스 오버라이드 */
	virtual void Init() override;
	virtual void Shutdown() override;
		
	/** 전달된 배포 이름으로 패치 시스템을 초기화 진행 */
	UFUNCTION(BlueprintCallable, Category = "Patching")
	void InitPatching(const FString& VariantName);

	/*
	* InitPatching 에서 등록한 콜백 함수  
	* HTTP 리퀘스트에 대한 응답 수신
	*/
	void OnPatchVersionResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	/*
	* 게임 패치 프로세스를 시작합니다. 
	* 패치 매니페스트가 최신이 아닌 경우 false를 반환합니다. 
	*/
	UFUNCTION(BlueprintCallable, Category = "Patching")
	bool PatchGame();

	/** 진행률 표시줄 등을 채우는 데 사용할 수 있는 패치 상태 구조체를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "Patching")
	FPPatchStats GetPatchStatus();

	/** 패치 매니페스트가 쿼리되고 게임을 패치할지 여부를 결정할 준비가 되면 시작됩니다.*/
	UPROPERTY(BlueprintAssignable, Category = "Patching");
	FPatchCompleteDelegate OnPatchReady;

	/** 패치 프로세스가 성공하거나 실패할 때 발생 */
	UPROPERTY(BlueprintAssignable, Category = "Patching");
	FPatchCompleteDelegate OnPatchComplete;

protected:

	FString PatchPlatform;

	/*
	* 패치 버전을 쿼리할 URL
	* 패치 서버에 올라가 있는 패치 버전ID 파일에서 가져온다.
	*/
	UPROPERTY(EditDefaultsOnly, Category = "Patching")
	FString PatchVersionURL;

	/*
	* 다운로드를 시도할 청크ID 목록 
	* 이 프로젝트에선 BlueprintOffice Level 과 UI를 필수적으로 가져온다.
	*/
	UPROPERTY(EditDefaultsOnly, Category = "Patching")
	TArray<int32> ChunkDownloadList;

	/** 패치 manifest에서 업데이트가 확인되면 true로 설정합니다. */
	bool bIsDownloadManifestUpToDate = false;

	/** 기본 패치 프로세스가 진행 중인 경우 true로 설정 */
	bool bIsPatchingGame = false;

	/** 청크 다운로드 프로세스가 완료되면 호출됩니다. */
	void OnDownloadComplete(bool bSuccess);

public:

	/** 주어진 청크가 다운로드되고 마운트되면 true를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "Patching")
	bool IsChunkLoaded(int32 ChunkID);

	/** 패칭 프로세스가 성공하거나 실패할 때 발생 */
	UPROPERTY(BlueprintAssignable, Category = "Patching");
	FPatchCompleteDelegate OnSingleChunkPatchComplete;

	/** 기본 패치 프로세스와 별도로 다운로드할 단일 청크 목록 */
	TArray<int32> SingleChunkDownloadList;

	/** 참이면 시스템이 현재 개별 청크를 다운로드하고 있습니다. */
	bool bIsDownloadingSingleChunks = false;

	/** 시스템이 현재 단일 청크를 다운로드하고 있는지 여부를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "Patching")
		bool IsDownloadingSingleChunks()
	{
		return bIsDownloadingSingleChunks;
	}

	/** 다운로드 목록에 단일 청크를 추가하고 로드 및 마운트 프로세스를 시작합니다. */
	UFUNCTION(BlueprintCallable, Category = "Patching")
	bool DownloadSingleChunk(int32 ChunkID);

	/** 단일 청크 다운로드 프로세스가 완료되면 호출됨 */
	void OnSingleChunkDownloadComplete(bool bSuccess);
};
