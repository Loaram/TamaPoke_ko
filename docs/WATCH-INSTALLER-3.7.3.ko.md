## Windows 워치 설치 도우미 추가 (시험판)

명령어 없이 PC 등록(페어링) → 연결 확인 → 설치 / 업데이트 → 게임 열기를 진행하는 한국어 도우미를 추가했습니다. **게임은 정식 3.7.3 그대로이며 도우미만 시험판**입니다.

- [Windows 설치 도우미 ZIP](https://github.com/Loaram/TamaPoke_ko/releases/download/3.7.3/TamaPoke-3.7.3-Watch-Installer-Windows.zip): 정식 Wear APK, ADB, 7쪽 PDF를 동봉했습니다. Windows 10/11 x64에서 모두 압축을 푼 뒤 `TamaPoke-Watch-Installer.exe`를 실행하세요.
- [설치 도우미 PDF](https://github.com/Loaram/TamaPoke_ko/releases/download/3.7.3/TamaPoke-3.7.3-Watch-Installer-Guide-KO.pdf): 워치 설정, 두 포트 구분, 실제 도우미 화면, 설치·업데이트, 세이브 보호, 오류 해결을 설명합니다.
- [추가 파일 SHA-256](https://github.com/Loaram/TamaPoke_ko/releases/download/3.7.3/WATCH-INSTALLER-SHA256SUMS.txt): 이번 ZIP·PDF 전용 검사값입니다. 전체 배포 파일 검사값은 `SHA256SUMS.txt`에도 들어 있습니다.

워치 설정의 개발자 옵션·무선 디버깅은 직접 켜야 합니다. 기존 앱 위에 설치하며 앱 삭제·데이터 초기화·강제 다운그레이드·자동 백업은 하지 않습니다. 업데이트 전 세이브를 백업하고 미완료 한 마리 전송·교환은 같은 상대와 먼저 완료하세요. 전체 세이브를 다른 기기에 적용하면 대상의 기존 저장을 덮어씁니다.

자동 검사 30개, 숨긴 실제 Windows 창의 화면 검사, PDF 7쪽, ZIP CRC와 정식 APK 동일성 검사를 통과했습니다. **실제 워치 페어링·신규 설치·세이브 유지 업데이트는 아직 미검증**입니다. Galaxy Watch4-9용 공용 APK를 동봉하지만 모든 기기의 실기 검증을 뜻하지 않습니다. EXE는 코드 서명되지 않았으며 Windows 보안 차단 시 보안 기능을 끄지 말고 배포자에게 문의하세요. 기존 수동 워치 설치 가이드도 그대로 제공합니다.
