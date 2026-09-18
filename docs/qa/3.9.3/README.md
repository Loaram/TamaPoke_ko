# 3.9.3 배틀 후 저장 연결·오디오 종료 회귀 검사

## 원인과 재현

buildSquad의 `Pet tmp = pet`가 Preferences의 소유 handle까지 복사했다. ESP Arduino 3.3.8의 Preferences 소멸자는 end()를 호출하므로 임시 Pet 소멸 시 현재 Pet의 연결도 닫힌다. 원본 started는 true로 남아 Retry의 begin()도 연결을 다시 열지 못한다. 파티의 별도 연결과 교체 저널은 남으므로 포획 개체는 보관되고 재시작하면 교체가 복구될 수 있다.

기존 호스트 Preferences의 end()는 빈 함수라 문제를 놓쳤다. TAMAPOKE_TEST_NVS_HANDLES 옵션은 연결 생명주기·닫힌 연결의 읽기/쓰기 거부·읽기전용을 모사한다. 기존 코드에서 저장 검증 및 복귀 실패를 확인한 뒤 PartyMon 값 복사로 수정했다. Pet 복사·이동을 금지한다.

별도로 음악 종료 시 MUS_NONE 분기는 남은 음을 소모하지 않는데 busy()가 true면 큐를 대기0으로 반복했다. allOff()로 잔류 음을 정리해 다음 큐 대기를 보장한다. watchdog을 끄거나 제한시간을 늘리지 않는다.

## 검증 범위

- 전체 런타임 68종: 새 battle_storage 포함. 히드런/무한다이맥스, 코라이돈/루나아라의 탐색·포획·복귀·재로드를 실제 게임 함수로 실행.
- 실제 audio.cpp/gbsynth.cpp 4시나리오: 무음, 잔류 음, 소리 OFF, 재생 도중 음악 종료. 각각 유한 10,000회 큐 검사. FreeRTOS 실시간 스케줄링 전체를 재현한 것은 아님.
- 저장 중단·교체 복구·공용 활력·SD 장애, Android LAN 콜백, 웹 및 Windows 워치 설치 도우미 검사.
- APK 코드/서명/ABI/그림 팩, ESP 파티션/앱 범위, 배포 9자산 및 가이드 검증.
- 최종 커밋의 GitHub 검사 성공 후 main·태그·릴리스와 Pages의 공개 파일을 재확인.

완료 결과는 build-results.json과 동봉 로그를 기준으로 한다. 원형 ESP 수정 시험판에 대해 2026-09-18 사용자가 해결을 확인했다. Android/워치 모든 모델·물리적 SD 전원 차단·장기 내구성은 이번 확인 범위가 아니다. 저장 포맷·밸런스는 변경하지 않는다.
