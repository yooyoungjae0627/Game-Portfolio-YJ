# 🎯 UE5 Dedicated Server FFA Flag Capture  
## 포트폴리오 최종 README

**Lyra Architecture 기반 (Experience / GameFeature / GAS)**  
**Server Authority 100% / SSOT / Evidence-First 설계**

---

## 📌 1. 프로젝트 개요

본 프로젝트는 **UE5 Dedicated Server** 환경에서 동작하는  
**개인전(Free-for-All) 멀티플레이 게임**이다.

이 프로젝트의 목적은 단순히  
“멀티플레이가 된다”를 보여주는 것이 아니라,

- 서버 권위(Server Authority)를 끝까지 지키는 구조
- 단일 진실(SSOT) 기반 상태 관리
- Lyra 스타일 아키텍처(Experience / GameFeature / GAS)의 실제 적용
- 시작 → 경쟁 → 종료 → 기록 → 재접속까지 이어지는 완결된 게임 흐름

을 **코드 · 로그 · 증거**로 증명하는 것이다.

---

## 🎯 2. 핵심 목표 한 줄

**“30분 동안 깃발을 더 많이 캡처한 플레이어가 승리한다.”**

- PvP는 목적이 아니라 **캡처를 방해하기 위한 수단**
- 좀비는 전투를 복잡하게 만드는 **환경 압박 요소**

---

## 👥 3. 게임 인원 및 기본 규칙

- 게임 모드: **개인전(Free-for-All)**
- 팀 없음 (모든 플레이어는 서로 적)
- 최소 인원: **2명**
- 최대 인원: **4명**
- 서버: **Dedicated Server 전용**

모든 전투 판정, 점수 계산, 기록 저장은  
**서버에서만 수행**된다.

---

## 🧭 4. 전체 게임 흐름 요약

StartGameLevel  
→ LobbyLevel  
→ 방 생성 / 방 입장  
→ 캐릭터 선택  
→ Ready 체크  
→ MatchLevel  
→ Warmup (60초)  
→ Match (30분)  
→ Result  
→ 기록 저장  
→ LobbyLevel (기록 조회)

이 전체 흐름이 **하나의 게임 세션**으로 완결된다.

---

## 🏁 5. StartGameLevel → LobbyLevel

### 5.1 StartGameLevel

- 플레이어는 게임 실행 후 **ID를 입력**
- 서버는 해당 ID를 검증 및 수용
- 서버가 **LobbyLevel로 ServerTravel** 수행

이 시점부터:

- 플레이어는 서버가 관리하는 존재가 되며
- ID는 PlayerState의 식별자로 사용된다

---

## 🏠 6. Lobby 시스템 (이미 구현됨)

LobbyLevel은 **매치 진입 전 준비 단계**를 담당한다.

### 6.1 Lobby에서 가능한 기능

- 캐릭터 선택
- 방 생성
- 방 목록 표시
- 방 입장
- 채팅
- Ready 체크
- 게임 시작 (Host 전용)

---

### 6.2 방 생성 (Host)

Host는 방을 생성할 수 있다.

설정 항목:
- 방 제목
- 참여 인원 수 (**최소 2 / 최대 4**)

방 생성 시:
- 서버가 방 정보를 관리
- 다른 클라이언트의 방 목록(ListView)에 즉시 반영

---

### 6.3 방 입장 & 채팅

- 클라이언트는 방 목록의 Entry를 클릭하여 입장
- 입장 후:
  - 방 채팅 가능
  - Ready 상태 확인 가능

채팅 구조:
- 클라이언트 입력
- 서버 확정
- 서버가 모든 클라이언트에 복제

클라이언트 간 직접 통신은 존재하지 않는다.

---

### 6.4 Ready 시스템 & 게임 시작 조건

Ready 규칙:
- Host를 제외한 모든 플레이어가 Ready 체크
- 방에 설정된 참여 인원 수가 충족

조건 만족 시:
- **Host의 Game Start 버튼 활성화**

---

### 6.5 MatchLevel 이동

- Host가 Game Start 버튼 클릭
- 서버가 모든 플레이어를 **MatchLevel로 ServerTravel**

Lobby UI 종료 → 매치 전용 흐름 시작

---

## ⏱️ 7. MatchLevel 구조

MatchLevel은 두 단계로 구성된다.

### 7.1 Warmup (60초)

Warmup은 **준비 단계**다.

Warmup 동안 가능한 행동:
- 이동
- 사격 테스트
- 무기 / 탄약 / 수류탄 파밍

특징:
- 점수 / 캡처 없음
- 경쟁 시작 전 구간
- **Late Join 허용 (즉시 스폰)**

---

### 7.2 Match (30분)

Warmup 종료 후 **본 게임 시작**

