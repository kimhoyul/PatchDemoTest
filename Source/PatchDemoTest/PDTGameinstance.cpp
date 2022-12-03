// Fill out your copyright notice in the Description page of Project Settings.


#include "PDTGameinstance.h"
#include "ChunkDownloader.h"

DEFINE_LOG_CATEGORY_STATIC(LogPatch, Display, Display);

void UPDTGameinstance::Init()
{
	Super::Init();
}

void UPDTGameinstance::Shutdown()
{
	Super::Shutdown();

	// 청크다운로더 끄기
	FChunkDownloader::Shutdown();
}

void UPDTGameinstance::InitPatching(const FString& VariantName)
{
	// 패치 플랫폼을 담아준다.
	PatchPlatform = VariantName;

	// HTTP모듈 인스턴스 생성
	FHttpModule& Http = FHttpModule::Get();

	// 새로운 HTTP 리퀘스트 생성하며 콜백함수 바인딩
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http.CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UPDTGameinstance::OnPatchVersionResponse);

	// 설정후 리퀘스트 보내기 (manifest 파일을 전달받아 파씽하기 위함)
	Request->SetURL(PatchVersionURL);
	Request->SetVerb("GET");
	Request->SetHeader(TEXT("User-Agent"), "X-UnrealEngine-Agent");
	Request->SetHeader("Content-Type", TEXT("application/json"));
	Request->ProcessRequest();
}

void UPDTGameinstance::OnPatchVersionResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	// 이 패처 구성에 사용할 배포 이름
	const FString DeploymentName = "Patcher-Live";

	// 콘텐츠 빌드 ID. HTTP 응답은 이 정보를 제공할 것입니다.
	// 서버에서 사용할 수 있게 되면 새 패치 버전으로 업데이트할 수 있습니다.
	FString ContentBuildId = Response->GetContentAsString();

	UE_LOG(LogPatch, Display, TEXT("Patch Content ID Response: %s"), *ContentBuildId);

	// ---------------------------------------------------------

	UE_LOG(LogPatch, Display, TEXT("Getting Chunk Downloader"));

	// 선택한 플랫폼으로 청크 다운로더 초기화
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetOrCreate();
	Downloader->Initialize(PatchPlatform, 8);

	UE_LOG(LogPatch, Display, TEXT("Loading Cached Build ID"));

	// 캐시된 빌드 매니페스트 데이터가 있는 경우 로드를 시도합니다.
	// 가장 최근에 다운로드한 manifest 데이터로 청크 다운로더 상태를 채웁니다.
	if (Downloader->LoadCachedBuild(DeploymentName))
	{
		UE_LOG(LogPatch, Display, TEXT("Cached Build Succeeded"));
	}
	else {
		UE_LOG(LogPatch, Display, TEXT("Cached Build Failed"));
	}

	UE_LOG(LogPatch, Display, TEXT("Updating Build Manifest"));

	// 데이터와 일치하는 최신 매니페스트 파일이 있는지 서버에 확인하고 다운로드합니다.
	// ChunkDownloader는 Lambda 함수를 사용하여 비동기 다운로드가 완료되면 알려줍니다.
	TFunction<void(bool)> ManifestCompleteCallback = [this](bool bSuccess)
	{
		// write to the log
		if (bSuccess)
		{
			UE_LOG(LogPatch, Display, TEXT("Manifest Update Succeeded"));
		}
		else {
			UE_LOG(LogPatch, Display, TEXT("Manifest Update Failed"));
		}

		// manifest 상태에 플래그를 지정하여 패치해도 안전한지 여부를 알 수 있습니다.
		bIsDownloadManifestUpToDate = bSuccess;

		// 델리게이트를 호출해 패치를 시작할 준비가 되었음을 게임에 알립니다.
		OnPatchReady.Broadcast(bSuccess);
	};

	// BuildID 및 Lambda 콜백을 전달하여 manifest 업데이트 프로세스를 시작합니다.
	Downloader->UpdateBuild(DeploymentName, ContentBuildId, ManifestCompleteCallback);
}

