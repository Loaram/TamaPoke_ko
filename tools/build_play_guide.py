from __future__ import annotations

from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    Image,
    PageBreak,
    Paragraph,
    SimpleDocTemplate,
    Spacer,
    Table,
    TableStyle,
)


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "output" / "pdf" / "TamaPoke-ko.1.1.6-Play-Guide-KO.pdf"
VERSION = "ko.1.1.6"
PAGE_TOTAL = 16

FONT = Path(r"C:\Windows\Fonts\malgun.ttf")
FONT_BOLD = Path(r"C:\Windows\Fonts\malgunbd.ttf")
pdfmetrics.registerFont(TTFont("Malgun", str(FONT)))
pdfmetrics.registerFont(TTFont("MalgunB", str(FONT_BOLD)))

INK = colors.HexColor("#16223A")
MUTED = colors.HexColor("#5A6477")
RED = colors.HexColor("#B62B43")
BLUE = colors.HexColor("#3867D6")
GREEN = colors.HexColor("#258441")
PALE_RED = colors.HexColor("#FFF3F5")
PALE_BLUE = colors.HexColor("#EEF3FF")
PALE_GREEN = colors.HexColor("#EFF9F2")
PALE_GRAY = colors.HexColor("#F5F7FA")
LINE = colors.HexColor("#D9DFEA")

styles = getSampleStyleSheet()
BODY = ParagraphStyle(
    "BodyKo",
    fontName="Malgun",
    fontSize=10.2,
    leading=16,
    textColor=INK,
    spaceAfter=5,
)
SMALL = ParagraphStyle(
    "SmallKo",
    parent=BODY,
    fontSize=8.5,
    leading=12.5,
    textColor=MUTED,
)
TITLE = ParagraphStyle(
    "TitleKo",
    fontName="MalgunB",
    fontSize=23,
    leading=30,
    textColor=INK,
    spaceAfter=3,
)
SUBTITLE = ParagraphStyle(
    "SubtitleKo",
    parent=BODY,
    fontSize=10.5,
    leading=16,
    textColor=MUTED,
    spaceAfter=10,
)
H2 = ParagraphStyle(
    "H2Ko",
    fontName="MalgunB",
    fontSize=14,
    leading=20,
    textColor=BLUE,
    spaceBefore=6,
    spaceAfter=7,
)
H3 = ParagraphStyle(
    "H3Ko",
    fontName="MalgunB",
    fontSize=11,
    leading=16,
    textColor=INK,
    spaceBefore=5,
    spaceAfter=3,
)
CENTER = ParagraphStyle("CenterKo", parent=BODY, alignment=TA_CENTER)
TABLE_HEAD = ParagraphStyle(
    "TableHeadKo",
    parent=BODY,
    fontName="MalgunB",
    textColor=colors.white,
    alignment=TA_CENTER,
    spaceAfter=0,
)
CALLOUT = ParagraphStyle(
    "CalloutKo",
    parent=BODY,
    leftIndent=5 * mm,
    rightIndent=5 * mm,
    borderColor=BLUE,
    borderWidth=1,
    borderPadding=7,
    backColor=PALE_BLUE,
    spaceBefore=5,
    spaceAfter=8,
)
DANGER = ParagraphStyle(
    "DangerKo",
    parent=CALLOUT,
    borderColor=RED,
    backColor=PALE_RED,
)
SAFE = ParagraphStyle(
    "SafeKo",
    parent=CALLOUT,
    borderColor=GREEN,
    backColor=PALE_GREEN,
)


def p(text: str, style: ParagraphStyle = BODY) -> Paragraph:
    return Paragraph(text, style)


def bullet(text: str) -> Paragraph:
    return Paragraph("• " + text, BODY)


