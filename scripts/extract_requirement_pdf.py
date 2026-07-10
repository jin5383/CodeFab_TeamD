"""<이름>.pdf -> docs/requirements/<이름>.extracted.txt

PyMuPDF(fitz)로 텍스트를 추출한다. poppler(pdftotext)는 이 PDF들에 쓰인
한글 폰트의 ToUnicode 매핑을 읽지 못해 한글이 대부분 누락되지만,
PyMuPDF는 정상적으로 추출한다.

사용법:
    pip install pymupdf
    python scripts/extract_requirement_pdf.py [PDF_NAME ...]

PDF_NAME을 생략하면 CodeFab_Requirement.pdf와
CodeFab_Additional_Requirement.pdf(존재하는 것만) 모두 추출한다.
"""

import pathlib
import sys

import fitz

ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_NAMES = ["CodeFab_Requirement.pdf", "CodeFab_Additional_Requirement.pdf"]

HEADER_TEMPLATE = """\
# NOTE
# 이 파일은 {src_name}을 scripts/extract_requirement_pdf.py
# (PyMuPDF)로 추출한 결과입니다. PDF가 갱신되면 위 스크립트를 다시 실행해
# 이 파일을 갱신하세요.
# 원본 PDF의 표지/구분 슬라이드 일부는 텍스트 없이 이미지로만 되어 있어
# 추출되지 않습니다(아래 목록 참고). 그 외 페이지는 한글 포함 전체 텍스트가
# 정상적으로 추출됩니다.
#
"""


def extract_one(src: pathlib.Path) -> None:
    dest = ROOT / "docs" / "requirements" / f"{src.stem}.extracted.txt"

    doc = fitz.open(src)
    empty_pages = []
    chunks = []
    for i in range(doc.page_count):
        text = doc[i].get_text().strip()
        if not text:
            empty_pages.append(i + 1)
        chunks.append(f"----- page {i + 1} -----\n{text}\n")
    page_count = doc.page_count
    doc.close()

    note = HEADER_TEMPLATE.format(src_name=src.name)
    if empty_pages:
        note += f"# 텍스트 없는 페이지(이미지 전용 슬라이드로 추정): {empty_pages}\n#\n"

    dest.write_text(note + "\n".join(chunks), encoding="utf-8")
    print(f"wrote {dest} ({page_count} pages, {len(empty_pages)} empty)")


def extract() -> None:
    names = sys.argv[1:] or [n for n in DEFAULT_NAMES if (ROOT / n).exists()]
    for name in names:
        extract_one(ROOT / name)


if __name__ == "__main__":
    extract()
