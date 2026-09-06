# 자유 교체 베타 검증

대상: `codex/persistent-forms`의 **3.0.0-beta.3**. 폼체인지와 가라르/팔데아 체육관 변경을 유지합니다. 공개 업로드·실제 기기 설치는 하지 않았습니다.

- 실제 공통 코드를 사용하는 38개 테스트 묶음: `runtime-tests.log`.
- 최종 화면 변경 뒤 교체/놓아주기 재검증: `final-focused-tests.log`.
- 새 테스트는 파티와 박스의 실제 버튼, 파티 5칸+박스 60칸, 60번째 박스 칸, 개체별 돌봄 상태 보존, 재시작, TFR1 이전, v4 전체 세이브, 첫 저장 실패/중간 저장 실패/복구 반복을 확인합니다.
- PC, Android arm64/x86_64, Wear OS armv7/arm64, ESP32-S3를 빌드합니다. 서명/16KB 정렬 확인은 Android/Wear 빌드 로그에 포함됩니다.
- `party-detail.png`, `box-detail.png`: 실제 공통 렌더러의 466×466 결과를 눈으로 확인했습니다. 세 버튼과 현재 포켓몬 이동 안내가 겹치지 않습니다.
- 세이브 최대 9,216바이트, 레코드 72바이트. 저장 전송 시 큰 임시 버퍼는 스택이 아닌 힙에 배치합니다. 실제 ESP NVS 공간/전원 차단, 터치, Android↔Wear↔ESP 무선 전송은 아직 실기에서 확인하지 않았습니다.

산출물·소스 해시와 완료 상태는 `tools/record_active_swap_qa.py`가 모든 로그를 확인한 뒤 만드는 `build-results.json`에 기록합니다. 이전 `forms-runtime`/`gyms` 디렉터리의 바이너리는 beta.1/beta.2이며 이번 변경을 포함하지 않습니다.