def step_table(items: list[str]) -> Table:
    number_style = ParagraphStyle(
        "StepNumber",
        parent=CENTER,
        fontName="MalgunB",
        fontSize=10,
        textColor=colors.white,
        leading=13,
    )
    rows = [[p(str(i), number_style), p(text)] for i, text in enumerate(items, 1)]
    table = Table(rows, colWidths=[10 * mm, 164 * mm], hAlign="LEFT")
    table.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (0, -1), BLUE),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("BOX", (0, 0), (-1, -1), 0.4, LINE),
                ("INNERGRID", (0, 0), (-1, -1), 0.35, LINE),
                ("LEFTPADDING", (1, 0), (1, -1), 8),
                ("RIGHTPADDING", (1, 0), (1, -1), 8),
                ("TOPPADDING", (0, 0), (-1, -1), 6),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 6),
            ]
        )
    )
    return table


def info_table(rows: list[list[str]], widths: list[float]) -> Table:
    rendered = []
    for row_index, row in enumerate(rows):
        style = TABLE_HEAD if row_index == 0 else BODY
        rendered.append([p(cell, style) for cell in row])
    table = Table(rendered, colWidths=widths, hAlign="LEFT", repeatRows=1)
    table.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, 0), INK),
                ("GRID", (0, 0), (-1, -1), 0.45, LINE),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("TOPPADDING", (0, 0), (-1, -1), 6),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 6),
                ("LEFTPADDING", (0, 0), (-1, -1), 7),
                ("RIGHTPADDING", (0, 0), (-1, -1), 7),
            ]
        )
    )
    return table


def screenshot(path: Path, width: float = 65 * mm) -> Image:
    image = Image(str(path))
    image.drawWidth = width
    image.drawHeight = width
    return image


def screenshot_pair(left: Path, right: Path, width: float = 64 * mm) -> Table:
    table = Table(
        [[screenshot(left, width), screenshot(right, width)]],
        colWidths=[87 * mm, 87 * mm],
        hAlign="CENTER",
    )
    table.setStyle(
        TableStyle(
            [
                ("ALIGN", (0, 0), (-1, -1), "CENTER"),
                ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
                ("BACKGROUND", (0, 0), (-1, -1), PALE_GRAY),
                ("BOX", (0, 0), (-1, -1), 0.5, LINE),
                ("TOPPADDING", (0, 0), (-1, -1), 7),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 7),
            ]
        )
    )
    return table


def page_heading(section: str, title: str, description: str) -> list:
    return [
        p(section, ParagraphStyle("SectionLabel", parent=SMALL, fontName="MalgunB", textColor=RED)),
        p(title, TITLE),
        p(description, SUBTITLE),
    ]


def page_break(story: list) -> None:
    story.append(PageBreak())


def header_footer(canvas, doc) -> None:
    canvas.saveState()
    canvas.setStrokeColor(LINE)
    canvas.line(18 * mm, 15 * mm, 192 * mm, 15 * mm)
    canvas.setFont("Malgun", 8)
    canvas.setFillColor(MUTED)
    canvas.drawString(18 * mm, 10.5 * mm, f"TamaPoke 한국어판 {VERSION}")
    canvas.drawRightString(192 * mm, 10.5 * mm, f"{doc.page} / {PAGE_TOTAL}")
    canvas.restoreState()


screens = ROOT / "docs" / "screens"
qa_current = ROOT / "docs" / "qa" / "ko.1.1.6"

story: list = []

# 1. Cover
story.extend(
    [
        Spacer(1, 23 * mm),
        p("TAMAPOKE KOREAN EDITION", ParagraphStyle("Eyebrow", parent=CENTER, fontName="MalgunB", fontSize=9, textColor=RED)),
        Spacer(1, 5 * mm),
        p(
            "TamaPoke 한국어판",
            ParagraphStyle("CoverTitle", parent=TITLE, alignment=TA_CENTER, fontSize=30, leading=39),
        ),
        p(
            "ko.1.1.6 플레이 설명서",
            ParagraphStyle("CoverSub", parent=H2, alignment=TA_CENTER, fontSize=18, leading=26, textColor=BLUE),
        ),
        Spacer(1, 8 * mm),
        screenshot(screens / "main.png", 79 * mm),
        Spacer(1, 8 * mm),
        p("Waveshare ESP32-S3-Touch-AMOLED-1.75", CENTER),
        p("Android · Galaxy Watch4~9 연동 안내 포함", CENTER),
        Spacer(1, 10 * mm),
        p("설치 · 기본 조작 · 성장 · 기술 · 60칸 박스 · 체육관 · 근거리 배틀 · 세이브 이전", ParagraphStyle("CoverLine", parent=SMALL, alignment=TA_CENTER)),
        Spacer(1, 8 * mm),
        p("2026-09-05", CENTER),
    ]
)
page_break(story)