Match 동안:
- 깃발 활성화
- 캡처 경쟁
- PvP 허용
- 좀비 등장
- **파밍 계속 가능**

의도:
- 초반 불리해도 중·후반 파밍으로 역전 가능

---

## 🚩 8. 핵심 목표: 깃발 캡처

승리 조건은 킬 수가 아니다.

**깃발 캡처 성공 횟수**

### 8.1 캡처 규칙 (3초 유지)

- 깃발은 즉시 획득되지 않는다
- **캡처 존 안에서 3초 유지**해야 성공

즉시 취소 조건:
- 피격
- 사망
- 캡처 존 이탈

PvP를 단순 킬 게임이 아니라  
**캡처 방해 수단**으로 만들기 위한 설계다.

---

## 📍 9. 깃발 이동 시스템 (6 Spot 구조)

- 맵에 **총 6개의 깃발 스팟**
- 동시에 활성화되는 깃발은 **1개**
- 캡처 성공 시:
  - 현재 스팟 비활성화
  - **6개 중 하나로 랜덤 이동**

효과:
- 전투 위치 고정 방지
- 맵 전체 이동 유도
- 지속적인 충돌 발생

---

## 🧟 10. 좀비 시스템 (수동 스폰 + 리스폰)

### 10.1 좀비 배치 방식

- 웨이브 테이블 방식 ❌
- 각 깃발 스팟 주변에 **좀비 스폰 스팟을 수동 배치**

깃발이 활성화된 스팟에서만:
- 해당 스팟의 좀비 등장
- 캡처 지역에 압박 제공

---

### 10.2 깃발 이동 시 좀비 리스폰

- 깃발이 다른 스팟으로 이동하면
- **이전 스팟에서 죽었던 좀비는 다시 리스폰**

의도:
- 항상 동일한 난이도 유지
- 플레이어가 정리한 지역도 다시 위험 지역으로 전환

---

## 🔫 11. 무기 & 파밍 시스템

### 11.1 무기 슬롯 구조

- 슬롯: **1 / 2 / 3**
- 키보드 1, 2, 3으로 서버 승인 스왑

### 11.2 슬롯별 무기

**1번 슬롯: 라이플 (3종)**  
- 서로 다른 데미지 / 발사 딜레이 / 탄 수 / 피격 파티클  
- DataAsset 기반  
- 반드시 **버리기(Drop) 후 파밍**

**2번 슬롯: 권총**  
- 보조무기  
- 전용 애니메이션 모드  

**3번 슬롯: 수류탄**  
- 투척 애니메이션  
- 스플래시 데미지  
- 캡처 방해 핵심 수단  

---

## 🖥️ 12. HUD 시스템

- Tick ❌
- UMG Binding ❌
- **RepNotify → Delegate → Widget Update**

필수 HUD:
- RemainingTime
- MyCaptures / BestCaptureTime
- CurrentLeader

---

## 🏁 13. Result 단계 & Persistence

### 13.1 Result 진입

- 30분 종료 시 서버 Result 전환
- 모든 입력 서버 Reject
- 클라이언트는 결과 표시만 수행

### 13.2 승자 판정

1. Captures  
2. BestCaptureTime  
3. PvPKills  
4. ZombieKills  

### 13.3 WinnerOnly 기록 저장

- 서버만 저장
- 승자 1명만
- 서버 재시작 후에도 유지

### 13.4 로비 기록 조회

- LobbyLevel에서 기록 보기
- ListView로 표시
- 재접속 유지

---

## 🧠 14. 기술적 설계 원칙

- Server Authority 100%
- SSOT = PlayerState
- Experience 분리
- GameFeature 모듈화
- GAS Ability 통합
- 로그 기반 증명

---

# 🟦 PHASE 2 — 대규모 인원 대응 최적화 시스템

## PHASE 2 목표

**“엔진을 고치지 않고,  
언리얼 엔진 공식 기능만으로  
대규모 인원 상황을 안정적으로 처리한다.”**

본 Phase는 그래픽 품질을 낮추는 것이 목적이 아니라,  
**동시에 많은 캐릭터가 등장하는 상황에서도 시스템이 무너지지 않도록  
클라이언트 비용을 구조적으로 제어할 수 있음을 증명**하는 단계다.

---

## PHASE 2 핵심 개념

- Tier(Significance) 기반 캐릭터 중요도 분류
- CPU / GPU / Network / Background 비용을 분리해서 관리
- 개별 최적화가 아닌 **정책(Policy) 중심 최적화**
- 모든 처리는 엔진 공식 기능만 사용

---

## Tier 정의

| Tier | 거리 |
|------|------|
| Near | 0 ~ 10m |
| Mid  | 10 ~ 25m |
| Far  | 25m 이상 |

Tier 산정 정책:

