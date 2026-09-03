# TamaPoke 한국어판

**DylanPDao/TamaPoke v3.11 기반** 한국어판입니다. 원작은 Quique Tortosa의 [socquique/TamaPoke](https://github.com/socquique/TamaPoke)이며, [DylanPDao 포크](https://github.com/DylanPDao/TamaPoke)의 809종·7개 지방 및 기존 게임 기능을 유지합니다. ShadowEnemyx의 Gen 3 포크와는 별개입니다.

## 설치

지원 기기는 **Waveshare ESP32-S3-Touch-AMOLED-1.75**입니다. PC용 Chrome 또는 Edge에서 이 저장소의 GitHub Pages 설치 페이지를 이용합니다. 아직 배포하지 않은 경우 아래 개발자 안내로 로컬 설치 페이지를 열 수 있습니다.

1. 데이터용 USB 케이블로 기기를 PC에 연결합니다.
2. **한국어 펌웨어 설치**를 누르고 포트를 고릅니다.
3. 업데이트라면 **Erase device를 선택하지 않고 Next**를 누릅니다.
4. 펌웨어 설치 창을 닫습니다. 기기에 microSD를 넣고 **기기 연결**을 누릅니다.
5. 관동·성도·호연·신오·하나·칼로스·알로라 중 원하는 팩이나 전체 설치를 선택합니다.
6. 완료 후 연결을 해제하고 기기를 재시작합니다.

새 게임은 한국어로 시작합니다. 기존 세이브의 언어는 그대로 유지되므로 **설정의 언어 버튼을 눌러 한국어**를 선택하세요. 선택은 NVS에 저장됩니다.

### 업데이트와 초기화

- **업데이트:** Erase device 미선택. 부트로더, 파티션 테이블, boot_app0, 앱을 각각의 위치에 기록하여 NVS/세이브 영역을 피합니다.
- **완전 초기화:** 모든 게임 데이터를 지울 의도가 있을 때만 Erase device를 선택합니다. microSD 스프라이트는 별도입니다.
- 중요한 데이터는 시리얼 콘솔의 `EXPORT` 출력 전체를 보관하세요. 복원 시 해당 `IMPORT` 명령들을 사용합니다.
- `web/firmware/tamapoke.bin`은 빈 기기용 통합 이미지입니다. 기존 세이브를 유지하는 업데이트에 사용하지 마세요. 웹 manifest는 이 파일을 참조하지 않습니다.

### 연결 문제

포트가 보이지 않으면 **BOOT를 누른 채 RESET을 한 번 누르고 BOOT에서 손을 떼세요**. 충전 전용 케이블은 사용할 수 없습니다. 다른 설치 창·시리얼 모니터를 닫고 USB 허브 없이 연결하세요. 전송 중에는 케이블·SD 카드를 분리하거나 페이지를 닫지 마세요.

## 한국어 지원 범위

- UI 문자열 173개, 포켓몬 809종 이름, 기술 테이블 90개 항목(빈 기술 포함), 18개 타입, 지방·트레이너·장소 표시.
- 메뉴, 도감, 상태, 성장, 기술 선택, 배틀, 파티, 박스, 체육관, 근거리 대전, 언어 설정.
- 기존 6개 언어와 저장 데이터·무선 통신 형식 유지.
- Galmuri11의 필요한 글자만 펌웨어에 포함. UTF-8 디코딩과 픽셀 폭 계산을 에뮬레이터·실기에서 공유합니다.
- 별명 입력은 원본 영문 키보드를 사용합니다. 사용자 별명을 자동 번역하지 않습니다.
- ESP Web Tools의 설치 팝업은 영어이며, 한국어 페이지에서 버튼 순서를 설명합니다.

## 개발자 빌드

Arduino CLI와 다음 버전을 사용합니다.

```sh
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.8 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib install "GFX Library for Arduino@1.6.7" "SensorLib@0.4.1" "XPowersLib@0.3.3"
python -m pip install Pillow fonttools
python tools/check_korean.py
python tools/build_firmware.py --publish
python tools/check_web.py
node tools/tests/installer.test.cjs
```

빌드는 ESP32-S3, 16MB 플래시, OPI PSRAM, USB CDC On Boot, `app3M_fat9M_16MB` 파티션 설정을 사용합니다. Arduino의 스케치 폴더명 요구사항과 Windows 한글 경로 문제는 빌드 스크립트가 임시 스테이징으로 처리합니다. `--cli`로 Arduino CLI 경로, `--cache`로 영문 경로의 빌드 캐시를 지정할 수 있습니다.

macOS/Linux에서는 `bash tools/build_web.sh`도 같은 한국어 빌드·설치 검사를 실행합니다. 기존 지역 팩은 그대로 사용하며, 스프라이트를 수정한 경우에만 기존 생성 도구와 `tools/pack_bundle.py`를 실행합니다.

### PC 에뮬레이터

SDL2 및 C++17 컴파일러가 필요합니다.

```sh
python tools/unpack_sprites.py
python tools/build_emulator.py
build/emulator/tamapoke-emu --shot main --dex 25 --out preview.ppm
```

Windows에서는 `--cxx`로 LLVM-MinGW의 clang++.exe, `--sdl`로 SDL2의 x86_64-w64-mingw32 디렉터리를 지정합니다. 실제 펌웨어 소스를 사용하지만 터치·SD·오디오·전원·무선 전송은 하드웨어 검증을 대체하지 않습니다.

### 설치 페이지와 GitHub Pages

```sh
python -m http.server 8765 --bind 127.0.0.1 --directory web
```

브라우저에서 `http://127.0.0.1:8765`를 엽니다. GitHub에서는 본인 포크의 **Settings → Pages → Source: GitHub Actions**를 선택하고 **Deploy installer to GitHub Pages** 워크플로를 실행합니다. `main`의 웹 파일 변경도 배포를 실행합니다. 다른 브랜치에서 수동 실행할 때는 github-pages 환경의 배포 브랜치 정책도 해당 브랜치를 허용해야 합니다.

페이지 URL은 실제 배포가 성공하면 Actions 결과에 표시됩니다. `web` 전체를 배포하므로 펌웨어와 7개 지역 팩을 같은 HTTPS 주소에서 가져옵니다. GitHub Release의 CORS에 의존하지 않습니다. 배포 구성은 [GitHub Pages 공식 안내](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages)를 참고하세요.

## 라이선스와 출처

기존 [LICENSE](LICENSE)와 [CREDITS.md](CREDITS.md)를 유지합니다. 코드 MIT, PMD SpriteCollab 스프라이트 CC BY-NC, Galmuri11 SIL OFL 1.1입니다. 포켓몬·기술 한국어 이름은 [PokeAPI 데이터](https://github.com/PokeAPI/pokeapi/tree/master/data/v2/csv)를 기준으로 연결했습니다. Pokémon © Nintendo / Game Freak / The Pokémon Company. 비공식·비상업 팬 프로젝트입니다.
