# 폰 ↔ 워치 통신 수정 후보 검증

상태: **2026-09-08 사용자에게서 테스트판으로 문제가 해결됐다는 확인과 업로드 요청을 받음.** 아래는 테스트 당시 기록이며 정식 배포 검증은 [3.7.1 보고서](../3.7.1/README.md)를 참고한다.

## 사용자 보고 / 진단 경계

- 양쪽 3.7.0에서 폰 ↔ 워치 세이브 전송 진행률이 오르지 않고 시간이 지나면 “상대가 나갔어요” 표시.
- 같은 조합의 근거리 대전도 실패.
- 워치 Bluetooth를 끄고 다른 폰의 핫스팟에 연결해도 실패.
- **폰끼리 통신은 성공**한다는 추가 확인을 받음.
- 이를 바탕으로 워치의 네트워크 선택/상대 검색 수신 경로를 우선 보완했다. 실제 기기의 패킷 기록 없이 특정 OS/드라이버 원인으로 확정하지 않는다.

## 수정 범위

1. 실제 Wi-Fi Network를 요청하고 선택한다. 해당 Network의 IPv4 LinkProperties가 준비된 뒤 소켓을 만든다. INTERNET capability는 요구하지 않아 ESP의 인터넷 없는 방도 배제하지 않는다.
2. Wi-Fi 수신 필터용 MulticastLock을 통신 세션 동안만 획득한다. 세션 종료·실패·앱 백그라운드·Activity 종료 시 callback, binding, lock을 해제한다.
3. Wi-Fi 주소와 prefix로 directed broadcast를 구성해 인터페이스 열거가 실패해도 검색 목적지를 얻는다. 기존 UDP 포트와 프레임 형식은 유지한다.
4. 재시도는 기존 소켓을 폐기하고 새로 시작한다. 연결된 Wi-Fi가 바뀌면 낡은 소켓으로 조용히 계속하지 않고 재시도를 안내한다.
5. Android/Wear 연결 화면에 IP, TX, RX, 마지막 오류 번호와 연결 단계 표시를 추가한다. 상대가 확인되지 않은 시간 초과는 “상대를 찾지 못했습니다”로 구분한다.
6. 수신 루프는 한 프레임당 64개로 제한해 과다 패킷 때문에 취소/타이머/게임 루프가 굶지 않도록 한다.

저장 v7, 개체 레코드, 통신 프로토콜, 게임 밸런스 및 그림 팩은 변경하지 않았다. 실기 비교를 위해 테스트판은 정식 3.7.0과 같은 versionCode/서명을 유지하고 APK versionName의 revision만 2로 구분한다. 해결 확인 후 정식 3.7.1에서는 versionCode도 올린다.

## 자동 검사

- `tools/test_runtime.py --only android_udp`: 실제 `tools/android/link_udp.cpp`를 가상 POSIX/JNI 경계에서 실행. 권한/네트워크 준비 순서, 주소, 검색, 자기 신호 무시, 제3기기 차단, 끊김/재시도/오류 정리, 과다 수신 제한, 300칸 세이브의 양방향 전송과 단방향/양방향 패킷 손실. 29개 확인 통과.
- `tools/test_android_lan_java.py`: 실제 Activity를 가상 Android API로 실행. callback 순서, prefix 계산, 주소 변경, 오래된 callback 무시, timeout, permission 예외, binding 실패, IPv6-only 대기, 백그라운드/종료/중복 해제. 20개 확인 통과.
- Android ARM64/x86_64 및 Wear ARM32/ARM64 빌드, 서명/패키지/16KB 정렬 확인 통과.
- **전체 62개 런타임 회귀 검사 통과.** APK/소스 SHA-256은 로컬 `build/lan-fix-test/verification.json`에 기록했다. 패키지 ID·정식 서명·ABI·10개 그림 팩의 정식판 대비 바이트 일치도 확인했다.

실제 무선 드라이버/공유기/워치 UI 표시 검사는 이 모의 검사로 대체하지 않는다. 폰 핫스팟 제공 기기 자체에서의 실행과 ESP 실기 통신도 아직 확인하지 않았다. 공유기에서 자동 검색/기기 간 통신을 차단하면 별도 대책이 필요할 수 있다.

## 실기 통과 기준 / 배포 게이트

- 우선 워치만 테스트판으로 업데이트하고 폰은 기존 3.7.0 유지.
- 같은 Wi-Fi에서 상대 이름/확인 코드 표시, 세이브 송수신 진행률 100%와 받기 완료 확인. 적용은 사용자가 수신 기기 세이브 교체를 원할 때만 진행.
- 폰 ↔ 워치 근거리 대전 연결 확인. 가능하면 양방향/재시도도 확인.
- 실패하면 대기 화면의 IP/TX/RX/E와 방향을 받아 다음 진단을 진행.
- 사용자 실기 성공 확인 전 GitHub 릴리스/Pages/정식 APK를 교체하지 않는다. 성공 확인 뒤 정식 버그 패치 3.7.1과 필수 가이드를 준비한다.

## 설계 참고

- [Wear OS 직접 네트워크 통신](https://developer.android.com/training/wearables/data/network-communication): Wi-Fi request, binding과 사용 후 해제.
- [WifiManager.MulticastLock](https://developer.android.com/reference/android/net/wifi/WifiManager.MulticastLock): 수신 필터 요청과 배터리 비용/해제. 이것이 특정 워치 드라이버의 broadcast 문제를 반드시 해결한다는 뜻은 아니다.