# 2. Firmware install
story.extend(page_heading("01 설치", "펌웨어 설치와 안전한 업데이트", "Chrome 또는 Edge에서 공개 설치 페이지를 이용합니다."))
story.append(
    step_table(
        [
            "데이터 전송이 가능한 USB 케이블로 ESP32를 PC에 직접 연결합니다.",
            "<b>https://loaram.github.io/TamaPoke_ko/</b>를 열고 <b>한국어 펌웨어 설치</b>를 누릅니다.",
            "목록에서 ESP32 직렬 포트를 고른 뒤 설치를 진행합니다.",
            "기존 게임을 업데이트할 때는 <b>Erase device를 선택하지 않고 Next</b>를 누릅니다.",
            "설치 완료 메시지가 나오면 설치 창을 닫습니다.",
        ]
    )
)
story.extend(
    [
        Spacer(1, 5 * mm),
        p("저장을 지키는 핵심", H2),
        p("<b>Erase device</b>를 선택하면 포켓몬, 도감, 파티, 박스, 배지와 설정이 모두 초기화됩니다. 완전히 새로 시작할 때만 선택하세요.", DANGER),
        p("웹 설치기는 부트로더, 파티션, boot_app0, 앱을 나눠 기록해 세이브 영역을 피합니다. 업데이트에는 통합 <b>tamapoke.bin</b>을 직접 쓰지 마세요.", SAFE),
        p("포트가 보이지 않으면 BOOT를 누른 채 RESET을 한 번 누르고 BOOT에서 손을 뗍니다. 충전 전용 케이블과 USB 허브는 피하세요.", SMALL),
    ]
)
page_break(story)

# 3. Sprites
story.extend(page_heading("02 설치", "9개 지방 스프라이트 설치", "포켓몬 그림은 microSD에 지방별 팩으로 넣습니다."))
story.append(
    info_table(
        [
            ["도감 지방", "포함 범위", "설치 선택"],
            ["관동 · 성도 · 호연", "초기 세대", "필요한 지방만 선택 가능"],
            ["신오 · 하나 · 칼로스", "중기 세대", "나중에 추가 설치 가능"],
            ["알로라 · 가라르 · 팔데아", "후기 세대", "전국도감 1025종 범위"],
        ],
        [48 * mm, 56 * mm, 70 * mm],
    )
)
story.extend(
    [
        Spacer(1, 5 * mm),
        p("설치 순서", H2),
        step_table(
            [
                "기기에 microSD를 넣고 펌웨어 설치 창이 닫혔는지 확인합니다.",
                "설치 페이지에서 <b>기기 연결</b>을 누르고 직렬 포트를 선택합니다.",
                "원하는 지방 버튼 또는 <b>9개 지방 전체 설치</b>를 누릅니다.",
                "완료 메시지가 나올 때까지 페이지를 열어 두고 케이블과 SD 카드를 빼지 않습니다.",
                "<b>연결 해제</b>를 누른 뒤 ESP32를 재시작합니다.",
            ]
        ),
        Spacer(1, 4 * mm),
        p("관동 팩 하나도 약 10~15분 걸릴 수 있습니다. 없는 지방 팩의 포켓몬은 빈 그림으로 나오지 않도록 해당 도감과 알 후보에서 잠깁니다.", CALLOUT),
    ]
)
page_break(story)