bool UPDTGameinstance::PatchGame()
{
	// 다운로드 manifest 최신인지 확인
	if (!bIsDownloadManifestUpToDate)
	{
		// manifest의 유효성을 먼저 검사하고유효하지 않으면 패치를 할 수 없습니다.
		UE_LOG(LogPatch, Display, TEXT("Manifest Update Failed. Can't patch the game"));

		return false;
	}
	UE_LOG(LogPatch, Display, TEXT("Starting the game patch process."));

	// 청크 다운로더 받기
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

	// 패칭 플래그 올리기
	bIsPatchingGame = true;

	// manifest에서 각 청크 파일의 현재 상태를 씁니다.
	// 이렇게 하면 특정 청크가 이미 다운로드되었는지, 진행 중인지, 보류 중인지 등을 확인할 수 있습니다.
	for (int32 ChunkID : ChunkDownloadList)
	{
		// 디버그 목적으로 청크 상태를 로그. 
		//원하는 경우 여기에서 더 복잡한 논리를 수행할 수 있습니다.
		int32 ChunkStatus = static_cast<int32>(Downloader->GetChunkStatus(ChunkID));
		UE_LOG(LogPatch, Display, TEXT("Chunk %i status: %i"), ChunkID, ChunkStatus);
	}

	// pak 파일 마운팅 콜백 람다 함수
	TFunction<void(bool)> MountCompleteCallback = [this](bool bSuccess)
	{
		// lower the chunk mount flag
		bIsPatchingGame = false;

		if (bSuccess)
		{
			UE_LOG(LogPatch, Display, TEXT("Chunk Mount complete"));

			// 델리게이트 호출
			OnPatchComplete.Broadcast(true);
		}
		else {
			UE_LOG(LogPatch, Display, TEXT("Chunk Mount failed"));

			// 델리게이트 호출
			OnPatchComplete.Broadcast(false);
		}
	};

	// 에셋에 액세스할 수 있도록 다운받은 Paks를 마운트합니다.
	Downloader->MountChunks(ChunkDownloadList, MountCompleteCallback);

	return true;
}

FPPatchStats UPDTGameinstance::GetPatchStatus()
{
	// 청크 다운로더 가져오기
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

	// 로딩 상태 가져오기
	FChunkDownloader::FStats LoadingStats = Downloader->GetLoadingStats();

	// 정보를 출력 output stats에 복사합니다.
	// ChunkDownloader는 BP를 지원하지 않는 uint64로 다운로드된 바이트를 추적합니다
	// 따라서 오버플로 및 해석 오류를 피하기 위해 MB로 나누고 int32(서명됨)로 캐스트합니다.
	FPPatchStats RetStats;

	RetStats.FilesDownloaded = LoadingStats.TotalFilesToDownload;
	RetStats.MBDownloaded = (int32)(LoadingStats.BytesDownloaded / (1024 * 1024));
	RetStats.TotalMBToDownload = (int32)(LoadingStats.TotalBytesToDownload / (1024 * 1024));
	RetStats.DownloadPercent = (float)LoadingStats.BytesDownloaded / (float)LoadingStats.TotalBytesToDownload;
	RetStats.LastError = LoadingStats.LastError;

	// 상태 구조체 반환
	return RetStats;
}

void UPDTGameinstance::OnDownloadComplete(bool bSuccess)
{
	// Pak 다운로드가 성공 여부
	if (!bSuccess)
	{
		UE_LOG(LogPatch, Display, TEXT("Patch Download Failed"));

		OnPatchComplete.Broadcast(false);

		return;
	}

	UE_LOG(LogPatch, Display, TEXT("Patch Download Complete"));

	// 청크 다운로더 가져오기
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

	// 다운로드한 청크 목록을 빌드합니다.
	FJsonSerializableArrayInt DownloadedChunks;

	for (int32 ChunkID : ChunkDownloadList)
	{
		DownloadedChunks.Add(ChunkID);
	}

	// pak 마운팅 콜백 람다 함수
	TFunction<void(bool)> MountCompleteCallback = [this](bool bSuccess)
	{
		// 청크 마운트 플래그 내리기
		bIsPatchingGame = false;

		if (bSuccess)
		{
			UE_LOG(LogPatch, Display, TEXT("Chunk Mount complete"));

			OnPatchComplete.Broadcast(true);
		}
		else {
			UE_LOG(LogPatch, Display, TEXT("Chunk Mount failed"));

			OnPatchComplete.Broadcast(false);
		}
	};

	// 에셋에 액세스할 수 있도록 Paks를 마운트합니다.
	Downloader->MountChunks(DownloadedChunks, MountCompleteCallback);
}

