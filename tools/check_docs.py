#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Consistency checks for the documentation (docs/AI_WORKFLOW.md section 10).

  python tools/check_docs.py

Checks, over CLAUDE.md, README.md, CHANGELOG.md and docs/**/*.md:
  - every relative Markdown link points to an existing file or directory, and every #anchor to a
    heading of the target (GitHub slug rules);
  - requirement IDs (<AREA>-REQ-NNN / <AREA>-NFR-NNN, defined as the first cell of a table row,
    in backticks),
    ADR-NNN and RISK-NNN (defined as headings) are defined once, and every mention is defined;
  - every specification under docs/subsystems/ and docs/ui/ is linked from docs/README.md;
  - CLAUDE.md imports docs/AI_WORKFLOW.md with an @-line outside code blocks, and every @-import
    in CLAUDE.md resolves.
Exit status 1 when anything is wrong.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LINK = re.compile(r"(?<!!)\[([^\]]*)\]\(([^)\s]+)(?:\s+\"[^\"]*\")?\)")
IMAGE = re.compile(r"!\[([^\]]*)\]\(([^)\s]+)\)")
REQ_ID = re.compile(r"\b([A-Z]{2,3})-(REQ|NFR)-(\d{3})\b")
ADR_ID = re.compile(r"\bADR-(\d{3})\b")
RISK_ID = re.compile(r"\bRISK-(\d{3})\b")


def doc_files():
    files = [os.path.join(ROOT, n) for n in ("CLAUDE.md", "README.md", "CHANGELOG.md")]
    for base, _dirs, names in os.walk(os.path.join(ROOT, "docs")):
        if os.path.relpath(base, ROOT).replace("\\", "/").startswith("docs/research/logs"):
            continue
        files += [os.path.join(base, n) for n in names if n.endswith(".md")]
    return [f for f in files if os.path.isfile(f)]


def prose_lines(text):
    """Yield (line_no, line) outside fenced code blocks, with inline code removed."""
    fence = None
    for no, line in enumerate(text.splitlines(), 1):
        stripped = line.lstrip()
        if fence is None and (stripped.startswith("```") or stripped.startswith("~~~")):
            fence = stripped[:3]
            continue
        if fence is not None:
            if stripped.startswith(fence):
                fence = None
            continue
        yield no, re.sub(r"`[^`]*`", "``", line), line


def slugify(heading):
    text = re.sub(r"!\[[^\]]*\]\([^)]*\)", "", heading)
    text = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", text)
    text = text.replace("`", "").strip().lower()
    text = re.sub(r"[^\w\- ]", "", text)
    return text.replace(" ", "-")


_anchor_cache = {}


def anchors(path):
    if path not in _anchor_cache:
        seen = {}
        result = set()
        with open(path, encoding="utf-8") as f:
            for _no, _clean, raw in prose_lines(f.read()):
                m = re.match(r"^(#{1,6})\s+(.*?)\s*#*\s*$", raw)
                if not m:
                    continue
                slug = slugify(m.group(2))
                n = seen.get(slug, 0)
                seen[slug] = n + 1
                result.add(slug if n == 0 else f"{slug}-{n}")
        _anchor_cache[path] = result
    return _anchor_cache[path]


def main():
    errors = []
    req_defs, adr_defs, risk_defs = {}, {}, {}
    mentions = []  # (kind, id, where)
    readme_links = set()

    for path in doc_files():
        rel = os.path.relpath(path, ROOT).replace("\\", "/")
        with open(path, encoding="utf-8") as f:
            text = f.read()
        for no, clean, raw in prose_lines(text):
            where = f"{rel}:{no}"
            template = rel.startswith("docs/templates/")
            for m in [] if template else list(LINK.finditer(clean)) + list(IMAGE.finditer(clean)):
                target = m.group(2)
                if re.match(r"^[a-z][a-z0-9+.-]*:", target):  # http:, mailto:, ...
                    continue
                file_part, _, anchor = target.partition("#")
                dest = path if not file_part else os.path.normpath(
                    os.path.join(os.path.dirname(path), file_part))
                if os.path.relpath(dest, ROOT).startswith(".."):
                    continue  # outside the repository, e.g. GitHub's ../../releases
                if not os.path.exists(dest):
                    errors.append(f"{where}: broken link {target}")
                    continue
                if rel == "docs/README.md" and file_part:
                    readme_links.add(os.path.relpath(dest, ROOT).replace("\\", "/"))
                if anchor and dest.endswith(".md") and anchor not in anchors(dest):
                    errors.append(f"{where}: no heading for #{anchor} in "
                                  f"{os.path.relpath(dest, ROOT)}")
            if template:
                continue
            row = re.match(r"^\|\s*`([A-Z]{2,3}-(?:REQ|NFR)-\d{3})`\s*\|", raw)
            if row:
                rid = row.group(1)
                if rid in req_defs:
                    errors.append(f"{where}: {rid} already defined at {req_defs[rid]}")
                req_defs.setdefault(rid, where)
            head = re.match(r"^#{2,4}\s+(ADR|RISK)-(\d{3})\b", raw)
            if head:
                kind, num = head.groups()
                defs = adr_defs if kind == "ADR" else risk_defs
                key = f"{kind}-{num}"
                if key in defs:
                    errors.append(f"{where}: {key} already defined at {defs[key]}")
                defs.setdefault(key, where)
            for m in REQ_ID.finditer(raw):
                mentions.append(("req", m.group(0), where))
            for m in ADR_ID.finditer(raw):
                mentions.append(("adr", m.group(0), where))
            for m in RISK_ID.finditer(raw):
                mentions.append(("risk", m.group(0), where))

    for kind, ident, where in mentions:
        if where.startswith("docs/templates/") or where.startswith("docs/AI_WORKFLOW.md"):
            continue  # examples of the ID formats
        defs = {"req": req_defs, "adr": adr_defs, "risk": risk_defs}[kind]
        if ident not in defs:
            errors.append(f"{where}: {ident} is mentioned but never defined")

    for sub in ("docs/subsystems", "docs/ui", "docs/firmware"):
        base = os.path.join(ROOT, sub)
        if not os.path.isdir(base):
            continue
        for name in sorted(os.listdir(base)):
            spec = f"{sub}/{name}"
            if name.endswith(".md") and spec not in readme_links:
                errors.append(f"docs/README.md: does not link {spec}")

    claude = os.path.join(ROOT, "CLAUDE.md")
    imports = []
    with open(claude, encoding="utf-8") as f:
        for no, _clean, raw in prose_lines(f.read()):
            m = re.match(r"^@(\S+)\s*$", raw.strip())
            if m:
                imports.append(m.group(1))
                if not os.path.isfile(os.path.join(ROOT, m.group(1))):
                    errors.append(f"CLAUDE.md:{no}: import @{m.group(1)} does not resolve")
    if "docs/AI_WORKFLOW.md" not in imports:
        errors.append("CLAUDE.md: no @docs/AI_WORKFLOW.md import outside a code block")

    for e in errors:
        print(e)
    print(f"check_docs: {len(doc_files())} files, {len(req_defs)} requirements, "
          f"{len(adr_defs)} ADRs, {len(risk_defs)} risks, {len(errors)} problem(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