# 4. First start and controls
story.extend(page_heading("03 시작", "첫 파트너와 기본 조작", "처음 실행하면 지방과 스타팅 포켓몬을 고릅니다."))
story.append(screenshot_pair(screens / "egg.png", qa_current / "guide-starter.png"))
story.extend(
    [
        Spacer(1, 5 * mm),
        bullet("후보를 본 뒤 <b>지방 선택</b>으로 돌아가 다른 지방 스타팅과 비교할 수 있습니다."),
        bullet("포켓몬을 짧게 누르면 쓰다듬고, 이름을 누르면 상세 메뉴가 열립니다."),
        bullet("아래 아이콘은 먹이, 수면, 목욕, 훈련입니다."),
        bullet("홈에서 오른쪽으로 밀면 파티, 왼쪽으로 밀면 체육관으로 이동합니다."),
        bullet("PWR를 짧게 누르면 화면을 끄고, 길게 누르면 전원을 끕니다."),
        p("기존 저장은 언어 설정을 유지합니다. 한국어가 아니면 트레이너 설정의 언어 버튼에서 한국어를 선택하세요.", CALLOUT),
    ]
)
page_break(story)

# 5. Current rules
story.extend(page_heading("04 육성", "ko.1.1.6의 성장과 돌봄 규칙", "이전 설명서와 달라진 현재 수치를 한눈에 확인하세요."))
story.append(
    info_table(
        [
            ["항목", "현재 규칙", "기억할 점"],
            ["성장", "20분마다 1레벨", "레벨 100은 약 1일 9시간"],
            ["작별", "최종 진화 후 1일", "레벨 73 무렵부터 제안 가능"],
            ["수면 활력", "분당 +8", "실시간과 전원 꺼짐 시간 모두 적용"],
            ["조기 돌봄 종료", "다음 진화 지연 없음", "현재 포켓몬은 파티에 남지 않음"],
            ["방어 훈련", "활력 12 소모", "결과에 방어 +N 표시"],
        ],
        [37 * mm, 57 * mm, 80 * mm],
    )
)
story.extend(
    [
        Spacer(1, 6 * mm),
        p("기기의 시간이 기준입니다", H2),
        p("Android와 Wear OS는 휴대전화 또는 워치의 날짜와 시간을 따릅니다. 시간이 뒤로 바뀌어도 잘못된 대량 레벨업으로 계산하지 않습니다. ESP32에서는 기기 설정 시각을 사용합니다.", CALLOUT),
        p("작별을 거절하면 하루 뒤 다시 제안합니다. 최종 진화 전, 수면 중, 영구 동료 상태에서는 작별이 열리지 않습니다.", BODY),
    ]
)
page_break(story)

# 6. Dex and gyms
story.extend(page_heading("05 도감", "1025종 도감과 7개 지방 체육관", "도감은 9개 지방, 체육관은 관동부터 알로라까지 7개 지방입니다."))
story.append(screenshot_pair(qa_current / "guide-gallery.png", screens / "gymsj.png"))
story.extend(
    [
        Spacer(1, 5 * mm),
        bullet("도감은 관동 · 성도 · 호연 · 신오 · 하나 · 칼로스 · 알로라 · 가라르 · 팔데아를 지원합니다."),
        bullet("도감 화면에서 위아래로 포켓몬을 바꾸고 좌우로 지방 또는 페이지를 이동합니다."),
        bullet("체육관은 각 지방 관장 8명, 사천왕 4명, 챔피언 1명 순서입니다."),
        bullet("ko.1.1.6에서는 선택한 지방의 관장 이름, 상대 팀과 획득 배지가 전투 끝까지 같은 지방으로 유지됩니다."),
        p("그림 팩이 설치되지 않은 지방은 잠금으로 표시될 수 있습니다. 먼저 설치 페이지에서 해당 지방 팩을 넣으세요.", SAFE),
    ]
)
page_break(story)

