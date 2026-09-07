from __future__ import annotations

from pathlib import Path
from capture_guide_data import capture_rows

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
OUT = ROOT / "output" / "pdf" / "TamaPoke-3.6.0-Play-Guide-KO.pdf"
VERSION = "3.6.0"
PAGE_TOTAL = 29

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
            "플레이 설명서 · 3.6.0",
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
        p("2026-09-07", CENTER),
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
                "원하는 지방과 <b>폼체인지 팩</b>을 선택하거나 <b>지방·폼 전체 설치</b>를 누릅니다.",
                "완료 메시지가 나올 때까지 페이지를 열어 두고 케이블과 SD 카드를 빼지 않습니다.",
                "<b>연결 해제</b>를 누른 뒤 ESP32를 재시작합니다.",
            ]
        ),
        Spacer(1, 4 * mm),
        p("관동 팩 하나도 약 10~15분 걸릴 수 있습니다. 없는 지방 팩의 포켓몬은 빈 그림으로 나오지 않도록 해당 도감과 알 후보에서 잠깁니다.", CALLOUT),
        p("ESP32의 microSD는 그림뿐 아니라 300칸 보관 기록도 저장합니다. 업데이트 전 전체 세이브를 백업하고, 사용 중 SD를 빼거나 포맷하지 마세요. 카드 교체 시 세이브 복원이 필요합니다.", DANGER),
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
        bullet("일반 알은 아직 덜 모은 진화 계보를 우선합니다. <b>이로치 알은 도감 등록 여부와 무관하게</b> 선택 지방·희귀도에서 뽑으므로, 파이리 계보를 모두 모았어도 이로치 파이리가 다시 나올 수 있습니다. 이미 생성된 알은 다시 뽑지 않습니다."),
        bullet("포켓몬을 짧게 누르면 쓰다듬고, 이름을 누르면 상세 메뉴가 열립니다."),
        bullet("먹이는 포만을 채웁니다. 좋아하는 열매는 포만 +35·기쁨 +10, 다른 열매는 포만 +25입니다."),
        bullet("사탕은 포만 +10·기쁨 +12 대신 무게 +12가 오릅니다. 운동하면 무게를 줄일 수 있습니다."),
        bullet("수면은 활력을 회복합니다. 앱을 닫아도 직접 재운 상태가 유지되며, 다시 눌러 깨울 수 있습니다. 목욕은 배설물을 치우고 위생을 100으로 회복합니다."),
        bullet("홈에서 오른쪽으로 밀면 파티, 왼쪽으로 밀면 체육관으로 이동합니다."),
        bullet("PWR를 짧게 누르면 화면을 끄고, 길게 누르면 전원을 끕니다."),
        bullet("화면 위 배터리 그림은 ESP32·휴대전화·워치의 실제 잔량을 따르며 충전 중에는 번개가 표시됩니다."),
        p("트레이너 설정에서 언어·소리·음량을 조절합니다. 선택한 소리 켜기/끄기와 음량은 앱을 다시 실행해도 유지됩니다. 한국어가 아니면 언어 버튼에서 <b>한국어</b>를 선택하세요.", CALLOUT),
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
            ["좋은 작별", "부화 후 성장 24시간 + 일반 최종 진화", "성장 1,440분 · 레벨 73"],
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
        p("Android·워치는 기기의 날짜와 시간, ESP32는 설정 시각을 따릅니다. 앱 종료·백그라운드 중에는 오프라인 규칙을 적용해 깨어 있을 때 돌봄 수치가 15 아래로 더 떨어지지 않고 방치 실수가 추가되지 않습니다. 홈으로 데려온 동료도 이 시간 기준으로 성장합니다.", CALLOUT),
        p("작별을 거절하면 하루 뒤 다시 제안합니다. 최종 진화 전, 수면 중, 영구 동료 상태에서는 작별이 열리지 않습니다.", BODY),
        p("좋은 작별과 두 가지 알 보너스", H2),
        p("좋은 작별은 <b>부화 이후 실제 누적 성장 24시간</b>과 일반 최종진화가 조건입니다. 최종진화 후 24시간을 더 기다리는 뜻이 아니며 폼체인지도 필수가 아닙니다. 사용자가 작별을 선택하면 파티 우선, 가득 차면 박스에 보관합니다.", SAFE),
        p("알 보너스는 <b>연속 돌봄 10일과 수집 가능 도감 50%</b>를 모두 달성하면 최대 이로치 15%·전설 21%입니다. 좋은 작별·일반 작별·현재 포켓몬 보내기는 합쳐 하루 3회이며 자정에 초기화됩니다. 자세한 확률은 21쪽에서 확인하세요.", BODY),
    ]
)
page_break(story)

