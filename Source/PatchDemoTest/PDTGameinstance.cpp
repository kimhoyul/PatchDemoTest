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

	// ûũ�ٿ�δ� ����
	FChunkDownloader::Shutdown();
}

void UPDTGameinstance::InitPatching(const FString& VariantName)
{
	// ��ġ �÷����� ����ش�.
	PatchPlatform = VariantName;

	// HTTP��� �ν��Ͻ� ����
	FHttpModule& Http = FHttpModule::Get();

	// ���ο� HTTP ������Ʈ �����ϸ� �ݹ��Լ� ���ε�
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http.CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UPDTGameinstance::OnPatchVersionResponse);

	// ������ ������Ʈ ������ (manifest ������ ���޹޾� �ľ��ϱ� ����)
	Request->SetURL(PatchVersionURL);
	Request->SetVerb("GET");
	Request->SetHeader(TEXT("User-Agent"), "X-UnrealEngine-Agent");
	Request->SetHeader("Content-Type", TEXT("application/json"));
	Request->ProcessRequest();
}

void UPDTGameinstance::OnPatchVersionResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	// �� ��ó ������ ����� ���� �̸�
	const FString DeploymentName = "Patcher-Live";

	// ������ ���� ID. HTTP ������ �� ������ ������ ���Դϴ�.
	// �������� ����� �� �ְ� �Ǹ� �� ��ġ �������� ������Ʈ�� �� �ֽ��ϴ�.
	FString ContentBuildId = Response->GetContentAsString();

	UE_LOG(LogPatch, Display, TEXT("Patch Content ID Response: %s"), *ContentBuildId);

	// ---------------------------------------------------------

	UE_LOG(LogPatch, Display, TEXT("Getting Chunk Downloader"));

	// ������ �÷������� ûũ �ٿ�δ� �ʱ�ȭ
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetOrCreate();
	Downloader->Initialize(PatchPlatform, 8);

	UE_LOG(LogPatch, Display, TEXT("Loading Cached Build ID"));

	// ĳ�õ� ���� �Ŵ��佺Ʈ �����Ͱ� �ִ� ��� �ε带 �õ��մϴ�.
	// ���� �ֱٿ� �ٿ�ε��� manifest �����ͷ� ûũ �ٿ�δ� ���¸� ä��ϴ�.
	if (Downloader->LoadCachedBuild(DeploymentName))
	{
		UE_LOG(LogPatch, Display, TEXT("Cached Build Succeeded"));
	}
	else {
		UE_LOG(LogPatch, Display, TEXT("Cached Build Failed"));
	}

	UE_LOG(LogPatch, Display, TEXT("Updating Build Manifest"));

	// �����Ϳ� ��ġ�ϴ� �ֽ� �Ŵ��佺Ʈ ������ �ִ��� ������ Ȯ���ϰ� �ٿ�ε��մϴ�.
	// ChunkDownloader�� Lambda �Լ��� ����Ͽ� �񵿱� �ٿ�ε尡 �Ϸ�Ǹ� �˷��ݴϴ�.
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

		// manifest ���¿� �÷��׸� �����Ͽ� ��ġ�ص� �������� ���θ� �� �� �ֽ��ϴ�.
		bIsDownloadManifestUpToDate = bSuccess;

		// ��������Ʈ�� ȣ���� ��ġ�� ������ �غ� �Ǿ����� ���ӿ� �˸��ϴ�.
		OnPatchReady.Broadcast(bSuccess);
	};

	// BuildID �� Lambda �ݹ��� �����Ͽ� manifest ������Ʈ ���μ����� �����մϴ�.
	Downloader->UpdateBuild(DeploymentName, ContentBuildId, ManifestCompleteCallback);
}

