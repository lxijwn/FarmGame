"""Render the current submission Markdown as a Chinese PDF."""
from pathlib import Path
import re
from xml.sax.saxutils import escape
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, PageBreak, Preformatted
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT/'交付材料'
OUT.mkdir(exist_ok=True)
pdfmetrics.registerFont(TTFont('Chinese', 'C:/Windows/Fonts/simhei.ttf'))
INK = colors.HexColor('#233b35')
ACCENT = colors.HexColor('#426d52')
body = ParagraphStyle('body', fontName='Chinese', fontSize=9.5, leading=15.7,
                      textColor=INK, spaceAfter=8, wordWrap='CJK')
styles = {
    1: ParagraphStyle('title', parent=body, fontSize=23, leading=30, spaceAfter=18, keepWithNext=True),
    2: ParagraphStyle('h2', parent=body, fontSize=16, leading=23, textColor=ACCENT, spaceAfter=13, keepWithNext=True),
    3: ParagraphStyle('h3', parent=body, fontSize=11.8, leading=19, spaceBefore=7, spaceAfter=7, keepWithNext=True),
}
cell = ParagraphStyle('cell', parent=body, fontSize=8.5, leading=13.3, spaceAfter=0)
code = ParagraphStyle('code', fontName='Courier', fontSize=8.3, leading=11,
                      backColor=colors.HexColor('#f2f6f2'), leftIndent=10, spaceBefore=5, spaceAfter=10)


def inline(text):
    text = escape(text)
    text = re.sub(r'\[([^\]]+)\]\((https?://[^\s)]+)\)',
                  r'<link href="\2" color="#326746"><u>\1</u></link>', text)
    return re.sub(r'\*\*(.+?)\*\*', r'<b>\1</b>', text)


source = OUT/'Farm测试题_提交版.md'
if not source.exists():
    source = ROOT/'Farm测试题_提交版.md'
lines = source.read_text(encoding='utf-8').splitlines()
story = []
i = 0
while i < len(lines):
    line = lines[i].strip()
    if not line:
        i += 1
        continue
    if line == '<!-- pagebreak -->':
        story.append(PageBreak())
        i += 1
        continue
    if line.startswith('```'):
        chunk = []
        i += 1
        while i < len(lines) and not lines[i].startswith('```'):
            chunk.append(lines[i])
            i += 1
        story.append(Preformatted('\n'.join(chunk), code))
        i += 1
        continue
    if line.startswith('|'):
        rows = []
        while i < len(lines) and lines[i].strip().startswith('|'):
            cells = [c.strip() for c in lines[i].strip().strip('|').split('|')]
            if not all(re.fullmatch(r'[:\- ]+', c) for c in cells):
                rows.append([Paragraph(inline(c), cell) for c in cells])
            i += 1
        table = Table(rows, colWidths=[92, 413], repeatRows=1, hAlign='LEFT')
        table.setStyle(TableStyle([
            ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#e2ede3')),
            ('ROWBACKGROUNDS', (0, 1), (-1, -1), [colors.HexColor('#f6f8f4'), colors.white]),
            ('VALIGN', (0, 0), (-1, -1), 'TOP'),
            ('LEFTPADDING', (0, 0), (-1, -1), 8), ('RIGHTPADDING', (0, 0), (-1, -1), 8),
            ('TOPPADDING', (0, 0), (-1, -1), 6), ('BOTTOMPADDING', (0, 0), (-1, -1), 6),
            ('LINEBELOW', (0, 0), (-1, 0), .6, colors.HexColor('#b6c9b8')),
        ]))
        story.extend([table, Spacer(1, 10)])
        continue
    match = re.match(r'^(#{1,3}) (.*)', line)
    story.append(Paragraph(inline(match[2] if match else line), styles[len(match[1])] if match else body))
    i += 1


def footer(canvas, doc):
    canvas.saveState()
    canvas.setStrokeColor(colors.HexColor('#c6d4c5'))
    canvas.line(45, 40, A4[0]-45, 40)
    canvas.setFont('Chinese', 8)
    canvas.setFillColor(ACCENT)
    canvas.drawString(45, 26, 'Farm v1.3 / 游戏规则、需求整合与架构设计')
    canvas.drawRightString(A4[0]-45, 26, str(doc.page))
    canvas.restoreState()


path = OUT/'Farm测试题_提交版.pdf'
doc = SimpleDocTemplate(str(path), pagesize=A4, leftMargin=45, rightMargin=45,
                        topMargin=43, bottomMargin=54, title='Farm 测试题答题', author='')
doc.build(story, onFirstPage=footer, onLaterPages=footer)
print(path)