# 6. Dex and gyms
story.extend(page_heading("05 도감", "1025종 도감과 9개 지방 체육관", "관동부터 팔데아까지 모험하고, 가라르에서는 소드·실드 코스를 선택합니다."))
story.append(screenshot_pair(ROOT / "docs/qa/2.0.2/guide-gallery.png", ROOT / "docs/qa/3.5.1/gymsj.png"))
story.extend(
    [
        Spacer(1, 5 * mm),
        bullet("도감은 관동 · 성도 · 호연 · 신오 · 하나 · 칼로스 · 알로라 · 가라르 · 팔데아를 지원합니다."),
        bullet("도감 1/1025(982)는 등록 1종 / 전체 번호 1025종 / 현재 수집 가능 982종입니다. 괄호는 알·진화·탐색으로 모을 수 있는 종수이며, 지역별로도 표시합니다."),
        bullet("그림 미지원 43종은 수집 대상에서 제외됩니다. 괄호는 전체 팩 기준이며 설치한 팩 수나 기존 도감 기록을 바꾸지 않습니다."),
        bullet("도감 목록은 좌우로 페이지, 위아래로 지방을 바꿉니다. 포켓몬을 누르면 상세 정보를 볼 수 있습니다."),
        bullet("관동~알로라와 팔데아는 관장 8명, 사천왕 4명, 챔피언 1명 순서입니다. 가라르는 원작에 사천왕이 없어 챔피언 토너먼트 상대를 배치했습니다."),
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
                ["TamaPoke 수록", "717개", "자연 습득·호환·폼 전용·발버둥"],
                ["미수록", "202개", "현재 지원 습득표·전투 규칙 범위 밖"],
            ],
            [38 * mm, 29 * mm, 107 * mm],
        ),
        Spacer(1, 4 * mm),
        bullet("레벨업으로 새 기술을 배우며, 기술 네 칸이 차 있으면 교체할 기술을 선택합니다."),
        bullet("마스카나의 <b>트릭플라워</b>처럼 진화 순간 배우는 기술은 진화 직후 제안됩니다."),
        bullet("진화 기술을 지나쳤더라도 기술 카드의 레벨 1 목록에서 다시 선택할 수 있습니다."),
        bullet("한번에 여러 레벨이 올라도 기술 제안 대기열이 저장되어 재시작 후 이어집니다."),
        p("미수록 202개는 현재 지원하지 않는 기술머신·알기술·가르침·이벤트 또는 전투 시스템 전용 기술 등입니다. 외전 그림자 기술 18개는 919개 기준에서도 제외합니다. 폼별 습득표 69개와 전용 기술이 포함되지만, 모든 원작 특성·필드·PP 효과를 재현하는 것은 아닙니다.", CALLOUT),
    ]
)
page_break(story)

# 8. Party and box
story.extend(page_heading("07 파티", "현재 동료 포함 6마리와 300칸 박스", "함께 키우는 포켓몬 1마리와 파티 5마리가 한 전투에 참가합니다."))
story.append(screenshot_pair(ROOT / "docs/qa/3.1.0/companion-box-ivs.png", ROOT / "docs/qa/daily-rewards/box-picker.png"))
story.extend(
    [
        Spacer(1, 5 * mm),
        bullet("파티에는 5마리를 보관할 수 있습니다. 홈에서 키우는 현재 포켓몬까지 합쳐 전투 후보는 최대 6마리입니다."),
        bullet("박스의 포켓몬을 누르면 상세창이 열립니다. <b>놓아주기 → 예</b>로 바로 정리하며 파티를 거칠 필요가 없습니다. 파티/박스 놓아주기는 하루 작별 3회와 별개입니다."),
        bullet("파티에서 교체 대상을 먼저 골랐어도 상세창이 열립니다. 가운데 <b>교체</b> 버튼을 눌러야 실제로 바뀝니다. 교체 대상이 없으면 <b>파티로</b>로 이동합니다."),
        bullet("박스는 6칸씩 50페이지입니다. 아래 <b>&lt; / &gt;</b> 버튼이나 가로 스와이프로 이동합니다. 페이지 번호를 누르면 10페이지씩 묶인 목록에서 원하는 번호로 바로 갑니다."),
        bullet("좋은 작별·포획으로 합류하면 파티 우선, 파티가 가득 차면 박스로 들어갑니다. 둘 다 가득 찼을 때 보관 대상을 선택합니다."),
        bullet("파티/박스 상세창의 <b>데려오기</b>로 현재 키우는 포켓몬과 자유롭게 맞교환합니다. 성장 개체는 보관 중 시간이 멈추고 다시 데려오면 이어집니다."),
        bullet("전체 세이브 이전에는 현재 포켓몬, 300칸 박스, 폼·기술, 도감·배지, 연속일수·작별 사용 횟수가 함께 들어갑니다."),
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
        bullet("현재 포켓몬이 쓰러지면 다음 순서부터 파티 전체를 돌아 살아 있는 동료를 내보냅니다. 끝 칸이어도 앞쪽 동료가 남아 있으면 계속 싸우며, 쓰러진 동료는 건너뜁니다."),
        bullet("타입 상성에 따라 효과가 굉장함, 별로임 또는 없음으로 표시됩니다."),
        p("근거리 배틀은 다른 사용자의 파티와 연결하는 기능입니다. 다음 장의 네트워크 준비를 먼저 완료해야 합니다.", CALLOUT),
    ]
)
page_break(story)