bool UPDTGameinstance::PatchGame()
{
	// �ٿ�ε� manifest �ֽ����� Ȯ��
	if (!bIsDownloadManifestUpToDate)
	{
		// manifest�� ��ȿ���� ���� �˻��ϰ���ȿ���� ������ ��ġ�� �� �� �����ϴ�.
		UE_LOG(LogPatch, Display, TEXT("Manifest Update Failed. Can't patch the game"));

		return false;
	}
	UE_LOG(LogPatch, Display, TEXT("Starting the game patch process."));

	// ûũ �ٿ�δ� �ޱ�
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

	// ��Ī �÷��� �ø���
	bIsPatchingGame = true;

	// manifest���� �� ûũ ������ ���� ���¸� ���ϴ�.
	// �̷��� �ϸ� Ư�� ûũ�� �̹� �ٿ�ε�Ǿ�����, ���� ������, ���� ������ ���� Ȯ���� �� �ֽ��ϴ�.
	for (int32 ChunkID : ChunkDownloadList)
	{
		// ����� �������� ûũ ���¸� �α�. 
		//���ϴ� ��� ���⿡�� �� ������ ���� ������ �� �ֽ��ϴ�.
		int32 ChunkStatus = static_cast<int32>(Downloader->GetChunkStatus(ChunkID));
		UE_LOG(LogPatch, Display, TEXT("Chunk %i status: %i"), ChunkID, ChunkStatus);
	}

	// pak ���� ������ �ݹ� ���� �Լ�
	TFunction<void(bool)> MountCompleteCallback = [this](bool bSuccess)
	{
		// lower the chunk mount flag
		bIsPatchingGame = false;

		if (bSuccess)
		{
			UE_LOG(LogPatch, Display, TEXT("Chunk Mount complete"));

			// ��������Ʈ ȣ��
			OnPatchComplete.Broadcast(true);
		}
		else {
			UE_LOG(LogPatch, Display, TEXT("Chunk Mount failed"));

			// ��������Ʈ ȣ��
			OnPatchComplete.Broadcast(false);
		}
	};

	// ���¿� �׼����� �� �ֵ��� �ٿ���� Paks�� ����Ʈ�մϴ�.
	Downloader->MountChunks(ChunkDownloadList, MountCompleteCallback);

	return true;
}

FPPatchStats UPDTGameinstance::GetPatchStatus()
{
	// ûũ �ٿ�δ� ��������
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

	// �ε� ���� ��������
	FChunkDownloader::FStats LoadingStats = Downloader->GetLoadingStats();

	// ������ ��� output stats�� �����մϴ�.
	// ChunkDownloader�� BP�� �������� �ʴ� uint64�� �ٿ�ε�� ����Ʈ�� �����մϴ�
	// ���� �����÷� �� �ؼ� ������ ���ϱ� ���� MB�� ������ int32(�����)�� ĳ��Ʈ�մϴ�.
	FPPatchStats RetStats;

	RetStats.FilesDownloaded = LoadingStats.TotalFilesToDownload;
	RetStats.MBDownloaded = (int32)(LoadingStats.BytesDownloaded / (1024 * 1024));
	RetStats.TotalMBToDownload = (int32)(LoadingStats.TotalBytesToDownload / (1024 * 1024));
	RetStats.DownloadPercent = (float)LoadingStats.BytesDownloaded / (float)LoadingStats.TotalBytesToDownload;
	RetStats.LastError = LoadingStats.LastError;

	// ���� ����ü ��ȯ
	return RetStats;
}

