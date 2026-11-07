#!/usr/bin/env python3
"""Regenerate README.txt and docs/README.pdf from README.md.

Usage:  python3 tools/make_docs.py
Needs:  pip install markdown   and   wkhtmltopdf on PATH.

PDF style: black body text (#111), dark code blocks (#1a1d27, white text),
dark navy table headers.
"""

import pathlib
import re
import subprocess
import tempfile

import markdown

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "README.md"

CSS = """
body { font-family: 'Segoe UI', 'DejaVu Sans', Arial, sans-serif; color:#111; font-size:11pt; line-height:1.5; margin:0 6px; }
h1 { font-size:26pt; letter-spacing:4px; margin-bottom:4px; color:#111; }
h2 { font-size:15pt; border-bottom:2px solid #1f2a44; padding-bottom:3px; margin-top:26px; color:#111; page-break-after:avoid; }
h3 { font-size:12.5pt; margin-top:18px; color:#111; page-break-after:avoid; }
table { border-collapse:collapse; width:100%; margin:10px 0 14px; font-size:10pt; page-break-inside:avoid; }
th { background:#1f2a44; color:#ffffff; text-align:left; padding:6px 8px; }
td { border:1px solid #c9ccd3; padding:6px 8px; vertical-align:top; color:#111; }
tr:nth-child(even) td { background:#f4f5f7; }
pre { background:#1a1d27; color:#ffffff; padding:10px 12px; border-radius:6px; font-size:9.5pt; white-space:pre-wrap; page-break-inside:avoid; }
code { font-family: Consolas, 'DejaVu Sans Mono', monospace; }
p code, li code, td code { background:#1a1d27; color:#ffffff; padding:1px 5px; border-radius:3px; font-size:9.5pt; }
hr { border:none; border-top:1px solid #d5d7dc; margin:20px 0; }
"""

TABLE_SEPARATOR = re.compile(r"^\|[-| :]+\|$")


def strip_inline(text: str) -> str:
    """Remove markdown emphasis and inline code markers."""
    text = re.sub(r"\*\*(.+?)\*\*", r"\1", text)
    text = re.sub(r"(?<!\*)\*(?!\*)(.+?)(?<!\*)\*(?!\*)", r"\1", text)
    return re.sub(r"`([^`\n]+)`", r"\1", text)


def to_text(md: str) -> str:
    """Plain-text rendering: underlined headings, tables as 'name:  description'."""
    lines = md.split("\n")
    out, in_code = [], False
    for i, line in enumerate(lines):
        if line.startswith("```"):
            in_code = not in_code
            continue
        if in_code:
            out.append("    " + line)
            continue
        if TABLE_SEPARATOR.match(line):
            continue
        is_header_row = line.startswith("|") and i + 1 < len(lines) and TABLE_SEPARATOR.match(lines[i + 1])
        if is_header_row:
            continue
        line = strip_inline(line)
        if line.startswith("# "):
            out += [line[2:].upper(), "=" * len(line[2:])]
        elif line.startswith("## "):
            out += [line[3:].upper(), "-" * len(line[3:])]
        elif line.startswith("### "):
            out += [line[4:], "~" * len(line[4:])]
        elif line.strip() == "---":
            out.append("")
        elif line.startswith("|"):
            cells = [c.strip() for c in line.strip("|").split("|")]
            out.append("  " + cells[0] + (":  " + "  ".join(cells[1:]) if len(cells) > 1 else ""))
        else:
            out.append(line)
    return "\n".join(out).rstrip() + "\n"


def to_pdf(md: str, dest: pathlib.Path) -> None:
    body = markdown.markdown(md, extensions=["tables", "fenced_code"])
    # wkhtmltopdf ignores page-break-after on headings, so keep each heading
    # together with the table or code block that follows it.
    body = re.sub(r"(<h[23][^>]*>.*?</h[23]>\s*(?:<p>.*?</p>\s*)?(?:<table>.*?</table>|<pre>.*?</pre>))",
                  r'<div style="page-break-inside:avoid">\1</div>', body, flags=re.S)
    html = f"<!doctype html><html><head><meta charset='utf-8'><style>{CSS}</style></head><body>{body}</body></html>"
    dest.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile("w", suffix=".html", delete=False, encoding="utf-8") as f:
        f.write(html)
    subprocess.run(["wkhtmltopdf", "--quiet", "--enable-local-file-access", "-s", "Letter",
                    "--margin-top", "16mm", "--margin-bottom", "16mm",
                    "--margin-left", "14mm", "--margin-right", "14mm", f.name, str(dest)], check=True)
    pathlib.Path(f.name).unlink()


if __name__ == "__main__":
    md = SRC.read_text(encoding="utf-8")
    (ROOT / "README.txt").write_text(to_text(md), encoding="utf-8")
    to_pdf(md, ROOT / "docs" / "README.pdf")
    print("wrote README.txt and docs/README.pdf")
