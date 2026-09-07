# TamaPoke 3.5.3

3.5.2 후속 시뮬레이션에서 발견한 문제를 수정한 정식 버그 패치입니다. 새 기능이나 밸런스 변경이 아니므로 패치 번호를 올렸습니다.

## 수정 사항

- 구버전의 미완료 교체 기록과 시간대 변경이 겹칠 때 최신 훈련값이 과거 값으로 되돌아갈 수 있던 문제를 수정했습니다. 저장 당시 날짜를 기준으로 온전한 최신 기록을 확인합니다.
- 교체 저장 복구 대기 중에는 오프라인 성장과 앱 종료·복귀 저장도 멈춥니다. 재시도 성공 시점부터 시간을 다시 계산해 대기 시간이 한꺼번에 적용되지 않게 했습니다.
- 파티 놓아주기 확인창을 스와이프로 닫은 뒤 다른 포켓몬을 선택하면 이전 확인 상태가 남던 문제를 수정했습니다.
- 저장할 수 없는 상태에서는 탐색을 시작하지 않고 안내합니다. 차단된 탐색은 활력을 소모하지 않습니다.
- 트레이너 이름 키보드의 첫 입력 누락과 키보드 뒤 화면의 스와이프 반응을 수정했습니다.

## 검증과 업데이트

수정본의 전체 회귀 검사 58개, 저장 중단 1,056건, 교체·훈련·재시작 1,200회, 통신 장애 조합 90건, SD 장애 검사 10개를 통과했습니다. Android·Wear OS·ESP32·PC 빌드를 확인했습니다. 실제 기기 설치·무선·물리적 전원 차단 시험은 별도이며 모든 실기 상황의 무결함을 보장하지 않습니다.

업데이트 전 세이브를 백업하고 미완료 전송·교환은 같은 상대와 먼저 완료하세요. **앱을 삭제하지 말고 기존 앱 위에 설치하세요.** ESP는 기존 SD를 유지하고 **Erase device를 선택하지 마세요.** 저장 v7, 개체 72바이트, 배틀 규격 5와 거래 규격 1은 유지합니다. Android versionCode 3034 / Wear OS 3035입니다. 이미 사라진 훈련값은 추측해 복원하지 않습니다.

## 함께 제공하는 가이드와 설치 도구

- [28쪽 플레이 가이드](https://github.com/Loaram/TamaPoke_ko/releases/download/3.5.3/TamaPoke-3.5.3-Play-Guide-KO.pdf): 저장 확인 안내는 28쪽입니다.
- [Galaxy Watch4-9 수동 설치 가이드](https://github.com/Loaram/TamaPoke_ko/releases/download/3.5.3/TamaPoke-3.5.3-Galaxy-Watch4-9-Install-Guide-KO.pdf): 기존 원본을 그대로 동봉했습니다.
- [Windows 워치 설치 도우미](https://github.com/Loaram/TamaPoke_ko/releases/download/3.5.3/TamaPoke-3.5.3-Watch-Installer-Windows.zip): 이번 정식 Wear APK와 ADB 및 설명서를 동봉했습니다. 도우미는 시험판입니다.
- [7쪽 설치 도우미 설명서](https://github.com/Loaram/TamaPoke_ko/releases/download/3.5.3/TamaPoke-3.5.3-Watch-Installer-Guide-KO.pdf).

APK는 휴대전화용과 Wear OS용을 구분해서 받으세요. ESP 실기용 펌웨어는 ZIP 또는 [웹 설치 페이지](https://loaram.github.io/TamaPoke_ko/)에서 제공합니다. 모든 배포 파일의 검사값은 SHA256SUMS.txt에 있습니다.