- 로컬 카메라 기준 거리
- **Tick 사용 ❌**
- Timer 기반 계산 (0.2 ~ 0.5초)
- 히스테리시스 적용으로 경계 떨림 방지
- Tier 변경 시에만 로그 출력

---

## Tier 기반 최적화 정책 요약

### Render / GPU

- Tier에 따라 LOD 강제 적용
- Far Tier에서 Shadow 비활성 또는 Shadow Distance 축소
- Cosmetic Mesh(Tag 기반) Hidden 처리  
  (Destroy ❌, 재등장 비용 방지)

효과:
- DrawCall 감소
- Primitive 수 감소
- GPU 비용 절감

---

### Animation / CPU

- Visibility Based Anim Tick 적용
- Update Rate Optimization(URO) 적용

| Tier | Animation Update |
|------|------------------|
| Near | 매 프레임 |
| Mid  | 2 ~ 3 프레임당 1회 |
| Far  | 6 ~ 10 프레임당 1회 |

효과:
- Anim Update / Evaluate 비용 감소
- Animation Thread 안정화

---

### GameThread / Component Tick

대상 예시:
- Footstep
- IK 보정
- 상태 폴링
- UI 갱신
- 지속형 사운드 / VFX 컴포넌트

정책:
- Far Tier: Tick 비활성 또는 Interval 증가
- Mid Tier: Tick Interval 완화
- 폴링 로직 제거 → Delegate / Event 전환

효과:
- 캐릭터 수 증가에 따른 GameThread 병목 제거

---

### Network

- Tier 기반 NetUpdateFrequency 적용
  - Near: 높음
  - Mid: 중간
  - Far: 낮음
- Far Tier 캐릭터:
  - Relevancy 거리 축소
  - Idle 상태에서 Dormancy 적용
  - 상태 변경 시 Wake-up

효과:
- Replication Payload 감소
- 대규모 인원 상황에서도 네트워크 안정성 확보

---

### VFX / Audio

- Niagara / Audio에 Significance 개념 적용
- Far Tier:
  - Effect Deactivate 또는 Scalability 하향
- Mid Tier:
  - Spawn Rate 감소

효과:
- 대규모 인원에서 GPU / CPU 동시 폭주 방지

---

## 중앙 정책 구조 (핵심 설계)

모든 최적화는 **단 하나의 진입점**에서만 적용된다.

ApplyPolicyForMode(Mode)

적용 흐름:
1. Reset (기본 상태 복구)
2. Tier 산정
3. Render / Anim / Net / World 정책 적용

이 구조의 장점:
- 정책 누락 방지
- 유지보수 용이
- 신규 캐릭터 / 신규 플랫폼 확장 대응 가능

---

## PHASE 2 결과물 & 증거

- stat unit / anim / net / rhi 전후 비교 캡처
- 동일 조건 재현 가능한 Benchmark 시나리오
- Rendering.md (정책 설명 문서)
- Benchmark.md (재현 규칙 문서)

---

# 🟦 PHASE 3 — 서버 권위 Persistence 시스템

PHASE 3는 **매치 결과를 서버에서만 확정·저장하고,  
서버 재시작 이후에도 로비에서 확인할 수 있는 구조**를 목표로 한다.

---

## PHASE 3 핵심 원칙

- SaveGame 사용 ❌
- 파일 접근은 서버만 가능
- 저장은 Result 진입 시 **단 1회**
- 클라이언트는 요청 및 표시만 수행
- 모든 성공/실패는 로그로 증명

---

## 저장 정책 요약

- 저장 주체: Dedicated Server ONLY
- 저장 포맷: JSON (가독성 및 증거 목적)
- 저장 위치: Saved/MatchRecords/
- 덮어쓰기 ❌
- 서버 재시작 후 Load 가능

---

## 저장 흐름

전투 종료  
→ Result 진입  
→ 서버 Snapshot 생성  
→ JSON 파일 저장  
→ 서버 재시작  
→ 로비 진입 시 Load  
→ PlayerState Rep  
→ Lobby ListView 표시

---

## PHASE 3 결과물 & 증거

- Result 진입 시 저장 로그
- 서버 재시작 후 Load 로그
- Lobby 기록 조회 UI 표시

---

## Evidence-First 원칙

모든 Phase는 **로그와 수치로 증명**된다.

대표 로그 예시:

[PHASE][SV] Match Start Remain=1800  
[FLAG][SV] Capture OK Player=A  
[ZOMBIE][SV] Respawn Dead Spot=3  
[PERSIST][SV] Save OK MatchId=...  
[PERSIST][SV] Load OK Records=12


## ✅ 최종 결론

이 포트폴리오는  
**게임 완성도 + 대규모 인원 대응 + 서버 권위 영속성**을  
모두 증명하는  
**5년차 UE 클라이언트 개발자 포트폴리오**다.