bool UPDTGameinstance::IsChunkLoaded(int32 ChunkID)
{
	// 매니페스트가 최신 상태인지 확인
	if (!bIsDownloadManifestUpToDate)
		return false;

	// 청크 다운로더 가져오기
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

	// 청크 상태에 대해 청크 다운로더를 쿼리합니다. 청크가 마운트된 경우에만 true를 반환합니다.
	return Downloader->GetChunkStatus(ChunkID) == FChunkDownloader::EChunkStatus::Mounted;
}

bool UPDTGameinstance::DownloadSingleChunk(int32 ChunkID)
{
	// 다운로드 매니페스트가 최신인지 확인하세요.
	if (!bIsDownloadManifestUpToDate)
	{
		// we couldn't contact the server to validate our manifest, so we can't patch
		UE_LOG(LogPatch, Display, TEXT("Manifest Update Failed. Can't patch the game"));

		return false;
	}

	// 아직 게임을 패치하는 중이라면 개별 다운로드 무시
	if (bIsPatchingGame)
	{
		UE_LOG(LogPatch, Display, TEXT("Main game patching underway. Ignoring single chunk downloads."));

		return false;
	}

	// 청크를 여러 번 다운로드하지 않도록 합니다.
	if (SingleChunkDownloadList.Contains(ChunkID))
	{
		UE_LOG(LogPatch, Display, TEXT("Chunk: %i already downloading"), ChunkID);

		return false;
	}

	// 단일 청크 다운로드 플래그 올리기
	bIsDownloadingSingleChunks = true;

	// 다운로드 목록에 청크를 추가
	SingleChunkDownloadList.Add(ChunkID);

	UE_LOG(LogPatch, Display, TEXT("Downloading specific Chunk: %i"), ChunkID);

	// 청크 다운로더
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

	// pak 마운팅 콜백 람다 함수
	TFunction<void(bool)> MountCompleteCallback = [this](bool bSuccess)
	{
		if (bSuccess)
		{
			UE_LOG(LogPatch, Display, TEXT("Single Chunk Mount complete"));

			OnSingleChunkPatchComplete.Broadcast(true);
		}
		else {
			UE_LOG(LogPatch, Display, TEXT("Single Mount failed"));

			OnSingleChunkPatchComplete.Broadcast(false);
		}
	};

	// 에셋에 액세스할 수 있도록 Paks를 마운트합니다.
	Downloader->MountChunk(ChunkID, MountCompleteCallback);

	return true;

}

void UPDTGameinstance::OnSingleChunkDownloadComplete(bool bSuccess)
{
	// 다운로드 청크 플래그를 내리기
	bIsDownloadingSingleChunks = false;

	// Pak 다운로드가 성공여부
	if (!bSuccess)
	{
		UE_LOG(LogPatch, Display, TEXT("Patch Download Failed"));

		OnSingleChunkPatchComplete.Broadcast(false);

		return;
	}

	UE_LOG(LogPatch, Display, TEXT("Patch Download Complete"));

	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

	// 다운로드한 청크 목록을 빌드합니다.
	FJsonSerializableArrayInt DownloadedChunks;

	for (int32 ChunkID : SingleChunkDownloadList)
	{
		DownloadedChunks.Add(ChunkID);
	}

	// pak 마운팅 콜백 람다 함수
	TFunction<void(bool)> MountCompleteCallback = [this](bool bSuccess)
	{
		if (bSuccess)
		{
			UE_LOG(LogPatch, Display, TEXT("Single Chunk Mount complete"));

			OnSingleChunkPatchComplete.Broadcast(true);
		}
		else {
			UE_LOG(LogPatch, Display, TEXT("Single Mount failed"));

			OnSingleChunkPatchComplete.Broadcast(false);
		}
	};

	// 자산에 액세스할 수 있도록 Paks를 마운트합니다.
	Downloader->MountChunks(DownloadedChunks, MountCompleteCallback);

}
