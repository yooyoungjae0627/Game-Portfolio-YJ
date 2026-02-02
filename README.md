# 🎯 UE5 Dedicated Server FFA Flag Capture  
## 포트폴리오 카테고리별 소개 설명 스크립트 (DETAILED · MD)

> 이 문서는 **내 포트폴리오 전체를 타인에게 설명하기 위한 공식 스크립트**다.  
> 면접, README, 기술 발표, 코드 리뷰, 영상 내레이션 어디에 써도 되도록  
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

을 **코드, 로그, 동작 증거**로 설명하고 증명하는 데 있다.

---

## 2️⃣ 핵심 게임 목표 (Core Gameplay Goal)

> **“정해진 시간 동안 깃발을 더 많이 캡처한 플레이어가 승리한다.”**

이 게임은 킬 수 중심의 데스매치가 아니다.

- PvP는 **목표 달성을 방해하기 위한 수단**
- 좀비는 **전장을 복잡하게 만드는 환경 압박 요소**
- 플레이어는 **위험을 감수하고 목표를 수행할지**,  
  혹은 **상대를 방해할지**를 끊임없이 선택해야 한다.

이로 인해 플레이는 항상:
- 리스크 판단
- 위치 선정
- 타이밍 싸움

을 요구하도록 설계되었다.

---

## 3️⃣ 네트워크 & 권한 설계 (Server Authority / SSOT)

이 프로젝트는 **Server Authority 100%**를 전제로 설계되었다.

### 서버와 클라이언트의 역할 분리

- **클라이언트**
  - 입력(의도)만 서버로 전달
  - 서버 승인 결과를 시각적으로 표현

- **서버**
  - 모든 판정 수행
  - 상태 변경 확정
  - 점수 계산 및 기록 저장

### 단일 진실(SSOT)

모든 핵심 게임 상태는 **PlayerState**에만 존재한다.

- HP / Armor
- Weapon / Ammo
- Captures / PvPKills / ZombieKills / Headshots

Pawn / Character는:
- 애니메이션
- 이펙트
- 사운드  
같은 **코스메틱 전용**이다.

이 구조 덕분에:
- Late Join에서도 상태가 정확히 복원되고
- 클라이언트 조작/치트 가능성이 최소화된다.

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
- 플레이어가 ID 입력
- 서버가 검증 후 수용
- 서버가 LobbyLevel로 ServerTravel

### LobbyLevel
- 방 생성 / 입장
- 채팅
- Ready 체크
- Host만 Game Start 가능

### MatchLevel
- 서버가 Phase를 관리
- Warmup → Match → Result 자동 전환

---

## 5️⃣ Warmup / Match 단계 설계

### Warmup (30초)

Warmup은 **경쟁 전 준비 단계**다.

- 이동 가능
- 사격 테스트 가능
- 무기 / 탄약 / 수류탄 파밍 가능
- 점수 집계 ❌
- Late Join 허용

의도:
- 초반 실수로 인한 즉시 탈락 방지
- 매치 진입 스트레스 완화

---

### Match (15분)

본 게임 단계다.

- 깃발 활성화
- 캡처 경쟁 시작
- PvP / PvE 활성
- 파밍 계속 가능

의도:
- 초반에 불리해도
- 중·후반 파밍과 판단으로 역전 가능

---

## 6️⃣ Flag Capture 시스템 (핵심 콘텐츠)

깃발은 **즉시 획득되지 않는다**.

### 캡처 규칙

- 캡처 존 안에서 **E 키 3초 유지**
- 진행 타이머는 **서버에서만 관리**

즉시 취소 조건:
- E Release
- Zone 이탈
- 사망

> 피격만으로는 취소되지 않는다.  
> “캡처를 포기할지 버틸지”를 플레이어가 선택하도록 하기 위함이다.

---

### 캡처 성공 시 서버 처리

- PlayerState(SSOT)의 **Captures 증가**
- GameState의 **Announcement 4초 표시**
- 현재 깃발 비활성화
- 6개 Spot 중 랜덤 위치로 이동

