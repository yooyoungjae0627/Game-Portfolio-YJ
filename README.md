# 🎯 UE5 Dedicated Server FFA Flag Capture  
## 포트폴리오 카테고리별 소개 설명 스크립트  
### (DETAILED · MD · FINAL)

---

## 📌 문서 개요

이 문서는 **UE5 Dedicated Server 기반 멀티플레이 게임 포트폴리오 전체를 설명하기 위한 공식 스크립트**다.  

면접, README, 기술 발표, 코드 리뷰, 영상 내레이션 어디에 사용해도 무리가 없도록  

> **카테고리별 · 설계 의도 중심 · 기술 근거 중심**

으로 작성되었다.

---

# 1️⃣ 프로젝트 개요 (Project Overview)

이 프로젝트는 **UE5 Dedicated Server 환경에서 동작하는 개인전(Free-for-All) PvPvE Flag Capture 게임**이다.

목표는 단순히

> “멀티플레이 게임을 만들어봤다”

가 아니라,

- **Server Authority 100% 구조**
- **PlayerState 기반 SSOT 설계**
- **Lyra 스타일 Experience / GameFeature / GAS 적용**
- **시작 → 경쟁 → 종료 → 저장 → 재접속까지 이어지는 완결된 세션 구조**

를 코드와 실제 동작 증거로 증명하는 것이다.

---

# 2️⃣ 핵심 게임 목표 (Core Gameplay Goal)

> **“정해진 시간 동안 깃발을 더 많이 캡처한 플레이어가 승리한다.”**

- PvP는 목표 달성을 방해하기 위한 수단
- 좀비는 전장을 복잡하게 만드는 압박 요소

플레이어는 항상 다음을 요구받는다:

- 리스크 판단
- 위치 선정
- 타이밍 선택

---

# 3️⃣ 네트워크 & 권한 설계 (Server Authority / SSOT)

## 🔹 서버와 클라이언트 역할 분리

### 클라이언트
- 입력 의도 전달
- 결과 시각화

### 서버
- 모든 판정 수행
- 상태 변경 확정
- 점수 계산 및 기록 저장

---

## 🔹 단일 진실 (SSOT)

핵심 상태는 모두 **PlayerState**에만 존재한다.

- HP / Shield
- Ammo
- Captures
- PvPKills / ZombieKills / Headshots

Pawn은 애니메이션과 이펙트 등  
**코스메틱 전용 계층**이다.

이로 인해:

- Late Join 완전 복구
- 치팅 가능성 최소화

---

# 4️⃣ 전체 게임 흐름 (Session Lifecycle)

StartGameLevel  
→ LobbyLevel  
→ MatchLevel  
→ Warmup  
→ Match  
→ Result  
→ JSON 저장  
→ LobbyLevel

## 🔹 StartGameLevel
- ID 입력
- 서버 검증
- Lobby로 ServerTravel

## 🔹 LobbyLevel
- 방 생성 / 입장
- Ready 체크
- Host만 시작 가능

## 🔹 MatchLevel
- 서버 Phase 관리
- Warmup → Match → Result 자동 전환

---

# 5️⃣ Warmup / Match 설계

## 🔹 Warmup (30초)

- 이동 가능
- 사격 테스트 가능
- 점수 집계 없음
- Late Join 허용

## 🔹 Match (15분)

- 깃발 활성화
- PvP / PvE 활성
- 역전 가능 구조 유지

---

# 6️⃣ Flag Capture 시스템

## 🔹 규칙

- E키 3초 유지
- 타이머는 서버 전용

### 취소 조건
- Release
- Zone 이탈
- 사망

피격만으로 취소되지 않는다.  
플레이어가 판단하도록 설계했다.

### 성공 시 처리

- PlayerState Captures 증가
- Announcement 표시
- 깃발 랜덤 이동

---

# 7️⃣ Flag Capture UI 구조

HUD와 분리된 전용 위젯 구조.

- WBP_CaptureProgress (로직 포함)
- WBP_MatchHUD (배치 전용)

Tick 없이 이벤트 기반 처리.

---

# 8️⃣ 무기 & 전투 개요

## 🔹 무기 슬롯

