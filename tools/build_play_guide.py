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
OUT = ROOT / "output" / "pdf" / "TamaPoke-2.0.1-Play-Guide-KO.pdf"
VERSION = "2.0.1"
PAGE_TOTAL = 18

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
qa_current = ROOT / "docs" / "qa" / "2.0.0"

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
            "플레이 설명서 · 2.0.1",
            ParagraphStyle("CoverSub", parent=H2, alignment=TA_CENTER, fontSize=18, leading=26, textColor=BLUE),
        ),
        Spacer(1, 8 * mm),
        screenshot(screens / "main.png", 79 * mm),
        Spacer(1, 8 * mm),
        p("Waveshare ESP32-S3-Touch-AMOLED-1.75", CENTER),
        p("Android · Galaxy Watch4~9 연동 안내 포함", CENTER),
        Spacer(1, 10 * mm),
        p("처음 설치부터 탐색·포획 · 육성 · 전투 · 기기간 세이브 이전까지", ParagraphStyle("CoverLine", parent=SMALL, alignment=TA_CENTER)),
        Spacer(1, 8 * mm),
        p("2026-09-05", CENTER),
    ]
)
page_break(story)

# 2. Firmware install
story.extend(page_heading("01 설치", "펌웨어와 그림 팩 준비", "Chrome 또는 Edge에서 공개 설치 페이지를 이용합니다."))
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
        p("세이브를 지키려면 공개 설치 페이지의 설치 버튼을 사용하세요. 통합 <b>tamapoke.bin</b>을 직접 기록하는 방식은 사용하지 마세요.", SAFE),
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
        bullet("먹이는 포만을 채웁니다. 좋아하는 열매는 포만 +35·기쁨 +10, 다른 열매는 포만 +25입니다."),
        bullet("사탕은 포만 +10·기쁨 +12 대신 무게 +12가 오릅니다. 운동하면 무게를 줄일 수 있습니다."),
        bullet("수면은 활력을 회복하고, 목욕은 배설물을 치우며 위생을 100으로 회복합니다."),
        bullet("홈에서 오른쪽으로 밀면 파티, 왼쪽으로 밀면 체육관으로 이동합니다."),
        bullet("PWR를 짧게 누르면 화면을 끄고, 길게 누르면 전원을 끕니다."),
        bullet("화면 위 배터리 그림은 ESP32·휴대전화·워치의 실제 잔량을 따르며 충전 중에는 번개가 표시됩니다."),
        p("화면이 한국어가 아니면 트레이너 설정을 열고 언어 버튼을 눌러 <b>한국어</b>를 선택하세요.", CALLOUT),
    ]
)
page_break(story)

# 5. Growth and care rules
story.extend(page_heading("04 육성", "성장·수면·훈련·작별 규칙", "돌봄과 훈련이 진화, 능력치와 다음 알에 미치는 영향을 확인하세요."))
story.append(
    info_table(
        [
            ["항목", "규칙", "플레이 팁"],
            ["성장", "20분마다 1레벨", "레벨 100은 약 1일 9시간"],
            ["진화", "목표 레벨 + 돌봄 수치 40 이상", "돌봄 실수 1회당 1레벨 지연"],
            ["작별", "알에서 시작 후 24시간 + 최종 진화", "전체 육성 시간 기준 · 레벨 73 무렵"],
            ["수면 활력", "분당 +8", "실시간과 전원 꺼짐 시간 모두 적용"],
            ["조기 돌봄 종료", "다음 진화 지연 없음", "현재 포켓몬은 파티에 남지 않음"],
            ["공격 훈련", "활력 12 · 4회당 +1", "1회 최대 +18"],
            ["방어 훈련", "활력 12 · 점수 2당 +1", "1회 최대 +18"],
            ["스피드 훈련", "활력 10 · 2회당 +1", "1회 최대 +18"],
        ],
        [37 * mm, 57 * mm, 80 * mm],
    )
)
story.extend(
    [
        Spacer(1, 6 * mm),
        p("기기의 시간이 기준입니다", H2),
        p("Android·워치는 기기의 날짜와 시간, ESP32는 설정 시각을 따릅니다. 앱 종료·백그라운드 중에는 오프라인 규칙을 적용해 깨어 있을 때 돌봄 수치가 15 아래로 더 떨어지지 않고 방치 실수가 추가되지 않습니다. 영구 동료는 재접속해도 성장하지 않습니다.", CALLOUT),
        p("작별을 거절하면 하루 뒤 다시 제안합니다. 최종 진화 전, 수면 중, 영구 동료 상태에서는 작별이 열리지 않습니다.", BODY),
        p("올바른 작별과 다음 알", H2),
        p("평소 이로치 알의 기본 확률은 <b>1/48</b>입니다. 알에서 시작한 전체 육성 시간이 24시간 이상이고 현재 최종 진화 상태일 때 정상적으로 작별하면 다음 알은 <b>1/24</b>부터 시작합니다. 최종 진화한 시점부터 다시 24시간을 기다리는 규칙은 아닙니다. 돌봄 연속일수와 유대 보너스까지 좋으면 최고 <b>1/10</b>까지 올라갑니다.", SAFE),
        p("조기 돌봄 종료, 도망 또는 놓아주기는 작별 보너스를 주지 않습니다. 훈련 결과의 +N은 실제 상승량이며, 개체값에 따른 훈련 상한에 도달하면 더 오르지 않습니다.", BODY),
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
        bullet("체육관 전투는 선택한 지방의 관장, 상대 팀과 배지 순서로 끝까지 진행됩니다."),
        p("그림 팩이 설치되지 않은 지방은 잠금으로 표시될 수 있습니다. 먼저 설치 페이지에서 해당 지방 팩을 넣으세요.", SAFE),
    ]
)
page_break(story)

