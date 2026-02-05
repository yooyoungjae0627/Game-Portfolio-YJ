#include "MosesEventDrivenWidgetBase.h"

#include "UE5_Multi_Shooter/MosesLogChannels.h"

UMosesEventDrivenWidgetBase::UMosesEventDrivenWidgetBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 버전마다 Tick 제어 API가 달라서, 여기서 특정 함수 호출은 하지 않는다.
	// 대신 NativeTick을 final로 막고, 호출 시 로그로 “헌법 위반”을 즉시 드러낸다.
}

void UMosesEventDrivenWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!bDelegatesBound)
	{
		bDelegatesBound = true;
		BindDelegates();
		UE_LOG(LogMosesPhase, Warning, TEXT("[HUD][EVT] BindDelegates Widget=%s"), *GetName());
	}
}

void UMosesEventDrivenWidgetBase::NativeDestruct()
{
	if (bDelegatesBound)
	{
		bDelegatesBound = false;
		UnbindDelegates();
		UE_LOG(LogMosesPhase, Warning, TEXT("[HUD][EVT] UnbindDelegates Widget=%s"), *GetName());
	}

	Super::NativeDestruct();
}

void UMosesEventDrivenWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	// ✅ Tick이 호출되는 순간 자체가 “헌법 위반”이므로 경고를 남긴다.
	UE_LOG(LogMosesPhase, Warning, TEXT("[HUD][TICK][VIOLATION] NativeTick called! Widget=%s Delta=%.4f"),
		*GetName(), InDeltaTime);

	// 실제 갱신 로직은 절대 넣지 않는다.
}
