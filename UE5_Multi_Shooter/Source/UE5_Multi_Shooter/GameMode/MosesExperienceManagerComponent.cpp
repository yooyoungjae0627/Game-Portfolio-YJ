#include "MosesExperienceManagerComponent.h"

#include "MosesExperienceDefinition.h"
#include "UE5_Multi_Shooter/System/MosesAssetManager.h"
#include "UE5_Multi_Shooter/MosesLogChannels.h"

#include "Net/UnrealNetwork.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Engine.h" // GEngine

// Plugin path 만들기
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

// ------------------------------
// OnScreen Helper
// ------------------------------
static void ScreenMsg(const FColor& Color, const FString& Msg, float Time = 8.f)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, Time, Color, Msg);
	}
}

UMosesExperienceManagerComponent::UMosesExperienceManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);

	// (가능하면 .h에서 기본값 초기화도 추천)
	// 여기서 최소한의 안전 초기화
	LoadState = EMosesExperienceLoadState::Unloaded;
	bNotifiedReadyOnce = false;
	PendingGFCount = 0;
	CompletedGFCount = 0;
	bAnyGFFailed = false;
	CurrentExperience = nullptr;
	PendingExperienceId = FPrimaryAssetId();
	CurrentExperienceId = FPrimaryAssetId();
}

void UMosesExperienceManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMosesExperienceManagerComponent, CurrentExperienceId);
}

// ------------------------------
// SERVER: set experience id (authoritative)
// 서버에서 Experience를 확정하고,
// 서버/클라이언트 모두에게 Experience 초기화(OnRep)를 트리거한다.
// (중복 적용 방지 포함)
// ------------------------------
void UMosesExperienceManagerComponent::ServerSetCurrentExperience_Implementation(FPrimaryAssetId ExperienceId)
{
	// 안전장치: 소유자(보통 GameState)가 있어야 함
	check(GetOwner());

	// 안전장치: 이 함수는 "서버에서만" 실행되어야 함 (권한 강제)
	check(GetOwner()->HasAuthority());

	// 우리가 기대하는 PrimaryAssetType 이름은 "Experience"
	static const FPrimaryAssetType ExperienceType(TEXT("Experience"));

	// 디버그: 서버가 어떤 ExperienceId를 받았고 현재 로딩 상태가 뭔지 찍기
	ScreenMsg(FColor::Orange,
		FString::Printf(TEXT("[EXP-MGR][ServerSet] In=%s State=%d"), *ExperienceId.ToString(), (int32)LoadState));

	// 방어: 들어온 ExperienceId가 비정상이면 바로 실패 처리
	if (!ExperienceId.IsValid())
	{
		FailExperienceLoad(TEXT("ServerSetCurrentExperience: Invalid ExperienceId"));
		return;
	}

	// 방어: 혹시 다른 타입(예: Map, Item 등)으로 들어오면
	// "Experience" 타입으로 강제로 맞춰준다 (이름은 그대로, 타입만 교정)
	if (ExperienceId.PrimaryAssetType != ExperienceType)
	{
		ScreenMsg(FColor::Orange,
			FString::Printf(TEXT("[EXP-MGR][ServerSet] FixType -> Experience:%s"), *ExperienceId.PrimaryAssetName.ToString()));

		// 타입을 Experience로 고정해서 다시 만든다
		ExperienceId = FPrimaryAssetId(ExperienceType, ExperienceId.PrimaryAssetName);
	}

	// 중복 방지:
	// 이미 같은 Experience를 로딩 중이거나(또는 로딩 완료) 상태면
	// 또 다시 시작하지 않고 무시한다
	if (CurrentExperienceId == ExperienceId && LoadState != EMosesExperienceLoadState::Unloaded)
	{
		ScreenMsg(FColor::Silver,
			FString::Printf(TEXT("[EXP-MGR][ServerSet] Ignore duplicate. Id=%s State=%d"),
				*ExperienceId.ToString(), (int32)LoadState));
		return;
	}

	/*******************  플레이 하면 여기 밑을 바로 탄다. ********************/ 
	// 이전 로딩 상태 리셋
	// (주의: 콜백 리스트는 지우면 안 되므로 ResetExperienceLoadState 구현에서 그 부분을 보존해야 함)
	ResetExperienceLoadState();

	// 서버가 "이번 매치 Experience는 이거다"라고 확정 저장
	CurrentExperienceId = ExperienceId;

	// 🔍 로그: 서버 확정 결과 출력
	UE_LOG(LogMosesExp, Log, TEXT("[SERVER] Chose ExperienceId=%s"), *CurrentExperienceId.ToString());
	ScreenMsg(FColor::Orange, FString::Printf(TEXT("[EXP-MGR][ServerSet] Chosen=%s"), *CurrentExperienceId.ToString()));

	// ⭐ 핵심:
	// 원래 OnRep_* 는 "클라에서 값 복제되면 자동 호출"되는 함수인데,
	// 서버도 클라와 "같은 코드 경로"로 로딩을 돌리고 싶어서
	// 서버에서 직접 OnRep를 호출한다.
	//
	// 즉,
	// - 서버: 값을 바꾸고 -> OnRep를 직접 호출해서 로딩 시작
	// - 클라: 값이 복제되면 -> OnRep가 자동 호출되어 로딩 시작
	OnRep_CurrentExperienceId();
}

