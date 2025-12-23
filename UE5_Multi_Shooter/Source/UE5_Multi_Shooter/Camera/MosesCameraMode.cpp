// Fill out your copyright notice in the Description page of Project Settings.


#include "MosesCameraMode.h"

#include "MosesPlayerCameraManager.h"
#include "MosesCameraComponent.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// FMosesCameraModeView : "카메라가 최종적으로 봐야 할 정보"를 담는 구조체
///////////////////////////////////////////////////////////////////////////////////////////////////

FMosesCameraModeView::FMosesCameraModeView()
	: Location(ForceInit)           // 카메라 위치
	, Rotation(ForceInit)           // 카메라 회전
	, ControlRotation(ForceInit)    // 컨트롤 기준 회전 (보통 컨트롤러/플레이어 시야)
	, FieldOfView(Moses_CAMERA_DEFAULT_FOV) // FOV 기본값 설정
{
}

void FMosesCameraModeView::Blend(const FMosesCameraModeView& Other, float OtherWeight)
{
	// OtherWeight가 0 이하이면 섞을 필요 없음
	if (OtherWeight <= 0.0f)
	{
		return;
	}
	// 1 이상이면 Other 값으로 완전히 덮어씌우면 됨
	else if (OtherWeight >= 1.0f)
	{
		*this = Other;
		return;
	}

	// 위치 Lerp: 현재 Location에서 Other.Location으로 OtherWeight 비율만큼 이동
	// Location = Location + OtherWeight * (Other.Location - Location);
	Location = FMath::Lerp(Location, Other.Location, OtherWeight);

	// 회전도 동일한 개념으로 Lerp (회전이기 때문에 Normalize 후 보간)
	const FRotator DeltaRotation = (Other.Rotation - Rotation).GetNormalized();
	Rotation = Rotation + (OtherWeight * DeltaRotation);

	const FRotator DeltaControlRotation = (Other.ControlRotation - ControlRotation).GetNormalized();
	ControlRotation = ControlRotation + (OtherWeight * DeltaControlRotation);

	// FOV도 선형 보간
	FieldOfView = FMath::Lerp(FieldOfView, Other.FieldOfView, OtherWeight);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// UMosesCameraMode : 카메라 모드 1개(3인칭, 조준 시점 등)를 표현하는 클래스
///////////////////////////////////////////////////////////////////////////////////////////////////

UMosesCameraMode::UMosesCameraMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 카메라 모드 기본 설정값
	FieldOfView = Moses_CAMERA_DEFAULT_FOV;
	ViewPitchMin = Moses_CAMERA_DEFAULT_FOV;
	ViewPitchMax = Moses_CAMERA_DEFAULT_FOV;

	// 블렌딩 관련 초기값
	BlendTime = 0.0f;      // 0이면 즉시 전환
	BlendAlpha = 1.0f;     // 0~1 사이 진행도
	BlendWeight = 1.0f;    // 최종 가중치

	BlendFunction = EMosesCameraModeBlendFunction::EaseOut; // 부드럽게 빠지는 형태
	BlendExponent = 4.0f;  // Ease 계열 곡선 세기
}

UMosesCameraComponent* UMosesCameraMode::GetMosesCameraComponent() const
{
	// 이 CameraMode는 UMosesCameraModeStack에서 NewObject로 생성될 때
	// Outer를 UMosesCameraComponent로 지정해두었다.
	// → 따라서 GetOuter()를 캐스팅하면 사용할 수 있다.
	//   (어떤 CameraComponent에 붙어있는 CameraMode인지 알 수 있음)
	return CastChecked<UMosesCameraComponent>(GetOuter());
}

AActor* UMosesCameraMode::GetTargetActor() const
{
	const UMosesCameraComponent* MosesCameraComponent = GetMosesCameraComponent();
	return MosesCameraComponent->GetTargetActor();
}

FVector UMosesCameraMode::GetPivotLocation() const
{
	// 카메라가 기준으로 삼을 "Pivot 위치" 얻기 (보통 캐릭터 머리 근처)
	const AActor* TargetActor = GetTargetActor();
	check(TargetActor); // TargetActor가 반드시 있어야 한다 (아직은 null)

	// Pawn이면 PawnViewLocation(=BaseEyeHeight 반영된 위치)을 사용
	if (const APawn* TargetPawn = Cast<APawn>(TargetActor))
	{
		// BaseEyeHeight를 고려하여, ViewLocation을 반환함 (머리/눈 위치)
		return TargetPawn->GetPawnViewLocation();
	}

	// 일반 Actor면 그냥 Actor Location 사용
	return TargetActor->GetActorLocation();
}