# 7. Moves and evolution
story.extend(page_heading("06 기술", "레벨업과 진화 기술", "현재 기술표는 자연 습득 기술 689개와 진화 즉시 습득 기술을 포함합니다."))
story.append(screenshot_pair(screens / "moves.png", qa_current / "guide-profile.png"))
story.extend(
    [
        Spacer(1, 5 * mm),
        bullet("레벨업으로 새 기술을 배우며, 기술 네 칸이 차 있으면 교체할 기술을 선택합니다."),
        bullet("마스카나의 <b>트릭플라워</b>처럼 진화 순간 배우는 기술은 진화 직후 제안됩니다."),
        bullet("진화 기술을 지나쳤더라도 기술 카드의 레벨 1 목록에서 다시 선택할 수 있습니다."),
        bullet("한번에 여러 레벨이 올라도 기술 제안 대기열이 저장되어 재시작 후 이어집니다."),
        p("복잡한 고유 효과는 TamaPoke의 축약 전투 규칙으로 동작하지만, 습득 시점, 타입, 분류, 위력과 명중률은 현재 기술표를 따릅니다.", CALLOUT),
    ]
)
page_break(story)

# 8. Party and box
story.extend(page_heading("07 파티", "6마리 파티와 60칸 박스", "박스는 6마리씩 10페이지이며 아래에 현재/전체 페이지 숫자가 표시됩니다."))
story.append(screenshot_pair(qa_current / "guide-party.png", screens / "box.png"))
story.extend(
    [
        Spacer(1, 5 * mm),
        bullet("포켓몬 칸을 눌러 위치를 바꾸거나 상세 정보, 기술, 훈련 상태를 확인합니다."),
        bullet("박스 안에서는 좌우로 밀어 1/10부터 10/10까지 이동합니다."),
        bullet("기존 18칸 저장은 업데이트 후 앞쪽 18칸에 그대로 보존됩니다."),
        bullet("기기간 세이브 전송에는 라이브 포켓몬, 파티와 60칸 박스, 도감, 배지와 설정이 함께 들어갑니다."),
        p("놓아주기는 세이브 이전이나 다른 기기로 옮기기 버튼이 아닙니다. 포켓몬을 영구적으로 떠나보내는 기능이므로 확인 문구를 읽고 선택하세요.", DANGER),
    ]
)
page_break(story)

# 9. Training and battles
story.extend(page_heading("08 전투", "훈련과 배틀 기본", "활력과 기술 구성을 확인한 뒤 전투를 시작하세요."))
story.append(screenshot_pair(qa_current / "guide-defense.png", qa_current / "guide-battle.png"))
story.extend(
    [
        Spacer(1, 5 * mm),
        bullet("공격, 방어, 스피드 훈련은 해당 능력치를 올립니다. 방어 훈련은 활력 12를 사용하고 상승량을 화면에 표시합니다."),
        bullet("배틀에서는 기술을 고르거나 교체, 도망가기 등 표시된 행동을 선택합니다."),
        bullet("기절한 포켓몬과 활력이 부족한 포켓몬을 확인하고 파티 순서를 정리합니다."),
        bullet("타입 상성에 따라 효과가 굉장함, 별로임 또는 없음으로 표시됩니다."),
        p("근거리 배틀은 다른 사용자의 파티와 연결하는 기능입니다. 다음 장의 네트워크 준비를 먼저 완료해야 합니다.", CALLOUT),
    ]
)
page_break(story)

