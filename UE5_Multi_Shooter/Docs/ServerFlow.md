# Day1 — Server Boot & Lobby 진입 기준 고정

## 정책
- 단일 레벨(맵 이동 없음). 상태 전환은 UI/Phase로만 수행.

## 서버 부트 기준선(DoD 로그)
서버 단독 실행 시 아래 순서가 항상 1회 출력되어야 한다.

1) [AUTH] Server Boot
2) [EXP] Experience Selected
3) [ROOM] Lobby Initialized
4) [PHASE] Current = Lobby

## 실제 흐름
- GameInstance 생성 후 World 초기화 시점에 서버 부트 로그([AUTH])를 1회 고정
- GameMode InitGame → 옵션/예약 로직으로 Experience 선택([EXP])을 1회 고정
- ExperienceManager READY 이후에만 Lobby 초기화([ROOM]) 및 Phase Lobby 확정([PHASE]) 수행
