# TamaPoke 한국어판 — 다른 PC에서 이어가기

기록일: 2026-09-03. 다음 작업은 **실기 테스트 결과를 반영하는 것**입니다.

## 현재 상태

| 항목 | 확정된 내용 |
|---|---|
| 작업 저장소 | https://github.com/Loaram/TamaPoke_ko |
| 작업 기준 | DylanPDao/TamaPoke `6cd04669572fb2980d8520a74ed7f5a4c86dbadb`, 원본 v3.11 |
| 범위 | 809종·7개 지방 유지. ShadowEnemyx Gen 3 소스 사용 안 함 |
| 한국어 펌웨어 | **ko.1.0.1** |
| 현재 코드·펌웨어 | `ko.1.0.1` 태그 / `main` |
| 기본 브랜치 | `main` (한국어 작업 브랜치 `feat/korean`도 보존) |
| 공개 설치 페이지 | https://loaram.github.io/TamaPoke_ko/ |
| 실기 확인 | **아직 실시하지 않음. 사용자가 테스트 후 결과를 제공할 예정** |

ko.1.0.1은 확인창 겹침 수정판입니다. 이전 버전 3.11-ko.1의 기록은 `docs/qa/ko-3.11.1`에 보존합니다.

## 완료한 작업

- 한국어 UI 173개, 포켓몬 809종, 기술 테이블 90개 항목(빈 기술 포함), 18개 타입과 지방·트레이너·장소 이름.
- Galmuri11 부분 글꼴 632자, 비트맵 11,376바이트. UTF-8 디코딩·화면 정렬·긴 이름 폭 계산을 펌웨어와 에뮬레이터에서 공유.
- 기존 언어 인덱스 ES=0, EN=1, FR=2, DE=3, IT=4, PT=5를 유지하고 KO=6 추가. 새 세이브 기본값은 한국어, 기존 세이브의 언어 설정은 유지.
- 내부 포켓몬·기술 식별자, 저장 형식, 통신 형식 및 사용자 영문 별명 유지.
- Windows 한글 경로에서 에뮬레이터 스프라이트를 못 읽던 문제와 생성 도구의 문자 인코딩 문제 수정. Arduino 빌드는 영문 임시 경로에 스케치를 배치.
- 한국어 설치 페이지와 7개 원본 TPAK 팩, 전송 응답·시간초과·중단 처리, Pages 자동 배포 구성.
- 원본 라이선스·크레딧 유지. Galmuri SIL OFL 라이선스 포함. 이름 데이터 출처는 `localization/README.md` 참고.

## 검증 결과와 한계

| 동일 도구로 로컬 빌드 | Dylan 원본 | 한국어판 |
|---|---:|---:|
| 프로그램 플래시 | 1,808,375 B | 1,849,943 B |
| 전역 RAM | 66,116 B | 66,884 B |

한국어 앱 파일은 1,850,096바이트입니다. 프로그램 영역 3,145,728바이트의 58.8%를 사용합니다. 동적 메모리의 실기 최대 사용량은 측정하지 않았습니다.