# 7. Moves and evolution
story.extend(page_heading("06 기술", "레벨업과 진화 기술", "새 기술을 배우는 순간과 네 칸의 기술 구성을 관리하는 방법입니다."))
story.append(screenshot_pair(qa_current / "guide-moves.png", qa_current / "guide-profile.png"))
story.extend(
    [
        Spacer(1, 4 * mm),
        p("기술 수록 범위", H2),
        info_table(
            [
                ["구분", "개수", "포함 기준"],
                ["기준 기술", "919개", "원본 데이터의 일반 기술 ID 1~919"],
                ["TamaPoke 수록", "695개", "자연 습득 689 + 호환 기술 5 + 발버둥 1"],
                ["미수록", "224개", "자연 습득 범위 밖의 전용 기술"],
            ],
            [38 * mm, 29 * mm, 107 * mm],
        ),
        Spacer(1, 4 * mm),
        bullet("레벨업으로 새 기술을 배우며, 기술 네 칸이 차 있으면 교체할 기술을 선택합니다."),
        bullet("마스카나의 <b>트릭플라워</b>처럼 진화 순간 배우는 기술은 진화 직후 제안됩니다."),
        bullet("진화 기술을 지나쳤더라도 기술 카드의 레벨 1 목록에서 다시 선택할 수 있습니다."),
        bullet("한번에 여러 레벨이 올라도 기술 제안 대기열이 저장되어 재시작 후 이어집니다."),
        p("미수록 224개는 기술머신·알기술·가르침·이벤트 전용, 특정 폼이나 전투 시스템 전용, 최신 자연 습득표에서 쓰이지 않는 기술입니다. 외전 전용 그림자 기술 18개는 919개 기준에서도 제외했습니다. 복잡한 고유 효과는 기술을 빼지 않고 TamaPoke 전투 규칙에 맞게 단순화합니다.", CALLOUT),
    ]
)
page_break(story)

# 8. Party and box
story.extend(page_heading("07 파티", "현재 동료 포함 6마리와 60칸 박스", "함께 키우는 포켓몬 1마리와 파티 5마리가 한 전투에 참가합니다."))
story.append(screenshot_pair(qa_current / "guide-party.png", screens / "box.png"))
story.extend(
    [
        Spacer(1, 5 * mm),
        bullet("파티에는 5마리를 보관할 수 있습니다. 홈에서 키우는 현재 포켓몬까지 합쳐 전투 후보는 최대 6마리입니다."),
        bullet("포켓몬 칸을 눌러 위치를 바꾸거나 상세 정보, 기술, 훈련 상태를 확인합니다."),
        bullet("박스 안에서는 좌우로 밀어 1/10부터 10/10까지 이동합니다."),
        bullet("파티가 가득 찬 상태에서 새 포켓몬이 합류하면 파티 교체 화면에서 보낼 자리를 선택합니다."),
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
        bullet("공격·방어 훈련은 활력 12, 스피드 훈련은 활력 10을 사용합니다. 모두 한 번에 최대 +18이며 결과 화면의 +N은 실제 상승량입니다."),
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
        p("한 지방은 관장 8명 → 사천왕 4명 → 챔피언 순서로 진행합니다. 승리할 때마다 다음 상대가 열리며, 지방별 진행도와 배지는 따로 저장됩니다.", SAFE),
        p("도전 전에 파티 순서, 남은 활력과 기술 타입을 확인하세요. 어려운 상대는 훈련과 기술 교체 후 다시 도전할 수 있습니다.", BODY),
    ]
)
page_break(story)

