# 워치 설치 도우미 3.5.0 시험판 검증 (2026-09-07)

- 다른 작업의 3.4.0용 도우미를 정식 3.5.0 Wear APK로 갱신했습니다. 게임 코드·세이브 형식·APK·게임 태그는 변경하지 않습니다.
- 설치 전 대상 주소·백업·미완료 전송/교환 확인창(기본 아니요), PDF 열기 버튼, 시작 시 파일 확인 전 ADB 동작 잠금을 추가했습니다.
- 빌드: `.venv/Scripts/python.exe tools/build_watch_installer.py --out build/watch-installer-350-final --game-version 3.5.0 --guide output/pdf/TamaPoke-3.5.0-Watch-Installer-Guide-KO.pdf --preview`
- 산출물: `build/watch-installer-350-final/TamaPoke-3.5.0-Watch-Installer-Windows.zip`, 18,006,643바이트. EXE·정식 APK·ADB와 의존 DLL·라이선스·7쪽 PDF를 동봉합니다.
- .NET Framework 컴파일 및 자동 검사 **30개 통과**: IP/포트·코드, 명령 삽입 거부, 코드의 선행 0·출력 비공개, 오류 안내, 프로세스 종료 코드·시간 초과, 손상·누락 DLL·중복/경로 이탈/빈 검사 목록 거부, GUI 생성·파일 검증 전 버튼 잠금·PDF 열기 버튼.
- 실제 Windows Form을 숨겨 렌더링한 화면에서 3.5.0·한국어·필드·버튼·기록 배치를 확인했습니다. 큰 창 예시를 PDF에 넣었으며 작은 화면은 스크롤을 사용합니다.
- PDF는 ReportLab으로 생성하고 7쪽 모두 Poppler PNG로 육안 확인했습니다. 글꼴·표·링크·페이지 번호, 워치 설정·포트 예시·세이브 보호·오류 안내를 검수했습니다.
- ZIP CRC·SHA-256 및 실제 배포 폴더의 런타임 파일 검증을 통과했습니다. 동봉 ADB 37.0.1의 버전 명령을 실행했습니다. 제한 실행 환경에서는 사용자 설정 폴더 접근 오류가 있어 일반 사용자 환경에서 다시 확인했습니다. 기기 연결은 하지 않았습니다.
- 동봉 Wear APK SHA-256: `e7ec3aa8bc462ac18e49634589e16eaeb0600199174534dae5167a6867d49f7d` (기존 공개 3.5.0 Wear 자산과 동일).
- 릴리스에 ZIP·별도 PDF·`WATCH-INSTALLER-SHA256SUMS.txt`만 추가합니다. 업로드 도구는 동봉 APK를 기존 공개 Wear 자산의 SHA와 비교하고, 동봉 PDF와 별도 PDF의 일치 및 기존 자산 보존을 검사합니다.
- 모든 기기 대상 명령은 사용자가 입력하고 연결 확인을 마친 주소에 `-s`를 지정합니다. Wear OS feature를 설치 직전 다시 확인합니다. 설치는 `install -r`만 사용하며 앱 삭제·데이터 초기화·강제 다운그레이드는 구현하지 않았습니다.
- 실제 워치 최초 페어링, 무선 연결, 신규 설치, 기존 세이브 유지 업데이트, 게임 실행은 **미검증**입니다. 통신 차단·워치 승인창·장시간 전송·실제 화면 배율 및 Windows SmartScreen 동작도 사용자 환경 검증이 필요합니다.
- 실제 워치 시나리오를 추가 확인하기 전까지 도우미는 시험판으로 표시합니다. EXE는 코드 서명되지 않았으며 Windows 보안 설정은 변경하지 않습니다.