// ------------------------------
// Experience가 이미 로딩 완료면: 지금 바로 Delegate 실행
// 아직 로딩 중이면: 나중에 실행하기 위해 콜백 리스트에 등록
// 로딩 완료 시: 등록된 모든 Delegate가 한 번씩 호출됨
// ------------------------------
void UMosesExperienceManagerComponent::CallOrRegister_OnExperienceLoaded(FMosesExperienceLoadedDelegate&& Delegate)
{
	// 이미 로딩 완료면 즉시 실행
	if (IsExperienceLoaded())
	{
		ScreenMsg(FColor::Green, TEXT("[EXP-MGR][Register] Already READY -> ExecuteImmediately"));
		Delegate.ExecuteIfBound(CurrentExperience);
		return;
	}

	// 로딩 전/중이면 콜백 등록
	OnExperienceLoadedCallbacks.Add(MoveTemp(Delegate));
	ScreenMsg(FColor::Cyan,
		FString::Printf(TEXT("[EXP-MGR][Register] Added callback. Count=%d"), OnExperienceLoadedCallbacks.Num()));
}

const UMosesExperienceDefinition* UMosesExperienceManagerComponent::GetCurrentExperienceChecked() const
{
	check(IsExperienceLoaded());
	check(CurrentExperience);
	return CurrentExperience;
}

// ------------------------------
// RepNotify: 서버가 CurrentExperienceId를 바꾸면 클라에서 호출됨
// 목적: "서버가 지정한 Experience"를 클라 쪽에서 로딩 시작 트리거로 사용
// ------------------------------
void UMosesExperienceManagerComponent::OnRep_CurrentExperienceId()
{
	// [Debug] 현재 받은 ExperienceId + 로딩 상태 출력
	ScreenMsg(FColor::Yellow,
		FString::Printf(TEXT("[EXP-MGR][OnRep] CurrentId=%s State=%d"),
			*CurrentExperienceId.ToString(), (int32)LoadState));

	// [Guard 1] 서버가 보낸 Id가 비정상이면 즉시 실패 처리
	if (CurrentExperienceId.IsValid() == false)
	{
		FailExperienceLoad(TEXT("OnRep_CurrentExperienceId: Invalid CurrentExperienceId"));
		return;
	}

	// [Guard 2] RepNotify는 여러 번 올 수 있음(재전송/재복제/상태변화 등)
	//          -> 이미 로딩을 시작했거나 끝난 상태면 중복 로딩 방지
	if (LoadState != EMosesExperienceLoadState::Unloaded)
	{
		ScreenMsg(FColor::Silver,
			FString::Printf(TEXT("[EXP-MGR][OnRep] Ignore (State != Unloaded). State=%d"), (int32)LoadState));
		return;
	}

	// [State] 이제부터 "이 Experience를 로딩 중" 상태로 전이
	PendingExperienceId = CurrentExperienceId;
	LoadState = EMosesExperienceLoadState::Loading;

	// [Debug] 로딩 시작 로그
	UE_LOG(LogMosesExp, Log, TEXT("[EXP] Begin Load=%s"), *CurrentExperienceId.ToString());
	ScreenMsg(FColor::Yellow, FString::Printf(TEXT("[EXP-MGR][Load] Begin %s"), *CurrentExperienceId.ToString()));

	// [Load] AssetManager를 통해 ExperienceDefinition(Primary Asset) 로딩 시작
	//        - 여기서는 bundle 없이 "정의(Definition)만" 먼저 당겨오고,
	//        - 로딩 완료 콜백(OnExperienceAssetsLoaded)에서 다음 단계 진행
	UMosesAssetManager& AssetManager = UMosesAssetManager::Get();

	TArray<FPrimaryAssetId> AssetIds;
	AssetIds.Add(CurrentExperienceId);

	AssetManager.LoadPrimaryAssets(
		AssetIds,
		{}, // bundles 미사용(필요하면 "Client" / "Server" 등 번들로 확장 가능)
		FStreamableDelegate::CreateUObject(this, &ThisClass::OnExperienceAssetsLoaded)
	);
}