# 10. Gym regional behavior
story.extend(page_heading("09 체육관", "지방별 관장과 배지 진행", "왼쪽 화면에서 지방과 상대를 고르고 준비된 파티로 도전합니다."))
story.append(screenshot_pair(qa_current / "guide-gallery.png", screens / "gymsj.png"))
story.extend(
    [
        Spacer(1, 5 * mm),
        step_table(
            [
                "체육관 화면에서 좌우로 지방을 선택합니다.",
                "위아래로 관장, 사천왕 또는 챔피언을 고릅니다.",
                "상대 이름과 지방을 확인한 뒤 도전을 시작합니다.",
                "승리하면 선택한 지방의 다음 상대와 배지가 열립니다.",
            ]
        ),
        Spacer(1, 5 * mm),
        p("ko.1.1.6은 관동으로 잘못 바뀌던 지역 상태를 수정했습니다. 7개 지방의 관장 8명, 사천왕 4명, 챔피언 1명을 합친 91개 시작 조합을 자동 검사했습니다.", SAFE),
        p("자동 검사는 통과했지만 실제 ESP32 화면에서 모든 91개 전투를 끝까지 플레이한 것은 아닙니다.", SMALL),
    ]
)
page_break(story)

# 11. LAN overview
story.extend(page_heading("10 근거리 배틀", "어떤 기기끼리 연결할 수 있나요?", "ko.1.1.6끼리 연결하면 ESP32, Android와 Wear OS가 같은 전투 규격을 사용합니다."))
story.append(
    info_table(
        [
            ["조합", "연결 방식", "먼저 할 일"],
            ["ESP32 ↔ ESP32", "ESP-NOW 직접 연결", "두 기기에서 근거리 대전 열기"],
            ["Android ↔ Android", "같은 Wi-Fi의 UDP", "두 앱을 같은 공유기에 연결"],
            ["Android ↔ Wear OS", "같은 Wi-Fi의 UDP", "휴대전화와 워치를 같은 공유기에 연결"],
            ["ESP32 ↔ 앱/워치", "ESP32 Wi-Fi 방의 UDP", "ESP32에서 먼저 근거리 대전 열기"],
        ],
        [39 * mm, 58 * mm, 77 * mm],
    )
)
story.extend(
    [
        Spacer(1, 6 * mm),
        p("ESP32가 만드는 Wi-Fi", H2),
        p("ESP32 화면에 <b>TamaPoke-XXXX</b>가 표시됩니다. Android 또는 워치의 Wi-Fi 설정에서 그 이름을 선택하고 암호 <b>tamapoke</b>를 입력합니다. 인터넷 없음 안내가 나오면 연결 유지를 선택하세요.", CALLOUT),
        bullet("통신 포트는 UDP 38631입니다. 공유기나 보안 앱이 로컬 통신을 막으면 연결되지 않을 수 있습니다."),
        bullet("상대를 찾는 최초 제한 시간은 최대 약 90초입니다."),
        bullet("새 16비트 기술을 포함하므로 무선 프로토콜 4인 ko.1.1.5 이상과 연결합니다."),
    ]
)
page_break(story)

# 12. LAN with ESP
story.extend(page_heading("11 근거리 배틀", "ESP32와 Android/워치 연결", "ESP32가 Wi-Fi 방을 만든 뒤 상대 기기가 그 방에 참가합니다."))
story.append(screenshot(qa_current / "guide-lan.png", 74 * mm))
story.extend(
    [
        Spacer(1, 4 * mm),
        step_table(
            [
                "ESP32에서 트레이너 메뉴의 <b>근거리 대전</b>을 엽니다.",
                "<b>방 만들기</b> 또는 <b>참가하기</b>를 누르고 사용할 파티를 선택합니다.",
                "ESP32 화면의 <b>TamaPoke-XXXX</b>를 확인합니다.",
                "Android 또는 워치를 해당 Wi-Fi에 연결하고 암호 <b>tamapoke</b>를 입력합니다.",
                "앱 또는 워치에서도 근거리 대전을 열고 방 선택과 파티 선택을 진행합니다.",
                "상대 이름과 준비 완료가 표시되면 전투를 시작합니다.",
            ]
        ),
        Spacer(1, 4 * mm),
        p("양쪽에서 같은 방 선택을 눌러도 기기 고유 번호를 비교해 역할을 자동 조정합니다. 그래도 연결이 안 되면 한쪽은 방 만들기, 다른 쪽은 참가하기로 다시 시도하세요.", SAFE),
    ]
)
page_break(story)

