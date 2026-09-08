# 3.7.2 배포 검증

3.7.2는 그림 크기와 유대 버그 패치입니다. [전체 그림 전수점검](../3.7.2-sprite-size/README.md)과 [표시 규칙](../../SPRITE-SIZING.ko.md)을 참조하세요.

- Android 3044 / Wear 3045: 빌드·공식 동일 서명·16KB 정렬·ABI·공식 그림 팩 확인.
- ESP: 최종 재빌드 및 앱/병합 펌웨어 해시 검증.
- 공식 자산 9개: SHA-256, APK 및 ZIP CRC, 동봉 워치 APK/PDF 동일성 검사.
- 플레이 PDF 29쪽과 설치 도우미 PDF 7쪽: 전체 페이지 렌더링 및 시각 검사. 기존 수동 설치 PDF는 원본과 동일.
- Windows 설치 도우미: 자동 검사 30개, 숨긴 실제 창의 최신 버전 화면 확인.
- 로컬 최종 관련 런타임 9종 및 별도 그림 74,188개 액션/영역 조합 통과.
- 전체 64종 런타임·Java 통신·SD 저장·ESP 빌드는 최종 커밋의 [GitHub Actions](https://github.com/Loaram/TamaPoke_ko/actions/workflows/verify.yml)에서 공개 전에 통과해야 합니다. Windows가 차단한 새 검사 실행을 통과로 기록하지 않습니다.

게임 코드는 마지막 그림 빌드와 동일합니다. 서버 검사에서 발견한 알로라 라이츄 폼 번호 오타는 테스트만 수정했습니다. 실제 폰·워치·ESP 설치와 프레임 속도는 이번 작업에서 검증하지 않았습니다.

배포 후 `tools/verify_public_release.py`는 main/태그 일치, 정식 최신 릴리스, 첨부 9개의 크기·SHA-256·다운로드, Pages 펌웨어/PDF와 두 워크플로 성공을 확인하며 결과는 로컬 `build/3.7.2/published-verification.json`에 남깁니다.
