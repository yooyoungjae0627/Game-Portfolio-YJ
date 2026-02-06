# 🎯 UE5 Dedicated Server FFA Flag Capture  
## 포트폴리오 카테고리별 소개 설명 스크립트  
### (DETAILED · MD · FINAL)

> 이 문서는 **UE5 Dedicated Server 기반 멀티플레이 게임 포트폴리오 전체를  
타인에게 설명하기 위한 공식 스크립트**다.  
> 면접, README, 기술 발표, 코드 리뷰, 영상 내레이션 어디에 사용해도 무리가 없도록  
> **카테고리별 · 설계 의도 중심 · 기술 근거 중심**으로 작성되었다.

---

## 1️⃣ 프로젝트 개요 (Project Overview)

이 프로젝트는 **UE5 Dedicated Server** 환경에서 동작하는  
**개인전(Free-for-All) PvPvE Flag Capture 게임**이다.

이 포트폴리오의 목표는 단순히  
> “멀티플레이 게임을 만들어봤다”

가 아니라,

- 서버 권위(Server Authority)를 끝까지 유지하는 구조
- PlayerState 단일 진실(SSOT) 기반 상태 관리
- Lyra 스타일 아키텍처(Experience / GameFeature / GAS)의 실전 적용
- 시작 → 경쟁 → 종료 → 기록 저장 → 재접속까지 이어지는 **완결된 게임 세션**

을 **코드, 로그, 실제 동작 증거**로 설명하고 증명하는 데 있다.

---

## 2️⃣ 핵심 게임 목표 (Core Gameplay Goal)

> **“정해진 시간 동안 깃발을 더 많이 캡처한 플레이어가 승리한다.”**

이 게임은 킬 수 중심의 데스매치가 아니다.

- PvP는 **목표 달성을 방해하기 위한 수단**
- 좀비는 **전장을 복잡하게 만드는 환경 압박 요소**
- 플레이어는 **위험을 감수하고 목표를 수행할지**,  
  혹은 **상대를 방해할지**를 끊임없이 선택해야 한다.

이로 인해 플레이는 항상  
- 리스크 판단  
- 위치 선정  
- 타이밍 싸움  

을 요구하도록 설계되었다.

---

## 3️⃣ 네트워크 & 권한 설계 (Server Authority / SSOT)

이 프로젝트는 **Server Authority 100%**를 전제로 설계되었다.

### 서버와 클라이언트의 역할 분리

**클라이언트**
- 입력(의도)만 서버로 전달
- 서버 승인 결과를 시각적으로 표현

**서버**
- 모든 판정 수행
- 상태 변경 확정
- 점수 계산 및 기록 저장

### 단일 진실(SSOT)

모든 핵심 게임 상태는 **PlayerState**에만 존재한다.

- HP / Armor
- Weapon / Ammo
- Captures / PvPKills / ZombieKills / Headshots

Pawn / Character는
- 애니메이션
- 이펙트
- 사운드

같은 **코스메틱 전용 역할**만 담당한다.

이 구조 덕분에:
- Late Join에서도 상태가 정확히 복원되고
- 클라이언트 조작·치트 가능성이 최소화된다.

---

## 4️⃣ 전체 게임 흐름 (Session Lifecycle)

하나의 게임 세션은 다음 흐름으로 **완결**된다.

StartGameLevel  
→ LobbyLevel  
→ MatchLevel  
→ Warmup  
→ Match  
→ Result  
→ 기록 저장  
→ LobbyLevel (기록 조회)

### StartGameLevel
- 플레이어 ID 입력
- 서버 검증
- LobbyLevel로 ServerTravel

### LobbyLevel
- 방 생성 / 입장
- 채팅
- Ready 체크
- Host만 Game Start 가능

### MatchLevel
- 서버가 Phase 관리
- Warmup → Match → Result 자동 전환

---

## 5️⃣ Warmup / Match 단계 설계

### Warmup (30초)

경쟁 전 준비 단계다.

- 이동 가능
- 사격 테스트 가능
- 무기 / 탄약 / 수류탄 파밍 가능
- 점수 집계 ❌
- Late Join 허용

**의도**
- 초반 실수로 인한 즉시 탈락 방지
- 매치 진입 스트레스 완화

---

### Match (15분)

본 게임 단계다.

- 깃발 활성화
- 캡처 경쟁 시작
- PvP / PvE 활성
- 파밍 지속 가능

**의도**
- 초반에 불리해도
- 중·후반 판단과 파밍으로 역전 가능

---

## 6️⃣ Flag Capture 시스템 (핵심 콘텐츠)

깃발은 **즉시 획득되지 않는다.**

### 캡처 규칙
- 캡처 존 안에서 **E 키 3초 유지**
- 진행 타이머는 **서버에서만 관리**

즉시 취소 조건:
- E Release
- Zone 이탈
- 사망