# 13. LAN app pairs
story.extend(page_heading("12 근거리 배틀", "앱과 앱, 워치끼리 연결", "두 기기를 같은 Wi-Fi 공유기에 연결한 상태에서 진행합니다."))
story.append(
    step_table(
        [
            "두 기기의 버전을 ko.1.1.6 또는 호환되는 프로토콜 4 버전으로 맞춥니다.",
            "휴대전화와 워치를 같은 Wi-Fi에 연결합니다. 모바일 데이터만 켠 상태는 사용할 수 없습니다.",
            "근거리 대전을 처음 열 때 로컬 네트워크 권한이 나오면 허용합니다.",
            "양쪽에서 트레이너 메뉴의 <b>근거리 대전</b>을 엽니다.",
            "방 만들기 또는 참가하기를 선택하고 사용할 파티를 고릅니다.",
            "상대가 나타나면 이름을 확인하고 준비 완료 상태에서 전투를 시작합니다.",
        ]
    )
)
story.extend(
    [
        Spacer(1, 6 * mm),
        p("연결이 안 될 때", H2),
        bullet("공유기의 게스트 Wi-Fi나 AP 격리 기능은 기기끼리 보지 못하게 할 수 있습니다."),
        bullet("VPN, 방화벽, 배터리 절약 모드를 잠시 끄고 TamaPoke의 로컬 네트워크 권한을 확인합니다."),
        bullet("실패 후에는 양쪽 모두 뒤로 나갔다가 다시 들어오고 최대 90초 기다립니다."),
        p("근거리 배틀은 인터넷 서버를 거치지 않습니다. 같은 장소의 로컬 연결에서만 상대를 찾습니다.", CALLOUT),
    ]
)
page_break(story)

# 14. Save transfer overview
story.extend(page_heading("13 세이브 이전", "보내기와 받기 전에 확인", "세이브 전송은 원본을 복사해 대상 기기의 저장을 교체하는 기능입니다."))
story.append(screenshot_pair(qa_current / "guide-lan.png", qa_current / "guide-savereceive.png"))
story.extend(
    [
        Spacer(1, 5 * mm),
        p("반드시 역할을 다르게 선택", H2),
        bullet("기존 세이브가 있는 원본 기기: <b>세이브 보내기</b>"),
        bullet("세이브를 받을 대상 기기: <b>세이브 받기</b>"),
        bullet("보내기↔보내기 또는 받기↔받기는 연결을 거부합니다."),
        p("받기 완료만으로 대상의 기존 세이브가 바뀌지 않습니다. 마지막에 <b>세이브 적용 → 예</b>를 눌렀을 때만 덮어씁니다.", SAFE),
        p("원본 기기의 세이브는 자동 삭제되지 않습니다. 양방향으로 교환하려면 한 방향을 끝낸 뒤 역할을 바꿔 다시 전송합니다.", CALLOUT),
    ]
)
page_break(story)

# 15. Save transfer steps
story.extend(page_heading("14 세이브 이전", "세이브 보내기, 확인, 적용", "연결 준비는 근거리 배틀과 같고 메뉴에서 보내기와 받기를 선택합니다."))
story.append(
    step_table(
        [
            "앱/워치끼리는 같은 Wi-Fi에 연결합니다. ESP32가 포함되면 ESP32에서 세이브 보내기 또는 받기를 먼저 누르고 상대 기기를 <b>TamaPoke-XXXX</b>에 연결합니다.",
            "원본은 <b>세이브 보내기</b>, 대상은 <b>세이브 받기</b>를 누릅니다.",
            "양쪽 화면의 <b>6자리 확인 코드</b>가 같은지 확인합니다. 다르면 뒤로 나가 다시 연결합니다.",
            "진행률이 100%가 되고 대상에 <b>받기 완료</b>가 나올 때까지 화면과 Wi-Fi를 유지합니다.",
            "대상에서 <b>세이브 적용</b>을 누르고 덮어쓰기 경고를 읽은 뒤 <b>예</b>를 선택합니다.",
            "재시작 후 포켓몬, 파티, 60칸 박스, 도감, 배지와 설정을 확인합니다.",
        ]
    )
)
story.extend(
    [
        Spacer(1, 5 * mm),
        p("전송 안전 장치", H2),
        p("데이터는 192바이트 조각으로 나누어 재전송하며 전체 저장 구조와 CRC16을 검사합니다. 연결이 끊기거나 손상되면 적용 버튼이 나오지 않아 대상의 기존 세이브가 유지됩니다.", SAFE),
    ]
)
page_break(story)