- 이전 3.11-ko.1 [기본 브랜치 GitHub 전체 검사](https://github.com/Loaram/TamaPoke_ko/actions/runs/33709498914): 성공.
- 이전 3.11-ko.1 [GitHub Pages 배포](https://github.com/Loaram/TamaPoke_ko/actions/runs/33709498902): 성공.
- 한글 글꼴·문자열·이름·표시 폭, 세이브 복원, 이전 저장 형식 업그레이드, 통신 프로토콜 회귀 검사 통과.
- 독립 에뮬레이터 프로세스를 재시작해도 한국어 설정 유지. 실제 에뮬레이터 화면 12개 확인.
- 이전 3.11-ko.1 공개 HTTPS 페이지의 한국어 설치 버튼 활성화와 버전 표시 확인. 펌웨어 4개 파일의 SHA-256이 로컬 빌드와 일치. 7개 팩 HTTP 응답과 크기 확인.
- 증거 파일: [docs/qa/ko-3.11.1](docs/qa/ko-3.11.1/README.md).

**실기 화면·터치·전원·SD·소리·무선 대전은 확인하지 않았습니다.** 에뮬레이터나 프로토콜 테스트 통과를 실기 성공으로 기록하지 마세요. 별명 입력 키보드와 ESP Web Tools의 팝업은 영문입니다.

## ko.1.0.1 변경 및 확인

- 돌봄 종료·진화·작별 확인창의 제목, 안내 두 줄, 버튼 간격 수정. 버튼 그림과 터치 영역은 동일한 상수를 공유합니다.
- 펌웨어·manifest·빌드 정보의 버전을 `ko.1.0.1`로 통일하고 설치 페이지의 버전 검사를 대응했습니다.
- ESP32-S3 펌웨어 빌드, 한국어 데이터·설치 파일 검사, 설치 페이지 전송 검사 통과.
- 실제 스케치의 버튼 영역·취소·방출 동작을 포함한 8개 실행 검사 통과.
- 호바귀 돌봄 종료, 진화, 작별 한국어 화면과 영어 확인창의 문구 간격 확인. [화면과 빌드 증거](docs/qa/ko.1.0.1/README.md).
- 원본 게임 데이터와 세이브 형식은 유지했습니다. 실기 테스트는 아직 미실시입니다.

## 다른 PC에서 시작

새 PC의 Codex에서 이 GitHub 저장소를 열거나 클론하고 `AGENTS.md`, 이 문서, `README.ko.md`를 읽으면 됩니다. 이전 PC의 폴더 경로는 사용할 필요가 없습니다.

```sh
git clone https://github.com/Loaram/TamaPoke_ko.git
cd TamaPoke_ko
git switch main
git pull --ff-only
```

코드와 문서는 공개 저장소에서 읽을 수 있습니다. 수정사항을 올리려면 새 PC에서도 **Loaram 계정으로 GitHub 인증**이 필요합니다. 이전 PC의 로그인 정보는 저장소에 들어 있지 않습니다.

필요한 도구는 Git, Python 3.12 이상, Node.js, Arduino CLI입니다. Windows에서 실행한 Python 패키지는 Pillow 12.3.0, fonttools 4.64.0이었습니다. 검증된 보드 설정은 다음과 같습니다.

```text
esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB
```

```sh
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.8 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib install "GFX Library for Arduino@1.6.7" "SensorLib@0.4.1" "XPowersLib@0.3.3"
python -m pip install Pillow==12.3.0 fonttools==4.64.0
python tools/check_korean.py
python tools/check_web.py
node tools/tests/installer.test.cjs
```

실기 테스트는 [공개 설치 페이지](https://loaram.github.io/TamaPoke_ko/)에서 이미 빌드된 펌웨어로 시작할 수 있습니다. 코드를 고친 경우에만 다음으로 다시 빌드합니다.

```sh
python tools/build_firmware.py --publish
python tools/check_web.py
```

CLI가 PATH에 없다면 `--cli`에 실제 실행 파일 경로를 지정합니다. Windows 에뮬레이터는 LLVM-MinGW와 SDL2 2.32.10의 x86_64-w64-mingw32 패키지로 검증했습니다. `python tools/build_emulator.py --cxx <clang++경로> --sdl <SDL2경로>`로 경로를 지정할 수 있습니다. macOS/Linux는 C++17 컴파일러와 SDL2 개발 패키지를 설치한 뒤 기본 명령을 사용합니다.

```sh
python tools/unpack_sprites.py
python tools/build_emulator.py
python tools/test_runtime.py
```

Windows의 `tools/test_runtime.py`도 `--cxx` 인자를 지원합니다. `build/`, 추출한 `tools/sdcard/mons/`, 다운로드한 도구와 캐시는 새 PC에서 재생성하는 파일입니다. 소스·글꼴·배포용 펌웨어·스프라이트 팩은 모두 저장소에 있습니다.

## 실기 테스트 후 이어갈 순서

1. [실기 체크리스트](docs/HARDWARE_TEST.ko.md)를 따라 확인하고, GitHub의 [실기 테스트 결과 양식](https://github.com/Loaram/TamaPoke_ko/issues/new?template=hardware-test.md)에 기기 버전·증상·사진·재현 순서를 남깁니다. 기록된 결과는 다른 PC에서도 읽을 수 있습니다.
2. 다음 Codex 작업에 저장소 주소와 해당 테스트 이슈 링크를 전달합니다. 먼저 실제 결과를 읽고 실패 항목을 재현합니다.
3. 번역 문제는 `localization/ko.json` 및 `i18n.cpp`, 이름은 `localization/names-ko.json`, 표시 문제는 `korean_text.h`와 해당 화면 코드를 확인합니다.
4. 이름 변경 후 `python tools/gen_korean_names.py`, 표시 문자열 변경 후 `python tools/gen_korean_font.py`, 이후 `python tools/check_korean.py`를 실행합니다.
5. 원인과 관련된 테스트를 실행하고 수정한 펌웨어를 빌드합니다. 새 펌웨어를 공개할 때는 `FW_VERSION`을 올리고 `--publish`로 바이너리·manifest·해시 정보를 함께 갱신합니다.
6. `main`의 `web/` 변경은 Pages를 배포합니다. 배포 후 실제 URL에서 새 버전·다운로드·세이브 영역 보호를 확인하고 인수인계 내용을 갱신합니다.

기존 `HANDOVER.md`의 Dragonair, 보드 버전, 3.x 개발 브랜치 등은 원본 작성자의 과거 기록입니다. 현재 사용자의 실기 결과로 해석하지 마세요.

## 설치·세이브 유지 조건

업데이트할 때 **Erase device를 선택하지 않습니다.** manifest는 부트로더(0), 파티션(0x8000), boot_app0(0xe000), 앱(0x10000)을 따로 기록합니다. NVS 0x9000..0xe000와 FFat 0x610000..0xff0000를 침범하지 않는지 `tools/check_installer.py`로 검사합니다.

`web/firmware/tamapoke.bin`은 빈 기기용 통합 이미지입니다. 기존 세이브 유지 업데이트에는 사용하지 않습니다. 실제 기기 세이브는 아직 백업받지 않았으며 이번 작업 자료에 포함되어 있지 않습니다.
