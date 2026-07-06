"""CodeFab_Requirement.pdf -> docs/CodeFab_Requirement.extracted.txt

PyMuPDF(fitz)로 텍스트를 추출한다. poppler(pdftotext)는 이 PDF에 쓰인
한글 폰트의 ToUnicode 매핑을 읽지 못해 한글이 대부분 누락되지만,
PyMuPDF는 정상적으로 추출한다.

사용법:
    pip install pymupdf
    python scripts/extract_requirement_pdf.py
"""

import pathlib

import fitz

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "CodeFab_Requirement.pdf"
DEST = ROOT / "docs" / "CodeFab_Requirement.extracted.txt"

HEADER = """\
# NOTE
# 이 파일은 CodeFab_Requirement.pdf를 scripts/extract_requirement_pdf.py
# (PyMuPDF)로 추출한 결과입니다. PDF가 갱신되면 위 스크립트를 다시 실행해
# 이 파일을 갱신하세요.
# 원본 PDF의 표지/구분 슬라이드 일부는 텍스트 없이 이미지로만 되어 있어
# 추출되지 않습니다(아래 목록 참고). 그 외 페이지는 한글 포함 전체 텍스트가
# 정상적으로 추출됩니다.
#
"""


def extract() -> None:
    doc = fitz.open(SRC)
    empty_pages = []
    chunks = []
    for i in range(doc.page_count):
        text = doc[i].get_text().strip()
        if not text:
            empty_pages.append(i + 1)
        chunks.append(f"----- page {i + 1} -----\n{text}\n")
    page_count = doc.page_count
    doc.close()

    note = HEADER
    if empty_pages:
        note += f"# 텍스트 없는 페이지(이미지 전용 슬라이드로 추정): {empty_pages}\n#\n"

    DEST.write_text(note + "\n".join(chunks), encoding="utf-8")
    print(f"wrote {DEST} ({page_count} pages, {len(empty_pages)} empty)")


if __name__ == "__main__":
    extract()
