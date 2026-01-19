# 🎮 Game Portfolio — 핵심 기술 요소 요약

---

## 1. 서버 권위(Server-Authoritative) 멀티플레이 구조

* **Dedicated Server 기반**
* 모든 게임 판정은 서버 단일 결정

  * Hit / Headshot
  * Damage 계산
  * Ammo 소비
  * Pickup 성공 여부
  * Score / Kill / Death
  * Respawn 위치
  * Match 종료 시점

### 클라이언트 역할

* 입력(Input) 처리
* Server RPC 요청
* Replication 결과 표시

---

## 2. Single Source of Truth (SSOT)

* **PlayerState가 게임 상태의 단일 진실**

  * Combat 상태
  * Ammo / WeaponSlot
  * Score / Kill / Death
* 상태 데이터는 Pawn에 존재하지 않음
* Pawn 교체 / 사망과 무관하게 상태 유지

---

## 3. Pawn / PlayerState 책임 분리

### Pawn (Body)

* 이동
* 입력
* 상호작용 요청

### PlayerState (Soul)

* 전투 로직 소유
* 서버 권위 데이터 관리
* CombatComponent 보유

---

## 4. CombatComponent 중심 전투 설계

* CombatComponent는 **PlayerState에만 존재**
* 책임

  * 전투 요청 검증
  * Ammo 감소
  * Damage 계산
  * Score 반영
  * RepNotify 발생
* 클라이언트에는 전투 판정 로직 없음

---

## 5. 경쟁 Pickup 원자성 (Atomicity)

* 동시 Interact 요청 처리
* 서버에서 원자적 Lock
* **성공 1명 / 실패 N명 보장**
* 중복 획득 구조적으로 차단

---

## 6. UI 이벤트 기반 구조 (Tick 제거)

* HUD Tick 사용 ❌
* RepNotify → Delegate → Widget Update
* UI는 상태를 폴링하지 않고 **이벤트로 갱신**

---

## 7. Lyra 기반 아키텍처 채택

* PlayerState 중심 설계
* **Experience 기반 게임 규칙 주입**
* **GameFeature 기반 기능 모듈화**
* PawnData / Input / HUD / Ability 구성 분리
* 맵은 규칙을 모르고, Experience가 게임을 정의

---

## 8. Dedicated Server 환경 전제

* Listen Server 미사용
* 서버 / 클라이언트 프로세스 분리

### 서버 전용 책임

* GameMode
* Match Timer
* Spawn 결정
* 저장 처리

---

## 9. GAS 확장 가능 구조

* Owner Actor = PlayerState
* Avatar Actor = Pawn
* AbilitySystemComponent 지속성 확보
* Pawn 교체 시에도 Ability / Attribute 유지
* Attribute 변경 → Replication → UI 이벤트 반영

---

## 10. 중앙 정책 기반 성능 제어

* Actor 개별 최적화 ❌
* **중앙 Perf Policy Manager**
* Tier (역할 기반 중요도)
* Significance (거리 / 가시성 기반 중요도)
* PerfMode (품질 ↔ 성능 스위치)

### 제어 대상

* **Render**: LOD / Shadow / Cull / Cosmetic
* **Animation**: URO / Visibility Tick
* **GameThread**: Tick 제어
* **Network**: NetUpdateFrequency / Relevancy / Dormancy
* **VFX / Audio**: 중요도 기반 재생 제한

---

## 11. 서버 권위 영속성 (Persistence)

* 서버만 저장 권한 보유
* 클라이언트 저장 ❌
* Match 종료 시 서버가 결과 확정
* PlayerState → Record 단방향 변환
* 로그인 ID 기준 기록 조회
* Record는 읽기 전용으로 복제

---

## 12. 실시간 상태 vs 영구 데이터 분리

### PlayerState

* 실시간 상태
* 매치 동안만 유효
* Replication 중심

### Record

* 확정된 과거 데이터
* 서버 영속 저장
* 재접속 시 사용

---

## 13. 구조 설명 가능성 (포트폴리오 포인트)

* 모든 흐름을 도식으로 설명 가능
* “왜 서버 권위인가”를 코드 구조로 증명
* 전투 / 최적화 / 영속성 확장에도 구조 붕괴 없음
* 로그 기반 판정 추적 가능

---

## 요약

> **Dedicated Server 기반으로
> PlayerState 단일 진실 구조를 중심에 두고,
> 서버 권위 전투 · 중앙 정책 최적화 · 서버 영속 저장을
> 하나의 일관된 아키텍처로 구현한 UE5 멀티플레이 포트폴리오**