> 피격만으로는 취소되지 않는다.  
> “캡처를 포기할지 버틸지”를 플레이어가 선택하도록 하기 위함이다.

### 캡처 성공 시 서버 처리
- PlayerState(SSOT)의 Captures 증가
- GameState Announcement 4초 표시
- 현재 깃발 비활성화
- 6개 Spot 중 랜덤 이동

---

## 7️⃣ Flag Capture UI 구조 (선택 B)

캡처 UI는 **HUD에서 분리된 전용 위젯**이다.

### 구조
- `WBP_CaptureProgress`
  - Parent: `UMosesCaptureProgressWidget`
  - 진행률 / 퍼센트 / 경고 표시
- `WBP_MatchHUD`
  - 배치만 담당
  - 로직 없음

### UX 요소
- 3초 ProgressBar 증가
- 2.5초부터 경고 색상
- 사운드 점진적 증가

---

## 8️⃣ 무기 & 전투 시스템

### 무기 슬롯
| 슬롯 | 무기 |
|----|----|
| 1 | 라이플 |
| 2 | 샷건 |
| 3 | 스나이퍼 |
| 4 | 유탄 발사기 |

### 기본 지급
- MatchLevel 진입 시
- 서버가 Rifle + Ammo 30 / 90 지급

### 전투 규칙
- 탄약 무기별 분리
- 서버에서만 탄약 소비 확정
- HUD는 RepNotify → Delegate 기반

### 스왑 연출
- 서버 승인 후 스왑
- UpperBody 몽타주
- AnimNotify(Detach / Attach)
- 손 1 + 등 3 무기 노출

---

## 9️⃣ 스나이퍼 & 유탄 (전술 요소)

### 스나이퍼
- 우클릭 스코프 UI (로컬)
- 이동 시 블러 증가
- 판정은 서버

### 유탄 발사기
- 서버 Projectile 스폰
- 충돌 시 폭발
- GAS RadialDamage 적용

→ 캡처 존 견제 핵심 수단

---

## 🔟 HUD 시스템 (이벤트 기반)

HUD는 **Tick 없이 이벤트로만** 동작한다.

- Tick ❌
- UMG Binding ❌
- RepNotify → Delegate → Widget Update

표시 항목:
- RemainingTime
- Captures / Deaths / ZombieKills
- 무기 / 슬롯 / 탄약
- 중앙 Announcement

---

## 🧟 1️⃣1️⃣ 좀비 시스템 개요

좀비는 **킬 대상이 아닌 전장 압박 요소**다.

- Flag Spot 주변 배치
- Flag 이동 시 이전 Spot 좀비 리스폰
- 항상 동일한 긴장도 유지

---

## 🧟‍♂️ 1️⃣2️⃣ 좀비 공격 시스템 (Zombie Combat)

좀비 공격은 **서버 권위 기반 근접 전투 시스템**이다.

### 공격 흐름
1. 서버에서 Spawn
2. ZombieAIController + AIPerception으로 감지
3. BehaviorTree 추적
4. 서버에서 공격 시작
5. 공격 애니 Multicast
6. NotifyState 구간에서만 히트박스 활성

### Attack Window 설계
- 정확한 타격 프레임에만 판정
- 근접 지속 타격 문제 제거

### 히트박스
- 손 소켓에 부착
- 손 애니메이션과 함께 이동
- Collision은 윈도우 동안만 ON
- TSet으로 1윈도우 1타격 제한

### 데미지
- GAS SetByCaller Damage GE
- 서버에서만 적용

### 결과
- ZombieKills 증가
- KillFeed / Headshot Announcement
- HEADSHOT 2초 표시 (Tick 없음)

---

## 🧟 1️⃣3️⃣ 좀비 리스폰 & 전장 압박

- Flag Spot 단위 관리
- 깃발 이동 시 이전 Spot 좀비 리스폰
- 안전 지대 제거
- 항상 위험 유지

---

## 1️⃣4️⃣ Result & Persistence (PHASE 3)

- 서버가 승자 판정
- WinnerOnly 기록 저장
- JSON 파일 서버 저장
- 서버 재시작 후에도 로비 조회 가능

---

## 1️⃣5️⃣ 대규모 인원 대응 (PHASE 2)

- Tier(Significance) 기반 정책
- Render / Anim / Network / VFX / Audio 분리
- Tick 제거 + Timer 기반

---

## 1️⃣6️⃣ Evidence-First 설계 철학

모든 시스템은 **로그와 증거**로 검증된다.

- Capture Success / Cancel
- Fire 승인
- GAS Apply
- Save / Load

---

## 🔚 최종 요약 문장

> 이 포트폴리오는  
> 서버 권위와 단일 진실을 기반으로,  
> 좀비·플레이어·깃발이 동시에 상호작용하는 전장을  
> 이벤트 중심 HUD와 완결된 세션 구조로 구현하고,  
> 그 모든 과정을 **코드와 증거로 설명하는 UE5 Dedicated Server 포트폴리오다.**