# 10. Gym regional behavior
story.extend(page_heading("09 체육관", "지방별 관장과 배지 진행", "왼쪽 화면에서 지방과 상대를 고르고 준비된 파티로 도전합니다."))
story.append(screenshot_pair(ROOT / "docs/qa/2.0.2/guide-gallery.png", ROOT / "docs/qa/3.5.1/gymsj.png"))
story.extend(
    [
        Spacer(1, 5 * mm),
        step_table(
            [
                "체육관 지방 선택창에서 도전할 지방을 누릅니다.",
                "상대 목록은 좌우로 페이지를 넘기고, 위아래로 밀면 지방을 바꿉니다.",
                "상대 이름과 지방을 확인한 뒤 도전을 시작합니다.",
                "승리하면 선택한 지방의 다음 상대와 배지가 열립니다.",
            ]
        ),
        Spacer(1, 5 * mm),
        p("팔데아까지 9개 지방의 진행도와 배지는 따로 저장됩니다. 가라르는 소드·실드에서 다른 관장도 각각 기록합니다. 관장 이후에는 사천왕 또는 가라르 챔피언 토너먼트를 거쳐 마지막 상대에게 도전합니다.", SAFE),
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
                "별도 결과창에서 포획 성공·실패와 보관 위치를 확인합니다. <b>확인</b>을 누르면 탐색으로 돌아갑니다. 자세한 화면은 25쪽을 보세요.",
            ]
        ),
        Spacer(1, 4 * mm),
        p("탐색 시작 조건", H2),
        p("현재 포켓몬이 알·수면·작별 연출 상태가 아니어야 하며 활력이 30 이상이어야 합니다. 포획할 자리를 위해 파티 또는 300칸 박스에 빈 칸도 하나 이상 필요합니다.", CALLOUT),
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
        p("<b>탐색 이로치 확률은 1/100(1%)</b>이며 일반·랜덤 탐색에 공통입니다. 종류와 별도로 판정하며 연속 돌봄·도감 보너스는 붙지 않습니다. 이로치라도 승리 후 포획 판정은 따로 진행합니다.", SAFE),
        p("승리 뒤 포획 확률", H2),
        info_table(
            [
                ["포획률 값·예시", "1회 승리 시 확률", "설명"],
                ["255 · 캐터피", "약 99.99%", "거의 반드시 포획"],
                ["45 · 이상해씨", "약 16.78%", "대략 6번 승리당 1번 수준"],
                ["3 · 뮤츠", "기본 2.5%", "완화 보정에도 도감 볼 배율 적용"],
            ],
            [46 * mm, 40 * mm, 88 * mm],
        ),
        Spacer(1, 3 * mm),
        p("표는 도감 0~99종의 기본 확률입니다. 상대 HP 10%·상태이상 없음 기준이며, 수집 가능 도감 100종마다 볼 배율이 0.2씩 증가합니다. 단계별 다섯 종의 확률표는 <b>29쪽</b>을 보세요. 볼 아이템이나 추가 던지기는 없습니다.", CALLOUT),
    ]
)
page_break(story)