---

## 7️⃣ Flag Capture UI 구조 (선택 B)

캡처 UI는 **HUD에서 분리된 전용 위젯**으로 설계했다.

### 구조

- `WBP_CaptureProgress`
  - Parent: `UMosesCaptureProgressWidget`
  - 진행률 / 퍼센트 / 경고 표시
- `WBP_MatchHUD`
  - 배치만 담당
  - 로직 없음

### UX 요소

- 3초 동안 ProgressBar 증가
- 2.5초부터 빨간 경고 표시
- 사운드 점점 커짐

장점:
- HUD 복잡도 감소
- 캡처 로직과 UI 결합 방지
- 유지보수 용이

---

## 8️⃣ 무기 & 전투 시스템

### 무기 슬롯 구조

무기는 **4종 고정**, 슬롯도 고정이다.

| 슬롯 | 무기 |
|----|----|
| 1 | 라이플 (자동) |
| 2 | 샷건 (클릭) |
| 3 | 스나이퍼 (클릭 + 스코프) |
| 4 | 유탄 발사기 |

### 기본 지급

- MatchLevel 진입 시
- 서버가 **Rifle + Ammo 30 / 90** 지급

---

### 전투 규칙

- 모든 탄약은 무기별로 분리
- 서버가 탄약 소비 확정
- HUD는 RepNotify → Delegate로만 갱신

### 스왑 연출

- 서버 승인 후에만 스왑
- UpperBody 몽타주 사용
- AnimNotify(Detach / Attach)로 손/등 무기 교체
- 손 1 + 등 3 무기 상시 노출

---

## 9️⃣ 스나이퍼 & 유탄 (전술 요소)

### 스나이퍼
- 우클릭 시 스코프 UI (로컬 연출)
- 이동 시 블러 증가
- 판정은 서버에서 그대로 수행

### 유탄 발사기
- 서버가 Projectile 스폰
- 충돌 시 폭발
- GAS RadialDamage 적용

→ 캡처 존 견제 핵심 수단

---

## 🔟 HUD 시스템 (이벤트 기반)

HUD는 **Tick 없이 이벤트로만** 동작한다.

### 원칙
- Tick ❌
- UMG Binding ❌
- RepNotify → Native Delegate → Widget Update

### 표시 항목
- RemainingTime (서버 타이머)
- Captures / Deaths / ZombieKills
- 현재 무기 / 슬롯 / 탄약
- 중앙 Announcement

---

## 1️⃣1️⃣ 좀비 시스템

좀비는 **킬 대상이 아니라 압박 요소**다.

- 깃발 Spot 주변 수동 배치
- 깃발 이동 시 이전 Spot 좀비 리스폰
- 항상 동일한 긴장도 유지

---

## 1️⃣2️⃣ Result & Persistence (PHASE 3)

Result 단계에서:

- 서버가 승자 판정
- **WinnerOnly 기록 저장**
- JSON 파일로 서버에 저장
- 서버 재시작 후에도 Lobby에서 조회 가능

SaveGame을 사용하지 않고:
- 서버 파일 접근만 허용
- 성공/실패 로그로 증명

---

## 1️⃣3️⃣ 대규모 인원 대응 (PHASE 2)

엔진 수정 없이:
- Tier(Significance) 기반 정책 적용
- Render / Anim / Network / VFX / Audio 분리 제어
- Tick 제거 + Timer 기반 계산

결과:
- 캐릭터 수 증가에도
- 성능 안정성 유지

---

## 1️⃣4️⃣ Evidence-First 설계 철학

이 프로젝트의 모든 주장은  
**로그와 수치로 증명**된다.

예:
- Capture Success / Cancel
- Fire 승인
- GAS Apply
- Save / Load

---

## 🔚 최종 요약 문장

> 이 포트폴리오는  
> 서버 권위와 단일 진실을 기반으로,  
> 이벤트 중심 HUD와 완결된 멀티플레이 흐름을  
> 실제 코드와 증거로 설명하는 UE5 포트폴리오다.