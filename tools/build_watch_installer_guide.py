"""Create the supplemental Korean Windows Watch installer guide (ReportLab)."""
import argparse
from pathlib import Path
from reportlab.pdfgen import canvas
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import Paragraph, Table, TableStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.utils import ImageReader
from build_android import ROOT, firmware_version


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--preview', type=Path, required=True)
    ap.add_argument('--font-dir', type=Path, default=Path('C:/Windows/Fonts'))
    args = ap.parse_args()
    version = firmware_version()
    out = ROOT / 'output/pdf' / f'TamaPoke-{version}-Watch-Installer-Guide-KO.pdf'
    out.parent.mkdir(parents=True, exist_ok=True)
    pdfmetrics.registerFont(TTFont('KR', str(args.font_dir / 'malgun.ttf')))
    pdfmetrics.registerFont(TTFont('KRB', str(args.font_dir / 'malgunbd.ttf')))
    pdfmetrics.registerFontFamily('KR', normal='KR', bold='KRB', italic='KR', boldItalic='KRB')
    c = canvas.Canvas(str(out), pagesize=(595.28, 841.89))
    c.setTitle(f'TamaPoke {version} 워치 설치 도우미 사용 설명서')
    c.setAuthor('TamaPoke 한국어판')
    ink = colors.HexColor('#18293B')
    accent = colors.HexColor('#167D91')
    style = ParagraphStyle('body', fontName='KR', fontSize=11, leading=18, textColor=ink, wordWrap='CJK')
    y = 0

    def text(s, size=11, bold=False, gap=12):
        nonlocal y
        ps = ParagraphStyle('s', parent=style, fontSize=size, leading=size*1.65, fontName='KRB' if bold else 'KR')
        p = Paragraph(s, ps)
        _, h = p.wrap(499, 750)
        if y-h < 58:
            raise RuntimeError('Page overflow: ' + s[:50])
        p.drawOn(c, 48, y-h)
        y -= h+gap

    def page(n, title, intro):
        nonlocal y
        if n > 1:
            c.showPage()
        c.setFillColor(accent); c.rect(0, 831, 595.28, 11, fill=1, stroke=0)
        c.setFont('KRB', 9); c.drawString(48, 798, f'TamaPoke {version}  |  Windows 워치 설치 도우미')
        c.setFillColor(ink); c.setFont('KRB', 24); c.drawString(48, 751, title)
        c.setStrokeColor(colors.HexColor('#D8E2E9')); c.line(48, 48, 547, 48)
        c.setFont('KR', 8); c.drawString(48, 32, '설치 도우미 시험판 · 모든 워치 모델 검증 아님 · 2026-09-12')
        c.drawRightString(547, 32, f'{n} / 7')
        y = 720
        text(intro, size=11, gap=23)

    def table(rows, widths):
        nonlocal y
        cells = [[Paragraph(v, style) for v in row] for row in rows]
        t = Table(cells, colWidths=widths)
        t.setStyle(TableStyle([
            ('BACKGROUND', (0,0), (-1,0), colors.HexColor('#E4F2F4')),
            ('VALIGN', (0,0), (-1,-1), 'TOP'),
            ('LINEBELOW', (0,0), (-1,-1), 0.5, colors.HexColor('#D8E2E9')),
            ('TOPPADDING', (0,0), (-1,-1), 10), ('BOTTOMPADDING', (0,0), (-1,-1), 10),
            ('LEFTPADDING', (0,0), (-1,-1), 10), ('RIGHTPADDING', (0,0), (-1,-1), 10),
        ]))
        _, h = t.wrap(499, 750)
        if y-h < 58: raise RuntimeError('Table overflow')
        t.drawOn(c, 48, y-h); y -= h+18

    page(1, '명령어 없이 워치에 설치하기', 'Windows PC에서 버튼으로 PC 등록 → 워치 연결 → 설치·업데이트 → 게임 실행을 진행합니다. 워치의 설정을 켜는 과정만 직접 해 주세요.')
    text('먼저 준비하세요', 15, True)
    text('• Windows 10/11 x64 PC, Wear OS 갤럭시 워치, 서로 통신 가능한 같은 Wi-Fi가 필요합니다.<br/>• 충분히 충전한 워치와 압축을 풀 PC 공간을 준비하세요. APK 원본은 약 387MB이며, 워치 설치에는 임시 공간도 필요합니다.<br/>• Galaxy Watch4-9 배포용 정식 APK를 동봉했습니다. 모델별 실제 설치·동작을 모두 시험했다는 뜻은 아닙니다.')
    text('1. ZIP을 받고 모두 압축 풀기', 15, True)
    text(f'<link href="https://github.com/Loaram/TamaPoke_ko/releases/tag/{version}" color="#167D91"><u>GitHub {version} 릴리스 열기</u></link> → Assets에서 아래 파일을 받습니다.<br/><b>TamaPoke-{version}-Watch-Installer-Windows.zip</b><br/>다운로드한 ZIP을 오른쪽 클릭 → 모두 압축 풀기 → 추출을 누릅니다. 압축 파일 안에서 바로 실행하지 마세요.')
    text('2. 도우미 실행', 15, True)
    text('<b>TamaPoke-Watch-Installer.exe</b>를 두 번 클릭합니다. APK·ADB·DLL·검사 목록을 같은 폴더에 그대로 두세요. Python이나 Android Studio, 명령어 입력은 필요하지 않습니다. 파일 확인이 끝날 때까지 잠시 기다립니다.')
    text('알아두세요', 15, True)
    text('도우미 EXE는 코드 서명되지 않은 시험판입니다. 보안 프로그램이 차단하면 출처를 확인하고 배포자에게 문의하세요. 백신·Smart App Control을 끄지 마세요. 이 PDF와 기존 수동 설치 가이드를 릴리스에서 따로 받을 수 있습니다.', size=10)

    page(2, '워치에서 연결 준비하기', '처음 한 번은 워치에서 개발자 옵션과 무선 디버깅을 직접 켜야 합니다. PC 프로그램만으로 이 설정을 켤 수는 없습니다.')
    text('1. 같은 Wi-Fi 연결', 15, True)
    text('PC와 워치를 같은 공유기의 Wi-Fi에 연결하세요. 워치가 휴대전화와 블루투스로 연결된 것만으로는 충분하지 않습니다. 워치 Wi-Fi가 실제로 연결됐는지 확인하고 화면을 켜 둡니다.')
    text('2. 개발자 옵션 표시', 15, True)
    text('워치 <b>설정 → 워치 정보 → 소프트웨어 정보</b>에서 <b>소프트웨어 버전</b>을 여러 번 빠르게 누릅니다. 개발자 모드가 켜졌다는 안내가 나오면 설정으로 돌아갑니다. 모델·OS에 따라 메뉴 이름과 위치가 조금 다를 수 있습니다.')
    text('3. 디버깅 켜기', 15, True)
    text('<b>설정 → 개발자 옵션 → ADB 디버깅</b>을 켭니다. 이어서 <b>무선 디버깅</b>을 켜고 신뢰하는 현재 네트워크에서 연결을 허용합니다. PC 등록이나 연결 승인창이 워치에 나타나면 내용을 확인하고 허용하세요.')
    text('4. 새 PC이면 페어링 화면 열기', 15, True)
    text('<b>무선 디버깅 → 새 기기 페어링</b>을 엽니다. 여기에 표시된 IP 주소, 페어링 포트, 6자리 코드를 다음 쪽처럼 PC에 입력합니다. 등록을 마칠 때까지 이 화면을 유지하세요.')
    text('메뉴가 없거나 연결이 안 되면', 15, True)
    text('코드 기반 “새 기기 페어링”이 없는 구형 OS는 워치 소프트웨어 업데이트 가능 여부를 먼저 확인하세요. 이 도우미는 코드 기반 무선 디버깅용입니다. 게스트·회사·공용 Wi-Fi는 기기 간 통신을 막을 수 있으므로, 직접 관리하는 다른 네트워크에서 시도하세요.')

    page(3, '포트 두 개를 구분하세요', 'IP 주소는 워치를 찾는 주소이고, 포트는 콜론(:) 뒤 숫자입니다. PC 등록에 쓰는 포트와 설치 연결에 쓰는 포트를 각각 해당 화면에서 읽으세요.')
    table([
        ['<b>워치 화면</b>', '<b>표시 예시</b>', '<b>PC에 입력</b>'],
        ['새 기기 페어링', '192.168.0.12:37891<br/>코드 012345', '워치 IP 주소: 192.168.0.12<br/>페어링 포트: 37891<br/>페어링 코드: 012345'],
        ['무선 디버깅<br/>기본 화면<br/>(한 화면 뒤로)', '192.168.0.12:41237', '워치 IP 주소: 192.168.0.12<br/>연결 포트: 41237'],
    ], [120, 145, 234])
    text('위 숫자는 설명용 예시입니다', 15, True)
    text('실제 워치에 지금 표시된 값으로 입력하세요. IP 입력칸에는 콜론과 포트를 넣지 않습니다. 코드가 0으로 시작하면 0까지 포함한 6자리 전부를 입력하세요. 코드는 화면과 기록에서 숨깁니다.')
    text('PC 등록 → 한 화면 뒤로 → 연결', 15, True)
    text('1. 새 기기 페어링 화면의 값을 넣고 <b>PC 등록 (페어링)</b>을 누릅니다.<br/>2. 도우미에 <b>PC 등록 완료</b>가 나오면 워치에서 한 화면 뒤로 갑니다.<br/>3. 무선 디버깅 기본 화면에 표시된 <b>연결 포트</b>를 입력합니다.<br/>4. <b>워치 연결 확인</b>을 누릅니다. 모델과 주소가 나오면 내 워치인지 확인합니다.')
    text('다음에 설치할 때', 15, True)
    text('같은 PC의 등록이 남아 있으면 PC 등록 단계는 생략할 수 있습니다. 무선 디버깅을 다시 켜거나 네트워크를 바꿨다면 현재 IP와 연결 포트를 다시 확인하세요. 예전 포트를 그대로 쓰지 마세요.')

    page(4, '도우미 화면 한눈에 보기', f'{version}용 도우미의 실제 창을 크게 펼친 예시입니다. 작은 PC 화면에서는 창 안을 아래로 스크롤하면 나머지 버튼과 작업 기록이 보입니다.')
    pic = ImageReader(str(args.preview)); iw, ih = pic.getSize()
    w = 380; h = w*ih/iw
    c.drawImage(pic, (595.28-w)/2, y-h, width=w, height=h)
    y -= h+12
    text('처음 파일 확인 중에는 버튼이 회색입니다. 준비가 끝나면 PC 등록·연결이 가능하고, 워치 연결 확인을 마치면 설치·게임 열기가 가능합니다. 위쪽 PDF 버튼으로 이 설명서를 다시 열 수 있습니다.', 10)

    page(5, '설치하고 게임 실행하기', '신규 설치와 기존 앱 업데이트는 같은 “설치 / 업데이트” 버튼을 사용합니다. 연결 확인 없이 다른 기기에 자동 설치하지 않습니다.')
    text('1. 설치할 워치 확인', 15, True)
    text('<b>연결 완료: 모델명 (IP:포트)</b>를 확인하세요. 내 워치가 아니면 설치하지 말고 IP와 연결 포트를 수정한 뒤 다시 연결합니다. 워치로 확인되지 않은 기기에는 설치를 진행하지 않습니다.')
    text('2. 설치 / 업데이트 누르기', 15, True)
    text('설치 전 확인창에서 대상 주소와 백업 안내를 읽습니다. 기존 사용자라면 다음 쪽의 세이브 보호 절차를 먼저 마치세요. 준비됐다면 <b>예</b>를 누릅니다. 기본 선택은 아니요입니다.')
    text('3. 완료될 때까지 기다리기', 15, True)
    text('약 387MB APK를 보내므로 Wi-Fi 상태에 따라 몇 분 걸릴 수 있습니다. 도우미의 움직이는 막대는 작업 중 표시이며 정확한 완료율이 아닙니다. PC 절전과 Wi-Fi 변경을 피하고 워치 화면을 켜 두세요. 설치 작업의 대기 제한은 10분입니다.')
    text('4. 게임 열고 확인', 15, True)
    text('<b>설치 완료!</b>가 표시되면 <b>워치에서 게임 열기</b>를 누릅니다. 또는 워치 앱 목록에서 TamaPoke를 엽니다. 기존 사용자라면 키우던 포켓몬·파티·박스·도감을 확인하세요. 실행 요청 완료는 요청을 보냈다는 뜻이므로 실제 워치 화면도 확인합니다.')
    text('5. 무선 디버깅 끄기', 15, True)
    text('설치가 끝나면 워치의 무선 디버깅을 끄세요. PC 등록은 남아 있으므로 다음번 연결에 재사용할 수 있습니다. 공유 PC였다면 무선 디버깅의 등록된 기기 목록에서 이 PC를 삭제하세요.')
    text('10분 뒤 시간 초과가 표시되면 설치가 끝났을 수도 있습니다. 먼저 워치 앱 목록을 확인하세요. 앱을 삭제하지 말고, 아직 설치되지 않았다면 현재 연결 포트로 다시 연결한 뒤 재시도합니다.', 10)

    page(6, '업데이트 전 세이브 지키기', '도우미는 앱 삭제·데이터 초기화·강제 다운그레이드를 하지 않습니다. 기존 앱 위에 설치하는 방식이지만, 자동 백업이나 세이브 복구 기능은 없습니다.')
    text('미완료 한 마리 전송·교환부터 끝내세요', 15, True)
    text('앱의 <b>포켓몬 전송 / 교환</b>이 확정된 뒤 중단됐다면, 먼저 같은 상대와 재연결해 양쪽 모두 완료하세요. 미완료 거래 중에는 전체 세이브 백업·복원이 막힙니다. 해결하려고 앱을 삭제하거나 데이터를 지우지 마세요.')
    text('전체 세이브를 다른 기기에 보관하는 방법', 15, True)
    text('전체 세이브 전송을 지원하는 호환 버전의 별도 기기가 있을 때 사용합니다. 연결과 저장 호환은 함께 제공되는 플레이 가이드 16~18쪽을 참고하세요. 한 마리 전송은 전체 백업이 아닙니다.')
    text('1. 워치와 백업받을 기기를 게임의 근거리 연결 방식으로 연결합니다.<br/>2. 원본 워치에서 <b>세이브 보내기</b>, 대상에서 <b>세이브 받기</b>를 선택합니다.<br/>3. 양쪽 확인 코드가 같은지 확인하고 전송이 끝날 때까지 기다립니다.<br/>4. 대상에서 <b>세이브 적용 → 예</b>를 선택하면 대상의 기존 저장이 교체됩니다.<br/>5. 대상 앱을 다시 열어 파티·박스·도감 등이 옮겨졌는지 확인합니다.')
    text('주의: 대상 기기의 저장을 덮어씁니다', 15, True)
    text('이미 다른 중요한 세이브가 있는 기기에 무심코 적용하지 마세요. 원본 워치의 저장은 자동 삭제되지 않습니다. 백업을 확인한 뒤 워치 업데이트를 진행하고, 업데이트 후 정상 확인 전까지 백업을 유지하세요.')
    text('서명 오류나 더 최신 버전 안내가 나오면', 15, True)
    text('기존 앱을 그대로 유지하세요. 서명 불일치는 같은 서명으로 만든 업데이트가 필요하고, 더 최신 버전이 설치된 경우에는 최신 도우미가 필요합니다. 앱 삭제로 해결하면 내부 세이브를 잃을 수 있습니다.', 10)

    page(7, '막히는 곳별 해결 방법', '실패 메시지와 화면을 확인하면 어느 단계를 다시 해야 하는지 알 수 있습니다. 페어링 코드 등 민감한 값은 문의할 때 가려 주세요.')
    table([
        ['<b>증상</b>', '<b>확인할 것</b>'],
        ['PC 등록 실패', '워치의 새 기기 페어링 화면을 다시 열고 현재 포트·새 6자리 코드로 입력합니다. 등록 중에는 이 화면을 유지하세요.'],
        ['연결 실패 / offline', '같은 Wi-Fi, 워치 화면, 기본 화면의 연결 포트를 확인합니다. 무선 디버깅을 껐다 켰으면 바뀐 포트를 넣습니다.'],
        ['unauthorized', '워치에 PC 연결 승인창이 있는지 확인합니다. 필요하면 PC 등록부터 다시 진행합니다.'],
        ['파일 없음 / 손상', '정식 ZIP을 다시 받고 모두 압축 풉니다. EXE만 따로 옮기거나 검사 목록을 수정하지 마세요.'],
        ['저장 공간 부족', '워치의 불필요한 다른 파일을 정리합니다. TamaPoke 앱 데이터는 삭제하지 마세요.'],
        ['설치 성공, 게임이 안 보임', '워치 앱 목록을 확인합니다. 도우미로 다시 연결하고 게임 열기를 누르세요. 계속 실패하면 오류 기록과 모델·OS를 알려 주세요.'],
        ['Windows 실행 차단', '배포 출처를 확인하고 문의합니다. 보안 기능을 끄지 마세요. 기존 수동 설치 PDF를 대안으로 참고할 수 있습니다.'],
    ], [142, 357])
    text('검증 범위와 참고 자료', 13, True, gap=6)
    text('자동 입력·오류·손상 파일 검사, 화면 및 ZIP 검사를 수행했습니다. 실제 워치 페어링·신규 설치·세이브 유지 업데이트는 미검증입니다. SHA-256은 파일 손상 확인용이며 배포자 신원을 보증하는 전자서명이 아닙니다.', 9, gap=7)
    text(f'<link href="https://developer.android.com/training/wearables/get-started/debug-wifi" color="#167D91"><u>Android Developers: Wear OS Wi-Fi 디버깅</u></link><br/><link href="https://developer.android.com/tools/releases/platform-tools" color="#167D91"><u>Android Developers: Platform-Tools</u></link><br/>게임 저장 안내: 함께 배포하는 {version} 플레이 가이드와 릴리스 설명. 연결 도구의 출처·라이선스는 동봉 NOTICE.txt를 확인하세요.', 9, gap=0)
    c.save()
    print(out)


if __name__ == '__main__':
    main()