// ------------------------------
// ExperienceDefinition loaded
// ------------------------------
void UMosesExperienceManagerComponent::OnExperienceAssetsLoaded()
{
	// stale callback 방지
	if (PendingExperienceId != CurrentExperienceId)
	{
		UE_LOG(LogMosesExp, Warning, TEXT("[EXP] AssetsLoaded stale. Pending=%s Current=%s"),
			*PendingExperienceId.ToString(), *CurrentExperienceId.ToString());

		ScreenMsg(FColor::Silver,
			FString::Printf(TEXT("[EXP-MGR][AssetsLoaded] STALE Pending=%s Current=%s"),
				*PendingExperienceId.ToString(), *CurrentExperienceId.ToString()));
		return;
	}

	UMosesAssetManager& AssetManager = UMosesAssetManager::Get();

	const UMosesExperienceDefinition* ExperienceDef =
		AssetManager.GetPrimaryAssetObject<UMosesExperienceDefinition>(CurrentExperienceId);

	if (!ExperienceDef)
	{
		FailExperienceLoad(TEXT("OnExperienceAssetsLoaded: ExperienceDef null"));
		return;
	}

	CurrentExperience = ExperienceDef;

	UE_LOG(LogMosesExp, Log, TEXT("[EXP] Loaded ExperienceDefinition=%s"), *CurrentExperienceId.ToString());
	ScreenMsg(FColor::Yellow, FString::Printf(TEXT("[EXP-MGR][AssetsLoaded] OK %s"), *CurrentExperienceId.ToString()));

	StartLoadGameFeatures();
}

// ------------------------------
// GameFeature URL
// ------------------------------
FString UMosesExperienceManagerComponent::MakeGameFeaturePluginURL(const FString& PluginName)
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
	if (!Plugin.IsValid())
	{
		return FString();
	}

	FString DescriptorFile = Plugin->GetDescriptorFileName();
	FPaths::NormalizeFilename(DescriptorFile);

	return FString::Printf(TEXT("file:%s"), *DescriptorFile);
}

// ------------------------------
// Activate GameFeatures
// ------------------------------
void UMosesExperienceManagerComponent::StartLoadGameFeatures()
{
	check(CurrentExperience);

	LoadState = EMosesExperienceLoadState::LoadingGameFeatures;

	const TArray<FString>& Features = CurrentExperience->GameFeaturesToEnable;

	ScreenMsg(FColor::Cyan,
		FString::Printf(TEXT("[EXP-MGR][GF] Start. Num=%d"), Features.Num()));

	// GF가 없으면 바로 READY
	if (Features.Num() == 0)
	{
		UE_LOG(LogMosesExp, Log, TEXT("[GF] None -> READY"));
		ScreenMsg(FColor::Green, TEXT("[EXP-MGR][GF] None -> READY"));
		OnExperienceFullLoadCompleted();
		return;
	}

	PendingGFCount = Features.Num();
	CompletedGFCount = 0;
	bAnyGFFailed = false;

	UGameFeaturesSubsystem& GFS = UGameFeaturesSubsystem::Get();

	UE_LOG(LogMosesExp, Warning, TEXT("[GF] Enabling %d features..."), PendingGFCount);
	ScreenMsg(FColor::Cyan, FString::Printf(TEXT("[EXP-MGR][GF] Enabling %d..."), PendingGFCount));

	for (const FString& PluginName : Features)
	{
		const FString URL = MakeGameFeaturePluginURL(PluginName);

		UE_LOG(LogMosesExp, Warning, TEXT("[GF] Request Activate: %s URL=%s"), *PluginName, *URL);
		ScreenMsg(FColor::Cyan, FString::Printf(TEXT("[EXP-MGR][GF] Req %s"), *PluginName));

		if (URL.IsEmpty())
		{
			// URL 생성 실패 = 플러그인 못 찾음
			bAnyGFFailed = true;
			CompletedGFCount++;
			ScreenMsg(FColor::Red, FString::Printf(TEXT("[EXP-MGR][GF] URL EMPTY %s"), *PluginName));
			continue;
		}

		GFS.LoadAndActivateGameFeaturePlugin(
			URL,
			FGameFeaturePluginLoadComplete::CreateUObject(
				this,
				&ThisClass::OnOneGameFeatureActivated,
				PluginName
			)
		);
	}
}