void UPDTGameinstance::OnDownloadComplete(bool bSuccess)
{
	// Pak �ٿ�ε尡 ���� ����
	if (!bSuccess)
	{
		UE_LOG(LogPatch, Display, TEXT("Patch Download Failed"));

		OnPatchComplete.Broadcast(false);

		return;
	}

	UE_LOG(LogPatch, Display, TEXT("Patch Download Complete"));

	// ûũ �ٿ�δ� ��������
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

	// �ٿ�ε��� ûũ ����� �����մϴ�.
	FJsonSerializableArrayInt DownloadedChunks;

	for (int32 ChunkID : ChunkDownloadList)
	{
		DownloadedChunks.Add(ChunkID);
	}

	// pak ������ �ݹ� ���� �Լ�
	TFunction<void(bool)> MountCompleteCallback = [this](bool bSuccess)
	{
		// ûũ ����Ʈ �÷��� ������
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

	// ���¿� �׼����� �� �ֵ��� Paks�� ����Ʈ�մϴ�.
	Downloader->MountChunks(DownloadedChunks, MountCompleteCallback);
}

bool UPDTGameinstance::IsChunkLoaded(int32 ChunkID)
{
	// �Ŵ��佺Ʈ�� �ֽ� �������� Ȯ��
	if (!bIsDownloadManifestUpToDate)
		return false;

	// ûũ �ٿ�δ� ��������
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

	// ûũ ���¿� ���� ûũ �ٿ�δ��� �����մϴ�. ûũ�� ����Ʈ�� ��쿡�� true�� ��ȯ�մϴ�.
	return Downloader->GetChunkStatus(ChunkID) == FChunkDownloader::EChunkStatus::Mounted;
}

bool UPDTGameinstance::DownloadSingleChunk(int32 ChunkID)
{
	// �ٿ�ε� �Ŵ��佺Ʈ�� �ֽ����� Ȯ���ϼ���.
	if (!bIsDownloadManifestUpToDate)
	{
		// we couldn't contact the server to validate our manifest, so we can't patch
		UE_LOG(LogPatch, Display, TEXT("Manifest Update Failed. Can't patch the game"));

		return false;
	}

	// ���� ������ ��ġ�ϴ� ���̶�� ���� �ٿ�ε� ����
	if (bIsPatchingGame)
	{
		UE_LOG(LogPatch, Display, TEXT("Main game patching underway. Ignoring single chunk downloads."));

		return false;
	}

	// ûũ�� ���� �� �ٿ�ε����� �ʵ��� �մϴ�.
	if (SingleChunkDownloadList.Contains(ChunkID))
	{
		UE_LOG(LogPatch, Display, TEXT("Chunk: %i already downloading"), ChunkID);

		return false;
	}

	// ���� ûũ �ٿ�ε� �÷��� �ø���
	bIsDownloadingSingleChunks = true;

	// �ٿ�ε� ��Ͽ� ûũ�� �߰�
	SingleChunkDownloadList.Add(ChunkID);

	UE_LOG(LogPatch, Display, TEXT("Downloading specific Chunk: %i"), ChunkID);

	// ûũ �ٿ�δ�
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

	// pak ������ �ݹ� ���� �Լ�
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

	// ���¿� �׼����� �� �ֵ��� Paks�� ����Ʈ�մϴ�.
	Downloader->MountChunk(ChunkID, MountCompleteCallback);

	return true;

}

void UPDTGameinstance::OnSingleChunkDownloadComplete(bool bSuccess)
{
	// �ٿ�ε� ûũ �÷��׸� ������
	bIsDownloadingSingleChunks = false;

	// Pak �ٿ�ε尡 ��������
	if (!bSuccess)
	{
		UE_LOG(LogPatch, Display, TEXT("Patch Download Failed"));

		OnSingleChunkPatchComplete.Broadcast(false);

		return;
	}

	UE_LOG(LogPatch, Display, TEXT("Patch Download Complete"));

	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

	// �ٿ�ε��� ûũ ����� �����մϴ�.
	FJsonSerializableArrayInt DownloadedChunks;

	for (int32 ChunkID : SingleChunkDownloadList)
	{
		DownloadedChunks.Add(ChunkID);
	}

	// pak ������ �ݹ� ���� �Լ�
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

	// �ڻ꿡 �׼����� �� �ֵ��� Paks�� ����Ʈ�մϴ�.
	Downloader->MountChunks(DownloadedChunks, MountCompleteCallback);

}