1. Rifle  
2. Shotgun  
3. Sniper  
4. Grenade Launcher  

## 🔹 기본 지급

Match 진입 시 서버가 Rifle 지급.

## 🔹 탄약

- 서버에서만 감소 확정
- RepNotify 기반 HUD 갱신

---

# 9️⃣ 스나이퍼 & 유탄

## 🔹 스나이퍼

- UI는 로컬 처리
- 판정은 서버

## 🔹 유탄 발사기

- 서버 Projectile 스폰
- GAS RadialDamage 적용

---

# 🔟 GAS 전투 시스템 (단일 파이프라인 구조)

이 프로젝트의 전투는 **Gameplay Ability System 기반 단일 파이프라인 구조**다.

전투 흐름:

Ability  
→ GameplayEffect  
→ AttributeSet  
→ Replication

---

## 🔹 Ability 계층 (GA_Fire)

Fire 입력은 GA_Fire로 정규화된다.

- NetExecutionPolicy = ServerOnly
- 클라는 Activate 요청만 수행
- 실행은 서버에서만 진행

### 서버 내부 흐름

1. 발사 조건 검증 (Ammo / Cooldown / 상태)
2. 서버 Trace 수행
3. Headshot / Falloff 계산
4. DamageValue 확정

서버가 최종 값을 완전히 결정한다.

---

## 🔹 ASC 위치 차이 해결

- 플레이어 ASC → PlayerState
- 좀비 ASC → Pawn

ResolveTargetASC 단계에서 이를 통합한다.

이후 전투 코드는 항상 TargetASC만 사용한다.

---

## 🔹 GameplayEffect 설계

GE는 계산기가 아니다.  
서버 확정 값을 전달하는 포맷이다.

- GE_AmmoCost
- GE_Damage (SetByCaller)

---

## 🔹 AttributeSet 규칙 처리

IncomingDamage  
→ PostGameplayEffectExecute

- Shield 감소
- Health 감소
- Health ≤ 0 → 서버 Death 확정

규칙은 한 지점에만 존재한다.

---

## 🔹 GameplayCue

- MuzzleFlash
- HitImpact
- Hitmarker

규칙과 연출을 완전 분리했다.

---

# 1️⃣1️⃣ HUD 시스템

- Tick 없음
- Binding 없음

RepNotify → Delegate 기반 구조.

---

# 1️⃣2️⃣ 좀비 시스템 개요

좀비는 전장 압박 요소.

- Flag Spot 주변 배치
- 깃발 이동 시 리스폰

---

# 1️⃣3️⃣ 좀비 공격 시스템 (Zombie Combat)

## 🔹 공격 흐름

1. 서버 Spawn
2. AIController + Perception
3. BehaviorTree 추적
4. 서버 공격 시작
5. Multicast 애니
6. NotifyState 구간 히트박스 활성

---

## 🔹 Attack Window

- 정확한 타격 프레임 판정
- 지속 타격 문제 제거

---

## 🔹 히트박스

- 손 소켓 부착
- 윈도우 동안만 Collision ON
- TSet으로 중복 방지

---

## 🔹 데미지

- GAS SetByCaller 적용
- 서버 전용 처리

---

# 1️⃣4️⃣ 좀비 리스폰 & 압박

- Spot 단위 관리
- 안전 지대 제거
- 긴장도 유지

---

# 1️⃣5️⃣ Result & Persistence

- 서버 승자 판정
- JSON 서버 저장
- 재접속 후 조회 가능

---

# 1️⃣6️⃣ 대규모 인원 대응

- Significance 정책
- Render / Anim / Network 분리
- Tick 제거

---

# 1️⃣7️⃣ Evidence-First 설계 철학

모든 시스템은 로그로 검증된다.

- Capture
- Fire 승인
- GAS Apply
- Save / Load

---

# 🔚 최종 요약

이 포트폴리오는  
서버 권위와 단일 진실을 기반으로,  
GAS 단일 전투 파이프라인을 통해 규칙을 통합하고,  
좀비·플레이어·깃발이 상호작용하는 전장을  
이벤트 기반 HUD와 완결된 세션 구조로 구현한  
**UE5 Dedicated Server 멀티플레이 포트폴리오다.**