# 16. Apply and troubleshooting
story.extend(page_heading("15 확인", "세이브 적용 화면과 문제 해결", "덮어쓰기 전 마지막 화면을 확인하고, 문제가 있으면 현재 세이브를 유지하세요."))
apply_image = screenshot(qa_current / "guide-saveconfirm.png", 67 * mm)
apply_text = p(
    "<b>예</b>: 대상 기기의 현재 세이브를 받은 세이브로 교체하고 재시작합니다.<br/><br/>"
    "<b>아니요</b>: 받은 데이터는 적용하지 않고 현재 세이브를 유지합니다.<br/><br/>"
    "확인 코드가 다르거나 예상하지 않은 기기 이름이면 적용하지 마세요."
)
apply_table = Table([[apply_image, apply_text]], colWidths=[80 * mm, 91 * mm], hAlign="LEFT")
apply_table.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("BACKGROUND", (0, 0), (-1, -1), PALE_GRAY), ("BOX", (0, 0), (-1, -1), 0.5, LINE), ("LEFTPADDING", (0, 0), (-1, -1), 8), ("RIGHTPADDING", (0, 0), (-1, -1), 8), ("TOPPADDING", (0, 0), (-1, -1), 8), ("BOTTOMPADDING", (0, 0), (-1, -1), 8)]))
story.append(apply_table)
story.extend(
    [
        Spacer(1, 5 * mm),
        p("빠른 문제 해결", H2),
        info_table(
            [
                ["증상", "확인할 내용"],
                ["상대를 못 찾음", "같은 Wi-Fi, 로컬 권한, TamaPoke-XXXX 연결과 90초 대기 확인"],
                ["전송이 멈춤", "양쪽에서 뒤로 나가 Wi-Fi를 다시 연결하고 처음부터 재시도"],
                ["적용 버튼이 없음", "조각 손실 또는 CRC 오류이므로 기존 세이브는 그대로 유지됨"],
                ["버전 오류", "양쪽을 ko.1.1.6 또는 프로토콜 4 호환 버전으로 업데이트"],
            ],
            [51 * mm, 123 * mm],
        ),
        Spacer(1, 5 * mm),
        p("설치 페이지: https://loaram.github.io/TamaPoke_ko/", SMALL),
        p("릴리스: https://github.com/Loaram/TamaPoke_ko/releases/tag/ko.1.1.6", SMALL),
        p("비공식·비상업 팬 프로젝트 · 코드 MIT · 스프라이트 PMD SpriteCollab (CC BY-NC) · 한글 글꼴 Galmuri11 (SIL OFL 1.1)", SMALL),
    ]
)

OUT.parent.mkdir(parents=True, exist_ok=True)
document = SimpleDocTemplate(
    str(OUT),
    pagesize=A4,
    rightMargin=18 * mm,
    leftMargin=18 * mm,
    topMargin=17 * mm,
    bottomMargin=21 * mm,
    title=f"TamaPoke {VERSION} 한국어 플레이 설명서",
    subject="ESP32 설치, 기본 조작, 근거리 배틀과 세이브 이전",
    author="Loaram / TamaPoke 한국어판",
)
document.build(story, onFirstPage=header_footer, onLaterPages=header_footer)
print(OUT)