# 11. Explore overview
story.extend(page_heading("10 탐색", "야생 포켓몬을 만나는 방법", "메뉴에서 탐색을 열고 지방과 탐색 방식을 선택합니다. 조우가 시작될 때 활력 30을 사용합니다."))
story.append(screenshot_pair(qa_current / "guide-explore.png", qa_current / "guide-wild.png"))
story.extend(
    [
        Spacer(1, 4 * mm),
        step_table(
            [
                "홈에서 포켓몬 이름을 눌러 메뉴를 열고 <b>탐색</b>을 선택합니다.",
                "찾고 싶은 포켓몬이 속한 지방을 고릅니다.",
                "일반 또는 랜덤을 고릅니다. 시작 순간 활력 30이 차감되며 승패와 관계없이 돌려받지 않습니다.",
                "야생 포켓몬을 쓰러뜨리면 몬스터볼 포획 판정이 한 번 진행됩니다.",
                "포획에 성공하면 빈 파티 칸에 들어가고, 파티가 찼으면 박스의 빈 칸으로 이동합니다.",
            ]
        ),
        Spacer(1, 4 * mm),
        p("탐색 시작 조건", H2),
        p("현재 포켓몬이 알·수면·작별 연출 상태가 아니어야 하며 활력이 30 이상이어야 합니다. 포획할 자리를 위해 파티 또는 60칸 박스에 빈 칸도 하나 이상 필요합니다.", CALLOUT),
    ]
)
page_break(story)

# 12. Explore odds
story.extend(page_heading("11 탐색", "조우 레벨·종류·포획 확률", "모든 확률은 탐색 1회를 시작했을 때 또는 전투에서 승리했을 때 각각 한 번 판정됩니다."))
story.append(
    info_table(
        [
            ["선택", "야생 포켓몬 레벨", "언제 쓰면 좋나요?"],
            ["일반", "현재 키우는 포켓몬 레벨 -5~+5", "비슷한 수준의 상대와 안정적으로 전투"],
            ["랜덤", "레벨 1~100에서 같은 확률", "낮거나 매우 높은 레벨도 감수하고 도전"],
        ],
        [34 * mm, 62 * mm, 78 * mm],
    )
)
story.extend(
    [
        Spacer(1, 4 * mm),
        p("레벨 범위는 1 아래나 100 위로 넘어가지 않습니다. 예를 들어 현재 포켓몬이 레벨 60이면 일반 탐색은 레벨 55~65, 레벨 3이면 1~8입니다. 파티 평균이나 가장 높은 파티원의 레벨은 사용하지 않습니다.", SAFE),
        p("조우 종류 확률", H2),
        info_table(
            [
                ["일반 포켓몬", "진화한 포켓몬", "희귀 포켓몬", "전설 포켓몬"],
                ["70%", "22%", "7%", "1%"],
            ],
            [43.5 * mm, 43.5 * mm, 43.5 * mm, 43.5 * mm],
        ),
        Spacer(1, 4 * mm),
        p("먼저 위 종류를 뽑고, 선택한 지방에서 그림이 준비된 해당 종류의 포켓몬 중 하나를 같은 확률로 고릅니다. 지방별 후보 수가 달라 특정 포켓몬의 최종 조우 확률도 달라집니다.", BODY),
        p("승리 뒤 포획 확률", H2),
        info_table(
            [
                ["포획률 값·예시", "1회 승리 시 확률", "설명"],
                ["255 · 캐터피", "약 99.99%", "거의 반드시 포획"],
                ["45 · 이상해씨", "약 16.78%", "대략 6번 승리당 1번 수준"],
                ["3 · 뮤츠", "2.5%", "정식 게임식 약 0.83% 대신 완화한 고정값"],
            ],
            [46 * mm, 40 * mm, 88 * mm],
        ),
        Spacer(1, 3 * mm),
        p("몬스터볼 1배, 상대 HP 10%, 상태이상 보너스 없음 조건의 포획식을 사용합니다. 볼 아이템이나 추가 던지기는 없으며, 실패하면 다음 탐색에서 다시 만나 승리해야 합니다.", CALLOUT),
    ]
)
page_break(story)

