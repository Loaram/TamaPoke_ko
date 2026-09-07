# 3.5.0 개체 전송 / 교환 검증

교환 프로토콜·사용 화면·기존 저장을 검사하고 Android/Wear/ESP/PC를 같은 소스로 빌드합니다. [사용법](../../RELEASE-3.5.0.ko.md), [설계와 한계](../../INDIVIDUAL-TRANSFER.ko.md).

- `trade.log`: 독립된 두 저장 상태, 손실 패킷, 300번째 박스 칸 선물, 양쪽 승인, 취소, 확정 이후 취소 금지, 재시작, 디스크 체크포인트 실패, 실제 영구 쓰기 다섯 경계의 모의 전원 차단, 보관 쓰기 실패, 잘못된 역할/개체, 전체 백업에서 로컬 거래 기록 제외.
- `runtime.log`: 전체 55개 중 기존 탐색 실행 파일에서 Windows 응용 프로그램 제어 정책(4551)으로 중단된 기록을 그대로 보존합니다. 이는 통과 기록이 아닙니다. 앞쪽 교환 프로토콜·실제 UI와 기존 저장·배틀 검사는 통과했습니다.
- `runtime-tail.log`: 중단 지점 뒤의 기존 탐색 결과·이로치 알·파티 생존·앱 저장·폼·박스·성장 검사입니다.
- `sd-store.log`: 기존 ESP SD 이중 보관 저장 실패 검사 10개.
- 기기별 빌드 로그, `build-results.json`: 공개 6파일, APK ABI/그림 팩/해시, ESP 이미지, 27쪽 가이드와 변경 없는 워치 설치 가이드 검증.
- `trade-*.png`, `guide-contact-*.png`: 에뮬레이터 화면 및 PDF 전체 페이지 검수. 무선 상대 미리보기는 UI 테스트 데이터입니다.

Windows 보안 설정은 변경하지 않습니다. [GitHub Verify Korean edition](https://github.com/Loaram/TamaPoke_ko/actions/workflows/verify.yml)의 동일 배포 코드 전체 55개 검사 및 ESP 빌드 성공을 정식 공개 조건으로 삼습니다. 실제 기기 설치·기기 간 무선·물리 SD 전원 차단 시험은 별도이며 완료했다고 주장하지 않습니다.
