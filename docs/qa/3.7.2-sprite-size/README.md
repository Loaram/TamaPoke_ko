# 3.7.2 크기 보정 전수검수 — 미배포

검수일: 2026-09-08. 사용자 승인 후 작은 종의 크기 차이 보존 및 전체 종 점검을 추가 반영했습니다. [표시 규칙](../../SPRITE-SIZING.ko.md).

## 범위와 결과

| 항목 | 결과 |
|---|---|
| 크기 데이터 | 전국 1,025종 + 지원 폼 176개 |
| 실제 그림 경로 | 기본 982종 + 폼 176개 × 일반/이로치 및 기존 대체 경로 = 2,316개 |
| 프레임 검사 | 112,549개, 포맷·팔레트 색 인덱스·프레임 길이·투명 영역·재생 시간 검증 통과 |
| 액션/화면 영역 조합 | 74,188건, 크기 상한·홈 상태문구 영역·좌우 산책 위치의 원형 화면 경계 검증 통과 |
| 비교 화면 검토 | 기본 그림 11장 + 폼 2장의 전종 시트, 비율/프레임 변화가 큰 26개 별도 검토 |
| 작은 종 확인 | 피츄 96, 피카츄 108, 리자몽/잠만보 168px 기준; 단독 대여르 108px 별도 보정 |
| 목록 확인 | 사진과 같은 나인테일/푸크린/라플레시아/파라섹트/가라르 나옹/골덕 구성, 나옹 기본·알로라·가라르 일반/이로치 확인 |
| 전체 런타임 중간 검사 | 64종 통과. 최종 경계·대여르 조정과 구분한 중간 결과 |
| 최종 관련 회귀 | bond, forms_ui, sprite, box_pages, box_direct, box_sort, active_swap, battle, release 9종 통과 |
| 최종 새 C++ sprite_layout 실행 | 빌드는 성공했으나 Windows WinError 4551 실행 정책 차단. **실행 통과로 간주하지 않음** |

초기 `sprite_layout` 검사는 실제 로더에서 18,547개 액션의 메타데이터와 모든 기본/폼/이로치 썸네일, 대표 7종의 Idle 전체 프레임을 검사해 통과했습니다. 이후 추가한 대여르 예외 및 원형 경계 검사를 포함하는 최종 새 실행 파일은 Windows에서 차단됐습니다. 전역 보안 설정은 변경하지 않았습니다. 최종 그림 데이터 검사는 별도 Python 경로로 모두 다시 실행했고, 최종 에뮬레이터에서 실제 홈·파티·박스 화면을 확인했습니다. 새 C++ 전용 검사는 차후 허용된 CI 환경에서 추가 실행해야 합니다.

기존 그림 미지원 43종은 검사에서 명시적으로 제외하며 새로 잠금을 해제하지 않습니다. 그림이 있는 982종에는 진화 계열 때문에 수집이 잠긴 15종이 포함되므로, 수집 가능 967종과 혼동하지 않습니다. 보관 중인 기존 개체의 그림도 검수 대상입니다.

## 빌드 및 남은 확인

- 최종 에뮬레이터 빌드와 사진 대응 화면 확인 완료: `build/size-preview/final/`.
- Android APK 빌드·서명·정렬 검사 완료: `build/android/TamaPoke-3.7.2-Android-Full-debug.apk`, versionCode 3044.
- Wear OS APK ARM32/ARM64 빌드·서명·정렬 검사 완료: `build/android/TamaPoke-3.7.2-WearOS-GalaxyWatch4-9-debug.apk`, versionCode 3045.
- ESP32 빌드 성공: 프로그램 영역 72%, 전역 메모리 66%(217,836바이트), 지역 변수 여유 109,844바이트. 앱 바이너리 2,282,576바이트. 기존 TouchDrv 헤더 사용 중단 예고 외 빌드 오류 없음. 출력 위치는 `build/3.7.2/esp-size/`이며 기존 유대 전용 `build/3.7.2/esp/`와 구별합니다.
- 실제 스마트폰·워치·ESP 기기 설치 및 프레임 속도 확인은 하지 않았습니다.
- 공개 GitHub/Pages/릴리스 업로드는 하지 않았습니다. 기존 준비용 웹 펌웨어와 PDF는 이번 크기 패치를 포함하지 않으므로 이후 배포 전에 갱신해야 합니다.

로컬 APK SHA-256:

- Android: `5463ea3c55c59a3633becef7f5191f54c91004b052cd5361af2d2bd83b80f6c7`
- Wear OS: `d2efb80abcb36867776e625b515b6ea3d2b32b414257c1e2e9fc2adc25f40aff`

## 재현 자료

`build/size-preview/`의 `final-geometry-audit.log`, `audit/audit.json`, `full-runtime.log`, `final-regressions.log`, `final-tests.log`, `android-build.log`, `wear-build.log`, `esp-build.log`를 구분해서 보관합니다. `home-final-comparison.png`, `box-final.png`, `forms-final.png`, `party-final.png`는 실제 에뮬레이터 프레임버퍼 화면입니다. 사용자 세이브를 불러오지 않은 촬영용 임시 구성입니다.

`tools/emu/main_sdl.cpp`의 `--shot size-home / size-box / size-forms / size-party`는 검수용 촬영 진입점이며 일반 플레이에는 테스트 개체를 생성하지 않습니다. 스프라이트 팩, 저장 v7, 개체 72바이트, 도감/폼/기술 번호, 교환 규격, 활력·확률은 그대로입니다. 보류한 유대 수정은 이번 로컬 준비본에 함께 포함되어 있습니다.