FRotator UMosesCameraMode::GetPivotRotation() const
{
	// 카메라가 기준으로 삼을 회전 (보통 컨트롤러 회전)
	const AActor* TargetActor = GetTargetActor();
	check(TargetActor);

	if (const APawn* TargetPawn = Cast<APawn>(TargetActor))
	{
		// Pawn의 GetViewRotation()은 보통 Controller의 ControlRotation을 반환한다.
		// → 플레이어가 보고 있는 방향에 맞춰서 카메라 회전이 결정되도록 하기 위함.
		return TargetPawn->GetViewRotation();
	}

	// Pawn이 아니면 Actor의 기본 회전 사용
	return TargetActor->GetActorRotation();
}

void UMosesCameraMode::UpdateView(float DeltaTime)
{
	// 1) Pivot 위치/회전 구하기 (보통 캐릭터의 머리 위치 + 시야 방향)
	FVector PivotLocation = GetPivotLocation();
	FRotator PivotRotation = GetPivotRotation();

	// 2) Pitch 값은 최소/최대 각도 사이로 제한
	PivotRotation.Pitch = FMath::ClampAngle(PivotRotation.Pitch, ViewPitchMin, ViewPitchMax);

	// 3) 계산한 PivotLocation/Rotation을 View에 반영
	View.Location = PivotLocation;
	View.Rotation = PivotRotation;

	// 4) ControlRotation은 일단 View.Rotation과 동일하게 사용
	View.ControlRotation = View.Rotation;

	// 5) FOV도 현재 모드의 FOV 사용
	View.FieldOfView = FieldOfView;

	// 정리:
	// - "이 카메라 모드에서 봐야 하는 위치/회전/FOV"를 View에 담아놓는다.
}

void UMosesCameraMode::UpdateBlending(float DeltaTime)
{
	// BlendAlpha(0~1)를 DeltaTime을 사용해서 증가시킴
	if (BlendTime > 0.f)
	{
		// BlendTime 동안 0 → 1로 천천히 올라간다.
		BlendAlpha += (DeltaTime / BlendTime);
	}
	else
	{
		// BlendTime이 0이면 바로 전환
		BlendAlpha = 1.0f;
	}

	// BlendFunction에 따라 BlendAlpha를 0~1 사이의 Weight로 재계산
	const float Exponent = (BlendExponent > 0.0f) ? BlendExponent : 1.0f;

	switch (BlendFunction)
	{
	case EMosesCameraModeBlendFunction::Linear:
		BlendWeight = BlendAlpha; // 직선형
		break;

	case EMosesCameraModeBlendFunction::EaseIn:
		BlendWeight = FMath::InterpEaseIn(0.0f, 1.0f, BlendAlpha, Exponent);
		break;

	case EMosesCameraModeBlendFunction::EaseOut:
		BlendWeight = FMath::InterpEaseOut(0.0f, 1.0f, BlendAlpha, Exponent);
		break;

	case EMosesCameraModeBlendFunction::EaseInOut:
		BlendWeight = FMath::InterpEaseInOut(0.0f, 1.0f, BlendAlpha, Exponent);
		break;

	default:
		checkf(false, TEXT("UpdateBlending: Invalid BlendFunction [%d]\n"), (uint8)BlendFunction);
		break;
	}
}

void UMosesCameraMode::UpdateCameraMode(float DeltaTime)
{
	// 1) 이 모드 기준으로 카메라 View 정보 계산 (위치/회전/FOV)
	UpdateView(DeltaTime);

	// 2) 이 모드의 BlendWeight(얼마나 화면에 섞일지)를 업데이트
	UpdateBlending(DeltaTime);
}