# 13. LAN overview
story.extend(page_heading("12 근거리 배틀", "어떤 기기끼리 연결할 수 있나요?", "ESP32, Android와 Wear OS는 가까운 곳의 로컬 네트워크로 대전합니다."))
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
        bullet("ESP32의 Wi-Fi는 인터넷 연결이 없어도 정상입니다. 인터넷 없음 안내에서 연결 유지를 선택하세요."),
        bullet("상대를 찾는 최초 제한 시간은 최대 약 90초입니다."),
        bullet("앱이나 워치에서 권한 안내가 나오면 로컬 네트워크 접근을 허용합니다."),
    ]
)
page_break(story)

# 14. LAN with ESP
story.extend(page_heading("13 근거리 배틀", "ESP32와 Android/워치 연결", "ESP32가 Wi-Fi 방을 만든 뒤 상대 기기가 그 방에 참가합니다."))
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

# 15. LAN app pairs
story.extend(page_heading("14 근거리 배틀", "앱과 앱, 워치끼리 연결", "두 기기를 같은 Wi-Fi 공유기에 연결한 상태에서 진행합니다."))
story.append(
    step_table(
        [
            "두 기기를 같은 Wi-Fi에 연결하고 TamaPoke를 실행합니다.",
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

# 16. Save transfer overview
story.extend(page_heading("15 세이브 이전", "보내기와 받기 전에 확인", "세이브 전송은 원본을 복사해 대상 기기의 저장을 교체하는 기능입니다."))
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

# 17. Save transfer steps
story.extend(page_heading("16 세이브 이전", "세이브 보내기, 확인, 적용", "연결 준비는 근거리 배틀과 같고 메뉴에서 보내기와 받기를 선택합니다."))
story.append(
    step_table(
        [
            "앱/워치끼리는 같은 Wi-Fi에 연결합니다. ESP32가 포함되면 ESP32에서 세이브 보내기 또는 받기를 먼저 누르고 상대 기기를 <b>TamaPoke-XXXX</b>에 연결합니다.",
            "원본은 <b>세이브 보내기</b>, 대상은 <b>세이브 받기</b>를 누릅니다.",
            "양쪽 화면의 <b>6자리 확인 코드</b>가 같은지 확인합니다. 다르면 뒤로 나가 다시 연결합니다.",
            "진행률이 100%가 되고 대상에 <b>받기 완료</b>가 나올 때까지 화면과 Wi-Fi를 유지합니다.",
            "대상에서 <b>세이브 적용</b>을 누르고 덮어쓰기 경고를 읽은 뒤 <b>예</b>를 선택합니다.",
            "ESP32는 재시작을 기다립니다. 앱·워치는 닫힌 앱을 다시 엽니다. 포켓몬, 파티, 박스, 도감과 배지를 확인합니다.",
        ]
    )
)
story.extend(
    [
        Spacer(1, 5 * mm),
        p("전송 안전 장치", H2),
        p("전송 중 빠진 데이터가 있으면 자동으로 다시 보내고, 완료 뒤 저장이 온전한지 검사합니다. 연결이 끊기거나 데이터가 손상되면 적용 버튼이 나오지 않아 대상의 세이브가 그대로 유지됩니다.", SAFE),
    ]
)
page_break(story)

# 18. Apply and troubleshooting
story.extend(page_heading("17 확인", "세이브 적용 화면과 문제 해결", "덮어쓰기 전 마지막 화면을 확인하고, 문제가 있으면 현재 세이브를 유지하세요."))
apply_image = screenshot(qa_current / "guide-saveconfirm.png", 67 * mm)
apply_text = p(
    "<b>예</b>: 받은 세이브로 교체합니다. ESP32는 재시작하며, Android·워치는 앱이 닫히면 직접 다시 열어 주세요.<br/><br/>"
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
                ["적용 버튼이 없음", "받은 데이터가 완전하지 않으므로 대상 세이브는 그대로 유지됨"],
                ["버전 오류", "양쪽 기기에 설치 페이지의 최신 앱 또는 펌웨어를 설치하고 재시작"],
            ],
            [51 * mm, 123 * mm],
        ),
        Spacer(1, 5 * mm),
        p("설치 페이지: https://loaram.github.io/TamaPoke_ko/", SMALL),
        p("릴리스: https://github.com/Loaram/TamaPoke_ko/releases/tag/2.0.1", SMALL),
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
    subject="처음 설치, 탐색과 포획, 기본 조작, 근거리 배틀과 세이브 이전",
    author="Loaram / TamaPoke 한국어판",
)
document.build(story, onFirstPage=header_footer, onLaterPages=header_footer)
print(OUT)