void UMosesExperienceManagerComponent::OnOneGameFeatureActivated(const UE::GameFeatures::FResult& /*Result*/, FString PluginName)
{
	CompletedGFCount++;

	UE_LOG(LogMosesExp, Warning, TEXT("[GF] Activate Completed Callback: %s (%d/%d)"),
		*PluginName, CompletedGFCount, PendingGFCount);

	ScreenMsg(FColor::Cyan,
		FString::Printf(TEXT("[EXP-MGR][GF] Done %s (%d/%d)"),
			*PluginName, CompletedGFCount, PendingGFCount));

	if (CompletedGFCount >= PendingGFCount)
	{
		if (bAnyGFFailed)
		{
			FailExperienceLoad(TEXT("One or more GameFeatures had invalid URL"));
			return;
		}

		UE_LOG(LogMosesExp, Warning, TEXT("[GF] Callbacks all arrived -> READY"));
		ScreenMsg(FColor::Green, TEXT("[EXP-MGR][GF] All callbacks -> READY"));

		OnExperienceFullLoadCompleted();
	}
}

// ------------------------------
// READY
// ------------------------------
void UMosesExperienceManagerComponent::OnExperienceFullLoadCompleted()
{
	if (LoadState == EMosesExperienceLoadState::Failed)
	{
		return;
	}

	LoadState = EMosesExperienceLoadState::Loaded;

	// READY 1회만
	if (!bNotifiedReadyOnce)
	{
		bNotifiedReadyOnce = true;

		UE_LOG(LogMosesExp, Log, TEXT("✅ READY: %s"), *CurrentExperienceId.ToString());
		ScreenMsg(FColor::Green, FString::Printf(TEXT("[EXP-MGR][READY] %s"), *CurrentExperienceId.ToString()));

		// 멀티캐스트(있으면)
		OnExperienceLoaded.Broadcast(CurrentExperience);

		// CallOrRegister로 모아둔 콜백 실행
		ScreenMsg(FColor::Green,
			FString::Printf(TEXT("[EXP-MGR][READY] ExecCallbacks Count=%d"), OnExperienceLoadedCallbacks.Num()));

		/* 주요 부분 : 여기서 현재 Experience 로딩 완료되었다고 GameMode에 토스!!! */
		for (FMosesExperienceLoadedDelegate& MosesExperienceLoadedDelegate : OnExperienceLoadedCallbacks)
		{
			MosesExperienceLoadedDelegate.ExecuteIfBound(CurrentExperience);
		}
		OnExperienceLoadedCallbacks.Reset();
	}

	DebugDump();
}

// ------------------------------
// Reset / Fail / Debug
// ------------------------------
void UMosesExperienceManagerComponent::ResetExperienceLoadState()
{
	LoadState = EMosesExperienceLoadState::Unloaded;

	CurrentExperience = nullptr;
	bNotifiedReadyOnce = false;
	PendingExperienceId = FPrimaryAssetId();

	// 🚫 절대 지우면 안 됨:
	// GameMode(InitGameState)에서 미리 등록한 콜백이 여기서 날아가면 READY가 GameMode로 절대 전달 안 됨
	// OnExperienceLoadedCallbacks.Reset();

	PendingGFCount = 0;
	CompletedGFCount = 0;
	bAnyGFFailed = false;

	ScreenMsg(FColor::Silver, TEXT("[EXP-MGR][Reset] State cleared (callbacks preserved)"));
}

void UMosesExperienceManagerComponent::FailExperienceLoad(const FString& Reason)
{
	LoadState = EMosesExperienceLoadState::Failed;

	UE_LOG(LogMosesExp, Error, TEXT("❌ LOAD FAILED: %s"), *Reason);
	ScreenMsg(FColor::Red, FString::Printf(TEXT("[EXP-MGR][FAIL] %s"), *Reason));

	DebugDump();
}

void UMosesExperienceManagerComponent::DebugDump() const
{
	UE_LOG(LogMosesExp, Log, TEXT("Dump ---------------------------"));
	UE_LOG(LogMosesExp, Log, TEXT("  State: %d"), (int32)LoadState);
	UE_LOG(LogMosesExp, Log, TEXT("  ExperienceId: %s"), *CurrentExperienceId.ToString());
	UE_LOG(LogMosesExp, Log, TEXT("  PendingId: %s"), *PendingExperienceId.ToString());
	UE_LOG(LogMosesExp, Log, TEXT("  Experience: %s"), CurrentExperience ? *CurrentExperience->GetName() : TEXT("null"));
	UE_LOG(LogMosesExp, Log, TEXT("  GF: Pending=%d Completed=%d AnyFailed=%d"),
		PendingGFCount, CompletedGFCount, bAnyGFFailed ? 1 : 0);
	UE_LOG(LogMosesExp, Log, TEXT("----------------------------------------"));
}