UMosesCameraModeStack::UMosesCameraModeStack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// CameraModeClass에 해당하는 인스턴스를 리턴.
// - 이미 있으면 재사용
// - 없으면 NewObject로 생성 후, CameraModeInstances에 캐시해둔다.
UMosesCameraMode* UMosesCameraModeStack::GetCameraModeInstance(TSubclassOf<UMosesCameraMode>& CameraModeClass)
{
	check(CameraModeClass);

	// 1) 이미 생성된 인스턴스가 있는지 확인
	for (UMosesCameraMode* CameraMode : CameraModeInstances)
	{
		// CameraMode는 클래스 타입을 기준으로 1개만 유지한다.
		if ((CameraMode != nullptr) && (CameraMode->GetClass() == CameraModeClass))
		{
			return CameraMode;
		}
	}

	// 2) 없으면 새로 생성 (Outer는 CameraComponent)


	UMosesCameraMode* NewCameraMode = NewObject<UMosesCameraMode>(GetOuter(), CameraModeClass, NAME_None, RF_NoFlags);
	check(NewCameraMode);

	// 3) 캐시에 추가
	CameraModeInstances.Add(NewCameraMode);

	return NewCameraMode;
}

//> GetOuter()는 "나를 만든/나를 들고 있는 객체" 를 가져오는 함수
//**UE에서 Outer가 중요한 이유 * *
//-Garbage Collection(메모리 관리) 기준이 됨
//- 하위 객체가 상위 객체를 따라다님(같이 삭제·유지)
//- CameraMode가 어떤 CameraComponent에 속하는지 알려줌
// 즉 Outer는** 라이프사이클과 소속 관계** 를 결정하는 핵심 정보
//UObject* Parent = NewObject<UObject>();
//UObject* Child = NewObject<UObject>(Parent);
//- `Child`의 Outer = `Parent`
//- Parent가 삭제되면 Child도 GC됨
//Child->GetOuter() == Parent; // true

void UMosesCameraModeStack::PushCameraMode(TSubclassOf<UMosesCameraMode>& CameraModeClass)
{
	if (CameraModeClass == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MosesCamera] PushCameraMode: CameraModeClass is NULL"));
		return;
	}

	UE_LOG(LogTemp, Verbose, TEXT("[MosesCamera] PushCameraMode: %s"), *GetNameSafe(CameraModeClass));

	// 1) 클래스에 해당하는 CameraMode 인스턴스 가져오기
	UMosesCameraMode* CameraMode = GetCameraModeInstance(CameraModeClass);
	check(CameraMode);

	int32 StackSize = CameraModeStack.Num();

	// 2) 이미 Top(0번 인덱스)에 같은 모드가 있으면 다시 올릴 필요 없음
	if ((StackSize > 0) && (CameraModeStack[0] == CameraMode))
	{
		// 가장 최근에 사용 중인 모드와 동일 → 그대로 유지
		return;
	}

	// 3) 스택 안에 이미 존재하는지 확인하기 위한 변수들
	int32 ExistingStackIndex = INDEX_NONE;
	float ExistingStackContribution = 1.0f; // 아래쪽부터 올라오면서 남아있는 기여도

	/**
	 * BlendWeight    | ExistingStackContribution 계산 예시
	 *
	 * 0.1f           | (1.0f)   * 0.1f = 0.1f         → 누적된 기여도에서 일부만 사용
	 * 0.3f           | (0.9f)   * 0.3f = 0.27f
	 * 0.6f           | (0.63f)  * 0.6f = 0.378f
	 * 1.0f           | (0.252f) * 1.0f = 0.252f
	 * 최종 합계      | 0.1f + 0.27f + 0.378f + 0.252f = 1.0f
	 *
	 * → 이런 식으로 여러 모드의 Weight를 곱셈/보완 방식으로 섞어서 최종 1.0을 맞춤
	 */
	for (int32 StackIndex = 0; StackIndex < StackSize; ++StackIndex)
	{
		if (CameraModeStack[StackIndex] == CameraMode)
		{
			// 이미 스택 어딘가에 같은 모드가 있었으면
			ExistingStackIndex = StackIndex;

			// 지금까지 누적된 기여도에 이 모드의 BlendWeight까지 반영
			ExistingStackContribution *= CameraMode->BlendWeight;
			break;
		}
		else
		{
			// 아직 찾는 모드가 아니면, (1 - BlendWeight)를 곱해서 남은 기여도 계산
			ExistingStackContribution *= (1.0f - CameraModeStack[StackIndex]->BlendWeight);
		}
	}
	// 위 루프 = "스택 아래에서 위로 올라오면서 이 모드가 기존에 어느 정도 섞여 있었는지 계산하는 과정"

	// 4) 기존 스택에서 해당 모드가 있었다면 제거하고, Top에 다시 올릴 준비
	if (ExistingStackIndex != INDEX_NONE)
	{
		CameraModeStack.RemoveAt(ExistingStackIndex);
		StackSize--;
	}
	else
	{
		// 한 번도 없던 모드면, 이전 기여도는 0에서 시작
		ExistingStackContribution = 0.0f;
	}

	// 5) BlendTime이 0보다 크고, 스택에 다른 모드도 있다면 → 부드럽게 기존 기여도에서 시작
	//    아니면 새 모드만 1.0으로 바로 적용
	const bool bShouldBlend = ((CameraMode->BlendTime > 0.f) && (StackSize > 0));
	const float BlendWeight = (bShouldBlend ? ExistingStackContribution : 1.0f);
	CameraMode->BlendWeight = BlendWeight;

	// 6) 스택의 Top(0번 인덱스)에 새 모드 삽입
	CameraModeStack.Insert(CameraMode, 0);

	// 7) 스택 맨 바닥(Last)은 항상 BlendWeight = 1.0f 로 유지
	//    → 가장 기본이 되는 카메라 모드
	CameraModeStack.Last()->BlendWeight = 1.0f;

	UE_LOG(LogTemp, Verbose, TEXT("[MosesCamera] StackSize after push: %d"), CameraModeStack.Num());
}

