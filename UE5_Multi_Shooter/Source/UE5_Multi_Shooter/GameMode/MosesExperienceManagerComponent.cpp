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

	// 로그: 서버 확정 결과 출력
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
// ExperienceDefinition loaded (PrimaryAsset 로딩 완료 콜백)
// ------------------------------
void UMosesExperienceManagerComponent::OnExperienceAssetsLoaded()
{
	// [비동기 로딩에서 가장 흔한 버그 방지]
	// Experience 로딩은 비동기(Async)로 끝나기 때문에,
	// 로딩 도중 CurrentExperienceId가 바뀌면 "이 콜백이 오래된(stale) 결과"일 수 있음.
	//
	// 예)
	// - Pending=A 로딩 시작
	// - 중간에 Current=B로 변경
	// - A 로딩 콜백이 늦게 도착 → 여기서 그대로 적용하면 상태 꼬임
	//
	// 그래서 "내가 기다리던 ID == 현재 ID"인지 검증하고 아니면 무시.
	if (PendingExperienceId != CurrentExperienceId)
	{
		UE_LOG(LogMosesExp, Warning,
			TEXT("[EXP] AssetsLoaded stale. Pending=%s Current=%s"),
			*PendingExperienceId.ToString(),
			*CurrentExperienceId.ToString()
		);

		ScreenMsg(FColor::Silver,
			FString::Printf(TEXT("[EXP-MGR][AssetsLoaded] STALE Pending=%s Current=%s"),
				*PendingExperienceId.ToString(),
				*CurrentExperienceId.ToString()
			)
		);
		return;
	}

	// AssetManager를 통해 PrimaryAssetObject를 꺼내는 단계
	// - 여기서 ExperienceDefinition(UPrimaryDataAsset)을 "이미 로딩된 상태"에서 가져옴
	UMosesAssetManager& AssetManager = UMosesAssetManager::Get();

	const UMosesExperienceDefinition* ExperienceDef =
		AssetManager.GetPrimaryAssetObject<UMosesExperienceDefinition>(CurrentExperienceId);

	// 로딩 성공 콜백이 왔는데도 null이면,
	// - AssetManager 스캔 설정 문제
	// - PrimaryAssetId 잘못됨
	// - 패키지/경로 문제
	// 같은 비정상 상황이므로 Fail 처리
	if (!ExperienceDef)
	{
		FailExperienceLoad(TEXT("OnExperienceAssetsLoaded: ExperienceDef null"));
		return;
	}

	// 이제부터 "현재 경험(Experience)"는 이 Definition으로 확정
	// - 이후 단계(GameFeature, PawnData, AbilitySet 등)는
	//   CurrentExperience를 기준으로 진행됨
	CurrentExperience = ExperienceDef;

	UE_LOG(LogMosesExp, Log,
		TEXT("[EXP] Loaded ExperienceDefinition=%s"),
		*CurrentExperienceId.ToString()
	);
	ScreenMsg(FColor::Yellow,
		FString::Printf(TEXT("[EXP-MGR][AssetsLoaded] OK %s"),
			*CurrentExperienceId.ToString()
		)
	);

	// 다음 단계:
	// ExperienceDefinition에 적힌 GameFeaturesToEnable 목록을 활성화(Load+Activate)
	// ※ 여기서부터도 비동기이며 "모두 완료"되어야 READY가 됨
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
// Activate GameFeatures (GF 플러그인 Load + Activate 단계)
// ------------------------------
void UMosesExperienceManagerComponent::StartLoadGameFeatures()
{
	// CurrentExperience는 앞 단계에서 세팅되어야만 함.
	// nullptr이면 로딩 파이프라인 순서가 깨진 것.
	check(CurrentExperience);

	// 로드 상태 머신:
	// AssetsLoaded → LoadingGameFeatures → (모두 완료) → READY
	LoadState = EMosesExperienceLoadState::LoadingGameFeatures;

	// 이 Experience가 활성화해야 할 GameFeature 플러그인 목록
	// (예: "GF_ShooterCore", "GF_UI", "GF_Weapons" 등)
	const TArray<FString>& Features = CurrentExperience->GameFeaturesToEnable;

	ScreenMsg(FColor::Cyan,
		FString::Printf(TEXT("[EXP-MGR][GF] Start. Num=%d"), Features.Num())
	);

	// [최적화 + 단순화]
	// 활성화할 GF가 하나도 없으면, 더 기다릴 비동기가 없으므로 즉시 READY로 진행 가능
	if (Features.Num() == 0)
	{
		UE_LOG(LogMosesExp, Log, TEXT("[GF] None -> READY"));
		ScreenMsg(FColor::Green, TEXT("[EXP-MGR][GF] None -> READY"));

		// "Experience 전체 로딩 완료" 처리
		// - 여기서 PendingStartPlayers Flush / OnExperienceLoaded 브로드캐스트 같은 걸 하게 됨
		OnExperienceFullLoadCompleted();
		return;
	}

	// [비동기 완료 카운팅]
	// Features 각각은 비동기로 활성화되며,
	// "모든 요청이 완료"되어야만 다음 단계(READY)로 넘어갈 수 있음.
	PendingGFCount = Features.Num(); // 총 요청 수
	CompletedGFCount = 0;              // 완료 콜백 도착 수
	bAnyGFFailed = false;          // 하나라도 실패했는지

	// GameFeaturesSubsystem은
	// - 플러그인 로드
	// - GameFeature Action 적용(컴포넌트 추가, 데이터 등록 등)
	// 을 담당하는 엔진 시스템
	UGameFeaturesSubsystem& GFS = UGameFeaturesSubsystem::Get();

	UE_LOG(LogMosesExp, Warning, TEXT("[GF] Enabling %d features..."), PendingGFCount);
	ScreenMsg(FColor::Cyan, FString::Printf(TEXT("[EXP-MGR][GF] Enabling %d..."), PendingGFCount));

	// 각 GameFeature 플러그인에 대해 Load+Activate 요청
	for (const FString& PluginName : Features)
	{
		// PluginName → URL로 변환
		// 엔진의 LoadAndActivateGameFeaturePlugin은 "URL" 형태를 요구하는 경우가 많아서
		// (예: "file:.../GF_ShooterCore.uplugin" 또는 "GameFeaturePlugin:/GF_ShooterCore")
		const FString URL = MakeGameFeaturePluginURL(PluginName);

		UE_LOG(LogMosesExp, Warning,
			TEXT("[GF] Request Activate: %s URL=%s"),
			*PluginName,
			*URL
		);
		ScreenMsg(FColor::Cyan,
			FString::Printf(TEXT("[EXP-MGR][GF] Req %s"), *PluginName)
		);

		if (URL.IsEmpty())
		{
			// URL 생성 실패는 사실상 "플러그인 못 찾음"이라서 실패로 기록
			// 그래도 전체 파이프라인을 멈추지 않고 카운트를 진행해야
			// 나중에 '모든 요청이 끝났는지' 판단 가능함.
			bAnyGFFailed = true;
			CompletedGFCount++;

			ScreenMsg(FColor::Red,
				FString::Printf(TEXT("[EXP-MGR][GF] URL EMPTY %s"), *PluginName)
			);

			// 주의:
			// 여기서 "완료 증가"만 하고 끝내면,
			// 마지막에는 Completed==Pending이 되어 OnExperienceFullLoadCompleted가 호출되도록
			// OnOneGameFeatureActivated 쪽에서 Completed를 검사하는 로직이 있어야 함.
			continue;
		}

		// 비동기 Load + Activate 요청
		// 완료되면 OnOneGameFeatureActivated(PluginName) 콜백이 호출됨
		//
		// 여기서 중요한 점:
		// - 요청 순서대로 완료되지 않을 수 있음 (완료 순서가 랜덤)
		// - 일부는 실패할 수 있음
		// - 그래서 완료 카운팅 + 실패 플래그가 필요
		GFS.LoadAndActivateGameFeaturePlugin(
			URL,
			FGameFeaturePluginLoadComplete::CreateUObject(
				this,
				&ThisClass::OnOneGameFeatureActivated, // 각 GF 완료 콜백
				PluginName                              // 어떤 플러그인 요청이었는지 식별용
			)
		);
	}
}


// ---------------------------------------------
// GameFeature 플러그인 하나의 활성화가 끝났을 때 호출되는 콜백
// ---------------------------------------------
void UMosesExperienceManagerComponent::OnOneGameFeatureActivated(
	const UE::GameFeatures::FResult& /*Result*/,
	FString PluginName
)
{
	// 비동기 요청 1건 완료
	// (성공/실패 여부와 상관없이 "끝났다"는 사실 자체를 카운트)
	CompletedGFCount++;

	UE_LOG(LogMosesExp, Warning,
		TEXT("[GF] Activate Completed Callback: %s (%d/%d)"),
		*PluginName, CompletedGFCount, PendingGFCount
	);

	ScreenMsg(FColor::Cyan,
		FString::Printf(TEXT("[EXP-MGR][GF] Done %s (%d/%d)"),
			*PluginName, CompletedGFCount, PendingGFCount)
	);

	// 모든 GameFeature 활성화 요청의 콜백이 도착했는지 확인
	// >= 로 비교하는 이유:
	// - 예외적인 중복 콜백 상황에서도 READY로 넘어가기 위한 방어 코드
	if (CompletedGFCount >= PendingGFCount)
	{
		// 하나라도 GF 로딩/활성화가 실패한 적이 있다면
		// Experience 전체를 실패 상태로 처리
		if (bAnyGFFailed)
		{
			FailExperienceLoad(TEXT("One or more GameFeatures had invalid URL"));
			return;
		}

		// 모든 GameFeature가 정상적으로 활성화됨
		UE_LOG(LogMosesExp, Warning, TEXT("[GF] Callbacks all arrived -> READY"));
		ScreenMsg(FColor::Green, TEXT("[EXP-MGR][GF] All callbacks -> READY"));

		// Experience 로딩 파이프라인 최종 완료 단계로 진입
		OnExperienceFullLoadCompleted();
	}
}


// ---------------------------------------------
// Experience READY (최종 로딩 완료 지점)
// ---------------------------------------------
void UMosesExperienceManagerComponent::OnExperienceFullLoadCompleted()
{
	// 이미 실패 상태면 READY로 올리면 안 됨
	// (중간에 GF 실패 → 이후 콜백이 와도 무시해야 함)
	if (LoadState == EMosesExperienceLoadState::Failed)
	{
		return;
	}

	// Experience 로딩 완료 상태로 전환
	LoadState = EMosesExperienceLoadState::Loaded;

	// READY 처리는 반드시 "한 번만" 실행되어야 함
	// - GF가 0개일 때
	// - GF 콜백이 몰려서 여러 경로로 들어올 때
	// 중복 실행 방지용 가드
	if (!bNotifiedReadyOnce)
	{
		bNotifiedReadyOnce = true;

		UE_LOG(LogMosesExp, Log, TEXT("✅ READY: %s"), *CurrentExperienceId.ToString());
		ScreenMsg(FColor::Green,
			FString::Printf(TEXT("[EXP-MGR][READY] %s"),
				*CurrentExperienceId.ToString())
		);

		// ---------------------------------
		// 1) Experience READY 이벤트 브로드캐스트
		// ---------------------------------
		// - 다른 시스템(GameMode, Subsystem, UI 등)이
		//   "이제 Experience를 써도 된다"는 신호를 받음
		OnExperienceLoaded.Broadcast(CurrentExperience);

		// ---------------------------------
		// 2) READY 이전에 등록된 대기 콜백 실행
		// ---------------------------------
		// CallOrRegister_OnExperienceLoaded 로 등록된 작업들:
		// - Pawn 스폰 재시도
		// - Input Mapping 적용
		// - AbilitySet 적용
		// - Lobby → Game 전환 처리 등
		ScreenMsg(FColor::Green,
			FString::Printf(TEXT("[EXP-MGR][READY] ExecCallbacks Count=%d"),
				OnExperienceLoadedCallbacks.Num())
		);

		// ★ 핵심 포인트
		// Experience 로딩 완료를 기다리던 모든 로직이
		// 이 지점에서 "한 번에" 실행된다
		for (FMosesExperienceLoadedDelegate& Delegate : OnExperienceLoadedCallbacks)
		{
			Delegate.ExecuteIfBound(CurrentExperience);
		}

		// 재사용 방지
		OnExperienceLoadedCallbacks.Reset();
	}

	// 현재 Experience 상태를 로그로 출력 (디버깅용)
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