# 13. LAN overview
story.extend(page_heading("12 근거리 배틀", "어떤 기기끼리 연결할 수 있나요?", "ESP32, Android와 Wear OS는 가까운 곳의 로컬 네트워크로 대전합니다."))
story.append(
    info_table(
        [
            ["조합", "연결 방식", "먼저 할 일"],
            ["ESP32 ↔ ESP32", "ESP-NOW 직접 연결", "두 기기에서 통신 메뉴 열기"],
            ["Android ↔ Android", "같은 Wi-Fi의 UDP", "두 앱을 같은 공유기에 연결"],
            ["Android ↔ Wear OS", "같은 Wi-Fi의 UDP", "휴대전화와 워치를 같은 공유기에 연결"],
            ["ESP32 ↔ 앱/워치", "ESP32 Wi-Fi 방의 UDP", "ESP32에서 먼저 통신 메뉴 열기"],
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
story.append(screenshot(ROOT / "docs/qa/3.5.1/lan.png", 74 * mm))
story.extend(
    [
        Spacer(1, 4 * mm),
        step_table(
            [
                "ESP32에서 트레이너 메뉴의 <b>통신 메뉴</b>를 엽니다.",
                "<b>방 만들기</b> 또는 <b>참가하기</b>를 누르고 사용할 파티를 선택합니다.",
                "ESP32 화면의 <b>TamaPoke-XXXX</b>를 확인합니다.",
                "Android 또는 워치를 해당 Wi-Fi에 연결하고 암호 <b>tamapoke</b>를 입력합니다.",
                "앱 또는 워치에서도 통신 메뉴를 열고 방 선택과 파티 선택을 진행합니다.",
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
            "통신 메뉴를 처음 열 때 로컬 네트워크 권한이 나오면 허용합니다.",
            "양쪽에서 트레이너 메뉴의 <b>통신 메뉴</b>를 엽니다.",
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
story.append(screenshot_pair(ROOT / "docs/qa/3.5.1/lan.png", qa_current / "guide-savereceive.png"))
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
        p("릴리스: https://github.com/Loaram/TamaPoke_ko/releases/tag/3.6.0", SMALL),
        p("비공식·비상업 팬 프로젝트 · 코드 MIT · 스프라이트 PMD SpriteCollab (CC BY-NC) · 한글 글꼴 Galmuri11 (SIL OFL 1.1)", SMALL),
    ]
)

page_break(story)
story.extend(page_heading("18 폼체인지", "원하는 모습으로 함께 지내기", "해금한 폼은 직접 선택하며, 전투가 끝나도 그 모습이 유지됩니다."))
story.append(screenshot_pair(ROOT / "docs/qa/forms-runtime/mega.png", ROOT / "docs/qa/forms-runtime/eternamax.png"))
story.extend([
    Spacer(1, 4*mm),
    info_table([["분류", "해금", "예시"], ["일반 폼", "60레벨", "지방의 모습·오거폰 가면·타입 변화"], ["메가진화", "70레벨", "지원하는 메가진화 폼"], ["강력한 폼", "80레벨", "합체·원시회귀·무한다이맥스 등"]], [40*mm,35*mm,99*mm]),
    Spacer(1, 4*mm),
    bullet("현재 포켓몬의 카드 또는 파티·박스 상세창에서 <b>폼체인지</b>를 엽니다. 화살표로 모습을 보고 필요한 레벨을 확인한 뒤 선택합니다."),
    bullet("현재 레벨만 확인하며 별도 아이템·유대·추가 24시간 조건은 없습니다. 합체도 재료 포켓몬을 소모하지 않습니다."),
    bullet("선택한 폼의 타입과 기본 능력치를 상태 표시와 전투에 반영합니다. 기본 모습으로는 언제든 돌아올 수 있습니다."),
    bullet("일반 다이맥스의 단순 확대는 없고, 지원하는 거다이맥스·무한다이맥스 모습만 있습니다. 애니메이션이 없는 폼은 선택 목록에서 제외합니다."),
    p("폼을 해금하거나 고르는 것은 좋은 작별의 필수 조건이 아닙니다. 일반 진화 계통의 최종진화 여부로 판정합니다.", SAFE),
])

page_break(story)
story.extend(page_heading("19 폼 기술", "새 기술의 교체 칸 직접 선택", "폼에 따라 기술 목록이 달라질 수 있지만 기존 기술을 임의로 덮어쓰지 않습니다."))
story.append(screenshot(ROOT / "docs/qa/form-moves-box/form-move-choice.png", 76*mm))
story.extend([
    Spacer(1, 5*mm),
    step_table(["폼을 바꾸면 그 폼에서 새로 배울 수 있는 기술이 차례로 표시됩니다.", "새 기술을 배울 때는 바꿀 기술 칸을 직접 누릅니다. 빈 칸이 있더라도 원하는 칸을 선택합니다.", "지금 배우지 않으려면 <b>배우지 않기</b>를 누릅니다. 뒤로 나가면 남은 제안은 닫힙니다.", "지나친 기술은 현재 폼의 기술 선택 목록에서 다시 고를 수 있습니다. 같은 폼을 다시 선택해 전용 기술 제안을 확인할 수도 있습니다."]),
    Spacer(1, 4*mm),
    bullet("로토무 가전 폼, 큐레무 합체, 우라오스 등에는 폼별 습득표가 적용됩니다. 지원하는 별도 목록은 69개입니다."),
    bullet("일반 기술은 폼을 바꿔도 저장된 칸에 남습니다. 로토무 가전 전용 기술처럼 특정 폼에서만 쓸 수 있는 기술은 다른 폼에서 사용할 수 없습니다. 새 폼에 맞는 기술로 교체하세요."),
    p("타입 변화 기술의 실제 타입도 폼을 따릅니다. 원작의 모든 특성·필드·PP·전용 연출까지 재현하는 것은 아니며, 전투는 TamaPoke의 단일 전투 규칙으로 진행됩니다.", CALLOUT),
])

page_break(story)
story.extend(page_heading("20 돌봄과 수집", "연속 10일 + 도감 50% 알 보너스", "기존 돌봄 보너스에 도감 보너스를 더하며, 두 목표를 채워야 최대치입니다."))
story.append(screenshot_pair(ROOT / "docs/qa/3.2.0/egg-bonus-dex-only.png", ROOT / "docs/qa/3.2.0/egg-bonus-max.png", 48*mm))
story.extend([Spacer(1, 3*mm),p("왼쪽: 도감 목표만 달성 / 오른쪽: 연속 10일과 도감 목표 모두 달성", SMALL)])
story.append(info_table([
    ["연속 돌봄", "수집 가능 도감률", "이로치 알", "전설 등급"],
    ["0~1일", "0% (기본)", "약 2.08%", "3.00%"],
    ["10일 이상", "0% (돌봄만)", "10.00%", "14.00%"],
    ["0~1일", "50% 이상 (도감만)", "10.00%", "14.00%"],
    ["5일", "50% 이상", "약 12.22%", "약 17.11%"],
    ["10일 이상", "50% 이상 (둘 다)", "15.00%", "21.00%"],
], [38*mm,62*mm,37*mm,37*mm]))
story.extend([
    Spacer(1, 3*mm),
    bullet("도감률은 <b>등록한 수집 가능 종 / 전체 수집 가능 종</b>입니다. 현재 982종 중 <b>491종(50%)</b>이 목표이며 미지원 43종은 제외합니다. 폼·이로치·중복 개체는 같은 종으로 한 번만 셉니다."),
    bullet("돌봄 0~1일은 기본, 2~10일은 점진적으로 증가합니다. 도감은 0~50%에서 점진적으로 증가하며 이후 추가 상승은 없습니다. 중간 확률은 두 진행도를 함께 반영합니다."),
    bullet("먹이·목욕·쓰다듬기·훈련을 한 날짜를 하루 한 번 기록합니다. 앱만 켜거나 알만 두드리면 인정되지 않습니다. 다시 데려온 동료의 돌봄도 인정합니다."),
    bullet("돌보지 않은 날이 하루 통째로 지나면 <b>연속 보너스만</b> 사라지고 도감 보너스는 남습니다. 교체·놓아주기는 도감 등록을 지우지 않습니다."),
    p("표는 보너스 계산 비교입니다. <b>전설은 기존처럼 도감 25종 등록 후</b> 추첨하므로 실제 도감 0종에서는 전설 0%입니다. 지역에 전설 후보가 없거나 도망 직후라면 전설이 나오지 않을 수 있습니다.", CALLOUT),
    bullet("화면의 '다음 알'은 새 알의 확률이며 이미 생긴 알은 다시 추첨하지 않습니다. 탐색 조우·이로치에는 알 보너스가 붙지 않습니다. 도감 등록 수에 따른 탐색 포획 보너스는 별도 규칙으로 29쪽에서 설명합니다."),
    p("<b>좋은 작별·일반 작별·현재 개체 보내기는 합쳐 하루 3회</b>입니다. 현지 자정에 초기화하며 교체와 보관 개체 정리는 차감하지 않습니다.", SAFE),
])

page_break(story)
story.extend(page_heading("21 보관과 안전", "자유 교체와 세이브를 지키는 방법", "현재 포켓몬과 보관 개체의 교환은 알이 아닐 때도 가능합니다."))
story.extend([
    step_table(["파티나 박스에서 데려올 포켓몬을 누릅니다.", "상세창의 <b>데려오기</b>를 누르면 현재 포켓몬은 그 보관 칸으로 들어오고 선택한 포켓몬이 홈으로 옵니다.", "키우던 개체의 성장 분·폼·기술·개체값·훈련·돌봄·유대는 그대로 보존됩니다. 보관 중에는 성장과 돌봄 시간이 흐르지 않습니다.", "포획한 개체나 좋은 작별로 남은 동료도 홈으로 데려오면 현재 레벨에서 성장하고 조건을 만족하면 진화합니다. 보관 중에는 멈춥니다."]),
    Spacer(1, 5*mm),
    p("ESP32의 SD 카드", H2),
    bullet("300칸 보관 기록은 SD에 기기별 이중 저장합니다. 기존 NVS 파티션을 바꾸지 않고 이전 저장을 읽습니다. Android·워치·PC는 기존 내부 저장 방식을 사용합니다."),
    bullet("게임 중 SD를 빼거나 기록 파일을 지우지 마세요. 카드가 없거나 쓰기에 실패하면 보관 변경이 거부될 수 있습니다. 원래 카드를 다시 장착하고 재시작하세요."),
    bullet("SD 교체 전에는 원래 카드가 있는 상태에서 전체 세이브를 백업하거나 다른 기기로 보냅니다. 새 카드에 그림 팩을 설치한 뒤 전체 세이브를 복원하세요."),
    p("업데이트와 기기 간 이전", H2),
    bullet("양쪽 기기를 같은 최신 버전으로 맞추세요. 이전 저장은 읽지만 새 저장을 구버전으로 되돌리는 것은 지원하지 않습니다."),
    bullet("작별 진행·보관 대기와 하루 사용 횟수도 저장됩니다. 앱을 다시 열었을 때 대기 중인 작별이 이어져도 새 작별 횟수를 추가로 쓰는 것은 아닙니다."),
    bullet("전체 세이브를 적용하는 동안 전원을 끄거나 SD를 빼지 마세요. 별도의 원본 백업을 보관한 뒤 진행합니다."),
    p("폰·워치 앱을 삭제하면 내부 세이브도 지워집니다. 설치 오류가 나더라도 먼저 기존 앱을 삭제하지 말고, 같은 서명 키의 APK인지 확인하고 세이브를 백업하세요.", DANGER),
])

page_break(story)
story.extend(page_heading("22 박스 정렬", "300칸을 원하는 순서로 정리하기", "박스 전체에 한 번 적용합니다. 파티와 현재 키우는 포켓몬은 바뀌지 않습니다."))
story.append(screenshot_pair(ROOT / "docs/qa/3.1.0/box-sort-menu.png", ROOT / "docs/qa/3.1.0/box-sort-confirm.png"))
story.extend([
    Spacer(1, 4*mm),
    step_table(["파티에서 <b>박스</b>를 열고 아래쪽 <b>정렬</b>을 누릅니다. 어느 페이지에서 열어도 300칸 전체가 대상입니다.", "<b>가나다순 · 번호순 · 레벨순</b> 중 원하는 기준을 고릅니다. 확인창에서 <b>예</b>를 누르면 적용하고 첫 페이지로 돌아옵니다.", "<b>아니요</b>는 정렬 메뉴로 돌아갑니다. 정렬 메뉴에서 뒤로 가면 원래 페이지와 교체 선택을 그대로 유지합니다."]),
    Spacer(1, 3*mm),
    info_table([["기준", "정렬 규칙"], ["가나다순", "공식 한국어 종 이름 기준. 별명과 폼 이름은 제외"], ["번호순", "작은 전국도감 번호부터 큰 번호 순"], ["레벨순", "높은 레벨부터 낮은 레벨 순"]], [40*mm,134*mm]),
    Spacer(1, 3*mm),
    bullet("빈칸은 뒤로 모읍니다. 가나다·번호가 같으면 레벨 높은 순, 레벨이 같으면 도감 번호순입니다. 기준이 모두 같으면 기존 순서를 유지합니다."),
    bullet("새로 포획하거나 보관한 포켓몬은 자동 정렬하지 않습니다. 필요할 때 다시 정렬하세요. 정렬을 적용하면 이전 교체 선택은 취소되므로 대상을 다시 고르세요."),
    p("개체의 별명·이로치·폼·기술·개체값·훈련·돌봄 기록은 그대로 보존됩니다. 정렬 실패가 표시되면 저장 공간을 확인하고, ESP는 원래 SD를 장착한 뒤 재시작하세요. 이전 칸 배치로 되돌리는 버튼은 없습니다.", SAFE),
])

page_break(story)
story.extend(page_heading("23 동료 육성·개체값", "다시 데려온 동료도 함께 성장", "포획했거나 좋은 작별로 남은 포켓몬도 키우는 자리에서 성장할 수 있습니다."))
story.append(screenshot_pair(ROOT / "docs/qa/3.1.0/companion-party-ivs.png", ROOT / "docs/qa/3.1.0/companion-box-ivs.png"))
story.extend([
    Spacer(1, 4*mm),
    step_table(["파티나 박스에서 포켓몬을 누르고 <b>데려오기</b>를 선택합니다. 현재 포켓몬과 자리를 맞바꿉니다.", "홈에서 함께 지내면 현재 레벨부터 <b>20분마다 1레벨</b>씩 성장합니다. 60레벨은 20분 후 61레벨, 좋은 작별 후 73레벨 동료는 20분 후 74레벨이 됩니다.", "일반 진화 조건을 만족하면 진화할 수 있습니다. 필요한 레벨과 돌봄 상태를 갖추고 진화를 선택하세요. 최대 레벨은 100입니다."]),
    Spacer(1, 3*mm),
    bullet("파티·박스 안에서는 성장하지 않습니다. 다시 보관했다 데려와도 진행 중이던 성장 분·기술·폼·개체값·훈련 기록이 이어집니다."),
    bullet("포획·좋은 작별 동료의 기존 보호는 그대로입니다. 도망가지 않고 다시 좋은 작별이나 일반 작별 대상이 되지 않습니다. 필요하면 파티·박스에서 놓아줄 수 있습니다."),
    p("능력치 옆 괄호 읽기", H2),
    p("<b>공격 99(31)</b>에서 <b>99는 현재 공격 능력치</b>, <b>괄호 안 31은 공격 개체값</b>입니다. 방어·속도·체력도 같은 표기입니다."),
    info_table([["값", "의미"], ["괄호 밖 능력치", "종·폼·레벨·개체값·훈련 등에 따라 계산된 현재 값"], ["괄호 안 개체값", "각 항목 0~31. 개체마다 고유하며 성장·훈련으로 다시 뽑지 않음"]], [45*mm,129*mm]),
    Spacer(1, 3*mm),
    p("기존 세이브의 포획·작별 동료도 그대로 사용할 수 있습니다. 개체값을 확인하는 것만으로 포켓몬이나 저장 데이터가 바뀌지 않습니다.", SAFE),
])

page_break(story)
story.extend(page_heading("24 탐색 결과", "포획 결과를 확인하세요", "전투가 끝나면 메시지 줄과 분리된 결과창이 열립니다. 확인을 누르기 전에는 자동으로 닫히지 않습니다."))
story.append(screenshot_pair(ROOT / "docs/qa/3.3.0/wild-result-party.png", ROOT / "docs/qa/3.3.0/wild-result-failed.png", 66*mm))
story.extend([
    Spacer(1,4*mm),
    info_table([["결과", "의미"], ["포획 성공", "파티 또는 박스에 보관했습니다. 실제 보관 위치를 안내합니다."], ["포획 실패", "배틀에서는 이겼지만 이번 포획 판정에 실패했습니다."], ["탐색 종료", "배틀에서 패배하여 포획 판정은 진행하지 않았습니다."], ["보관 오류", "보관 완료를 확인하지 못했습니다. 포획 실패와는 다른 저장 문제입니다."]], [35*mm,139*mm]),
    Spacer(1,3*mm),
    bullet("아래 <b>확인</b> 버튼을 눌러 탐색으로 돌아갑니다. 창 바깥 터치·스와이프로 닫히지 않으며, 마지막 기술 선택 때의 연속 터치로 바로 닫히지 않도록 잠깐 보호합니다."),
    bullet("이름 옆 <b>*</b>는 이로치 표시입니다. 성공한 포켓몬은 확인 버튼을 누르기 전에 이미 보관되며, 화면을 다시 그리거나 확인을 눌러도 포획 판정을 반복하지 않습니다."),
    bullet("포획 실패 후에는 다음 탐색에서 다시 만나 승리해야 합니다. 같은 결과창에서 공을 다시 던지는 기능은 없습니다. 탐색 시작에는 활력 30을 쓰며, 다음 포획은 그때의 도감 등록 수에 따른 보너스를 적용합니다."),
    p("보관 오류가 나오면 새 포획을 반복하지 말고 저장 공간을 확인하세요. ESP는 원래 microSD를 확인한 뒤 앱/기기를 다시 실행하고 파티·박스의 실제 보관 상태를 확인하세요. 앱 데이터 삭제나 SD 포맷은 하지 마세요.", SAFE),
])

page_break(story)
story.extend(page_heading("25 포켓몬 전송과 교환", "한 마리만 보내거나 서로 교환하기", "전체 세이브 복사와 별개입니다. 선택한 포켓몬만 옮기고 다른 동료·배지·설정·연속 기록은 유지합니다."))
story.append(screenshot_pair(ROOT / "docs/qa/3.5.0/trade-menu.png", ROOT / "docs/qa/3.5.0/trade-preview.png", 62*mm))
story.extend([
    Spacer(1,3*mm),
    step_table(["양쪽을 같은 지원 버전으로 업데이트합니다. 앱·워치는 같은 Wi-Fi, ESP와 연결할 때는 ESP의 TamaPoke 방에 연결합니다. 자세한 연결 방법은 13~15쪽을 보세요.", "<b>통신 메뉴 → 포켓몬 전송 / 교환</b>을 엽니다. 선물하려면 한쪽은 <b>한 마리 보내기</b>, 다른 쪽은 <b>한 마리 받기</b>를 누릅니다. 맞교환은 양쪽 모두 <b>서로 한 마리 교환</b>을 누릅니다.", "파티·박스 목록에서 보낼 개체를 선택합니다. 이전·다음 버튼과 스와이프로 페이지를 넘깁니다. 현재 키우는 포켓몬은 먼저 파티·박스에 보관해야 하며 알은 보낼 수 없습니다.", "양쪽의 <b>상대 이름·6자리 코드·보낼 개체·받을 개체</b>를 비교합니다. 보내기 정보 / 받기 정보에서 폼·별명·개체값·기술을 확인합니다.", "양쪽 모두 <b>확인 후 확정</b>을 누르고 완료 표시를 기다립니다. 보내기는 원본에서 제거하고 상대에게 보관하며, 교환은 선택했던 칸에 상대 개체가 들어옵니다."]),
    Spacer(1,3*mm),
    bullet("받기는 파티 빈칸을 먼저 사용하고 없으면 박스 빈칸을 사용합니다. 모두 차 있으면 시작할 수 없습니다. 맞교환은 내보내는 칸을 쓰므로 가득 찬 박스에서도 가능합니다."),
    p("레벨·폼·이로치·개체값·훈련·기술·별명·개별 돌봄 상태를 보존합니다. 전송 때문에 능력치를 다시 뽑거나 통신 진화를 시키지 않습니다.", SAFE),
])
page_break(story)
story.extend(page_heading("26 교환 중단과 복구", "포켓몬을 지키며 교환 마무리하기", "한쪽만 확정하거나 연결이 끊겨도 임의로 새 거래를 시작하지 마세요."))
story.extend([
    info_table([["표시 / 상황", "해야 할 일"], ["정보와 확인 코드를 비교하세요", "아직 내 쪽에서 확정하지 않았다면 취소할 수 있습니다. 양쪽 모두 원래 포켓몬을 유지합니다."], ["확정됨 - 상대 승인 대기", "내 승인이 저장됐습니다. 상대도 확인 후 확정해야 합니다. 이 단계부터는 일방 취소할 수 없습니다."], ["상대 연결 대기 / 재연결 필요", "같은 상대와 Wi-Fi를 다시 연결하고 다시 연결을 누릅니다. 앱을 껐다 켜도 진행 기록을 불러옵니다."], ["저장 오류", "앱 데이터·저장 파일을 지우지 마세요. 저장 공간 또는 ESP의 원래 SD를 확인하고 재시작합니다."], ["전송 / 교환 완료", "닫기를 누릅니다. 상대가 아직 대기 중이면 이전 거래 재연결로 다시 만나 완료 확인을 전달합니다."]], [51*mm,123*mm]),
    Spacer(1,5*mm),
    p("교환 중 제한", H2),
    bullet("진행 중인 개체를 놓아주거나 정렬·교체하지 못하도록 보관 변경을 막습니다. 거래가 미완료인 동안 전체 세이브 백업·복원도 막습니다. 백업은 거래 시작 전에 준비하세요."),
    bullet("확정 이후에는 상대가 오프라인이면 복구를 위해 다시 만나야 할 수 있습니다. 상대 기기를 초기화하거나 앱을 삭제하면 자동 복구가 불가능해질 수 있습니다."),
    bullet("지원하지 않는 기술·폼·데이터 또는 필요한 그림 팩이 없으면 연결을 거부합니다. 양쪽 역할과 버전, ESP의 지역·폼 그림 팩을 확인하세요."),
    p("도감과 성장 규칙", H2),
    bullet("받은 종과 이로치를 도감에 등록합니다. 이미 등록된 종은 중복 집계하지 않습니다. 보내도 내 도감 기록은 지우지 않으며, 받은 종은 기존 수집 가능 도감 보너스 계산에 반영됩니다."),
    bullet("전송·교환은 좋은 작별이 아니며 하루 작별 3회를 사용하지 않습니다. 받은 포켓몬은 홈에서 성장·진화할 수 있고 기존 작별 완료·도망 방지 등의 개별 상태는 그대로 유지합니다."),
    p("통신 오류의 중복 반영을 막지만, 예전 전체 세이브를 고의로 복원해 만드는 복제를 완전히 막는 온라인 인증 기능은 없습니다. 믿을 수 있는 가까운 상대와 이용하세요.", CALLOUT),
])

page_break(story)
story.extend(page_heading("27 데려오기와 저장 확인", "교체가 멈추면 이 순서로 확인하세요", "훈련값은 각 포켓몬에게 저장됩니다. 다른 개체와 교체하면 그 개체의 훈련값을 표시합니다."))
story.append(screenshot_pair(ROOT / "docs/qa/3.5.2/bring-learn.png", ROOT / "docs/qa/3.5.2/swap-recovery.png",62*mm))
story.extend([
    Spacer(1,4*mm),
    step_table(["<b>기술 습득 선택이 남아 있을 때:</b> 데려오기를 누르면 실제 기술 선택창으로 이동합니다. 배울 기술과 교체할 칸을 확인하거나 배우지 않기를 선택한 뒤 파티·박스를 다시 여세요.", "<b>진행 중인 통신:</b> 포켓몬 전송·교환을 먼저 완료하세요. 확정한 거래가 끊겼다면 같은 상대와 다시 연결합니다.", "<b>교체 저장 확인 필요:</b> 더 진행하면 기록이 되돌아갈 수 있어 일시 정지한 상태입니다. 기기 저장 공간을 확보하고 ESP는 원래 SD 연결을 확인한 뒤 <b>다시 확인</b>을 누르세요.", "다시 확인해도 계속 멈추면 앱 삭제·데이터 초기화·SD 포맷을 하지 말고 화면과 증상을 알려주세요. 구분이 불확실한 저장은 자동으로 덮어쓰지 않습니다."]),
    Spacer(1,4*mm),
    p("훈련과 저장을 지키는 습관",H2),
    bullet("저장 확인 중에는 오프라인 성장도 멈춥니다. 다시 확인에 성공한 시점부터 시간이 흐릅니다. 탐색이 잠겼다면 저장 상태를 먼저 확인하세요. 시작이 차단되면 활력은 쓰지 않습니다."),
    bullet("훈련 막대는 개체값으로 정해지는 훈련 상한 대비 진행도입니다. 능력치 화면의 공격·방어·속도 숫자나 괄호 속 개체값과는 다른 값입니다."),
    p("업데이트는 기존 앱 위에 설치합니다. 온전한 최신 저장이 확인되면 그 진행을 보존하지만, 이미 덮어써져 사라진 과거 훈련값을 추측해서 복원하지는 않습니다.",SAFE),
])

page_break(story)
story.extend(page_heading("28 도감 포획 보너스", "100종마다 야생 포획이 쉬워집니다", "배틀 승리 후 한 번의 포획 판정 기준입니다. 조우·이로치 확률과는 별개입니다."))
story.extend([
    p("볼 배율 = 1 + 0.2 × (수집 가능한 등록 종수 ÷ 100의 정수 부분)", SAFE),
    bullet("전체 지방의 수집 가능한 종을 합산합니다. 같은 종의 중복·이로치·폼은 한 종으로 세며, 그림 미지원 종은 제외합니다. 현재 수집 가능 종수는 982종입니다."),
    bullet("부화·진화·포획·전송으로 이미 등록한 도감도 반영됩니다. 업데이트 후 도감을 다시 채울 필요가 없고, 보관한 포켓몬을 놓아줘도 도감 기록은 유지됩니다."),
    Spacer(1, 3*mm),
    info_table(capture_rows(), [24*mm,15*mm,27*mm,27*mm,27*mm,27*mm,27*mm]),
    Spacer(1, 4*mm),
    p("표를 읽는 방법", H2),
    bullet("255·190·120·45·3은 확률(%)이 아니라 종별 포획률 값입니다. 예시는 각각 캐터피·피카츄·단데기·파이리·뮤츠입니다. 각 칸이 실제 승리 후 포획 성공 확률이며 반올림한 값입니다."),
    bullet("남은 HP 10%·상태이상 보너스 없음 조건입니다. 볼 배율은 최종 확률이 아닌 계산식 안에 적용됩니다. 정수 계산 때문에 배율이 올라도 같은 확률인 구간이 있습니다. 99.99%와 정확한 100%는 다릅니다."),
    bullet("포획률 3은 특별 보정 2.5%에 같은 볼 배율을 곱합니다. 도감 100종은 3%, 500종은 5%, 900종 이상은 7%입니다. 현재 도감 최대 배율은 2.8배이며 500종에서 멈추지 않습니다."),
    p("100번째 종을 잡는 판정은 기존 99종 배율을 사용합니다. 성공해 도감에 등록된 뒤 다음 포획부터 1.2배입니다. 패배 시에는 포획하지 않으며, 100%라도 저장 오류나 보관 문제까지 성공으로 처리하지는 않습니다.", CALLOUT),
])

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