// 스택에 있는 각 CameraMode를 매 프레임 업데이트하고, 오래된 모드들을 정리하는 함수
void UMosesCameraModeStack::UpdateStack(float DeltaTime)
{
	const int32 StackSize = CameraModeStack.Num();
	if (StackSize <= 0)
	{
		return;
	}

	int32 RemoveCount = 0;
	int32 RemoveIndex = INDEX_NONE;

	// Top(0)부터 아래로 순회하면서 각 모드를 업데이트
	for (int32 StackIndex = 0; StackIndex < StackSize; ++StackIndex)
	{
		UMosesCameraMode* CameraMode = CameraModeStack[StackIndex];
		check(CameraMode);

		// 개별 카메라 모드의 View/BlendWeight 업데이트
		CameraMode->UpdateCameraMode(DeltaTime);

		// 어떤 모드가 BlendWeight = 1.0에 도달했다면,
		// 그 "아래에 있는 모드들은 더 이상 쓸 필요가 없으니" 제거 대상
		if (CameraMode->BlendWeight >= 1.0f)
		{
			RemoveIndex = (StackIndex + 1);           // 다음 것부터
			RemoveCount = (StackSize - RemoveIndex);  // 나머지 전부 제거
			break;
		}
	}

	// 필요하면 아래쪽 모드들 제거
	if (RemoveCount > 0)
	{
		CameraModeStack.RemoveAt(RemoveIndex, RemoveCount);
	}
}

// 스택에 있는 카메라 모드들을 "BlendWeight"를 기준으로 하나의 View로 합치는 함수
void UMosesCameraModeStack::BlendStack(FMosesCameraModeView& OutCameraModeView) const
{
	const int32 StackSize = CameraModeStack.Num();
	if (StackSize <= 0)
	{
		return;
	}

	// 1) 바닥(Bottom) 모드부터 시작 (가장 기본이 되는 카메라)
	const UMosesCameraMode* CameraMode = CameraModeStack[StackSize - 1];
	check(CameraMode);

	// 바닥 모드는 Weight = 1.0 기준으로 OutView에 먼저 깔아줌
	OutCameraModeView = CameraMode->View;

	// 2) 그 위로 올라가면서 하나씩 Blend 적용 (Bottom → Top 방향)
	for (int32 StackIndex = (StackSize - 2); StackIndex >= 0; --StackIndex)
	{
		CameraMode = CameraModeStack[StackIndex];
		check(CameraMode);

		// 각 CameraMode의 View를 BlendWeight만큼 섞어주기
		OutCameraModeView.Blend(CameraMode->View, CameraMode->BlendWeight);
	}
}

// 스택 전체를 평가(Evaluate)해서 최종 카메라 뷰를 얻는 함수
void UMosesCameraModeStack::EvaluateStack(float DeltaTime, FMosesCameraModeView& OutCameraModeView)
{
	// 1) Top → Bottom 순으로 각 CameraMode 업데이트 및 정리
	UpdateStack(DeltaTime);

	// 2) Bottom → Top 순으로 Blend해서 최종 View 계산
	BlendStack(OutCameraModeView);
}


