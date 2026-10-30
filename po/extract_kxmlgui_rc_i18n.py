from __future__ import annotations

import argparse
import datetime as _dt
import os
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path


LANGUAGES = ["zh_CN"]


@dataclass(frozen=True)
class PoEntry:
    msgid: str
    msgstr: str
    refs: tuple[str, ...]


def _po_quote(value: str) -> str:
    value = value.replace("\\", "\\\\").replace('"', '\\"')
    value = value.replace("\n", "\\n")
    return f'"{value}"'


def _format_header(*, is_pot: bool, language: str | None) -> list[str]:
    now = _dt.datetime.now().astimezone()
    creation_date = now.strftime("%Y-%m-%d %H:%M%z")
    lang_value = "" if is_pot else (language or "")
    charset_value = "CHARSET" if is_pot else "UTF-8"
    project_id = "pineapple-notepad"

    lines = [
        'msgid ""',
        'msgstr ""',
        _po_quote(f"Project-Id-Version: {project_id}\n"),
        _po_quote("Report-Msgid-Bugs-To: \n"),
        _po_quote(f"POT-Creation-Date: {creation_date}\n"),
        _po_quote("PO-Revision-Date: YEAR-MO-DA HO:MI+ZONE\n" if is_pot else f"PO-Revision-Date: {creation_date}\n"),
        _po_quote("Last-Translator: FULL NAME <EMAIL@ADDRESS>\n" if is_pot else "Last-Translator: \n"),
        _po_quote("Language-Team: LANGUAGE <LL@li.org>\n" if is_pot else "Language-Team: \n"),
        _po_quote(f"Language: {lang_value}\n"),
        _po_quote("MIME-Version: 1.0\n"),
        _po_quote(f"Content-Type: text/plain; charset={charset_value}\n"),
        _po_quote("Content-Transfer-Encoding: 8bit\n"),
        "",
    ]
    return lines


def _write_po(path: Path, *, entries: list[PoEntry], is_pot: bool, language: str | None) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    out: list[str] = []

    if is_pot:
        out.extend(
            [
                "# SOME DESCRIPTIVE TITLE.",
                "# Copyright (C) YEAR THE PACKAGE'S COPYRIGHT HOLDER",
                "# This file is distributed under the same license as the pineapple-notepad package.",
                "# FIRST AUTHOR <EMAIL@ADDRESS>, YEAR.",
                "#",
                "#, fuzzy",
                "",
            ]
        )
    else:
        out.extend(
            [
                f"# {language} translations for pineapple-notepad package.",
                "# This file is distributed under the same license as the pineapple-notepad package.",
                "#",
                "",
            ]
        )

    out.extend(_format_header(is_pot=is_pot, language=language))

    for entry in entries:
        for ref in entry.refs:
            out.append(f"#: {ref}")
        out.append(f"msgid {_po_quote(entry.msgid)}")
        out.append(f"msgstr {_po_quote(entry.msgstr)}")
        out.append("")

    content = "\n".join(out).rstrip() + "\n"
    path.write_text(content, encoding="utf-8", newline="\n")


def _parse_po(path: Path) -> dict[str, str]:
    if not path.exists():
        return {}

    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    entries: dict[str, str] = {}

    current_msgid: list[str] | None = None
    current_msgstr: list[str] | None = None
    state: str | None = None
    obsolete_block = False

    def flush() -> None:
        nonlocal current_msgid, current_msgstr, state, obsolete_block
        if obsolete_block:
            current_msgid = None
            current_msgstr = None
            state = None
            obsolete_block = False
            return
        if current_msgid is None or current_msgstr is None:
            return
        msgid = "".join(current_msgid)
        msgstr = "".join(current_msgstr)
        if msgid != "":
            entries[msgid] = msgstr
        current_msgid = None
        current_msgstr = None
        state = None

    def parse_quoted(s: str) -> str:
        s = s.strip()
        if not (s.startswith('"') and s.endswith('"')):
            return ""
        s = s[1:-1]
        return s.replace('\\"', '"').replace("\\\\", "\\")

    for raw in lines:
        line = raw.strip()
        if not line:
            flush()
            continue
        if line.startswith("#~"):
            obsolete_block = True
            continue
        if line.startswith("#"):
            continue
        if line.startswith("msgid "):
            flush()
            current_msgid = [parse_quoted(line[len("msgid ") :])]
            current_msgstr = []
            state = "msgid"
            continue
        if line.startswith("msgstr "):
            if current_msgid is None:
                continue
            current_msgstr = [parse_quoted(line[len("msgstr ") :])]
            state = "msgstr"
            continue
        if line.startswith('"') and line.endswith('"') and state == "msgid" and current_msgid is not None:
            current_msgid.append(parse_quoted(line))
            continue
        if line.startswith('"') and line.endswith('"') and state == "msgstr" and current_msgstr is not None:
            current_msgstr.append(parse_quoted(line))
            continue

    flush()
    return entries


def _extract_kxmlgui_texts(rc_path: Path) -> list[PoEntry]:
    data = rc_path.read_text(encoding="utf-8", errors="replace")
    root = ET.fromstring(data)

    texts: list[str] = []
    seen: set[str] = set()
    for elem in root.iter():
        tag = elem.tag.split("}", 1)[-1]
        if tag != "text":
            continue
        if elem.get("translationDomain") is not None:
            continue
        if elem.text is None:
            continue
        value = elem.text.strip()
        if not value:
            continue
        if value in seen:
            continue
        seen.add(value)
        texts.append(value)

    ref = os.path.relpath(rc_path, rc_path.parents[1]).replace("\\", "/")
    return [PoEntry(msgid=t, msgstr="", refs=(ref,)) for t in texts]


def _repo_root_from_script() -> Path:
    return Path(__file__).resolve().parents[1]


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--rc",
        default=str(_repo_root_from_script() / "src" / "pineapple-notepadui.rc"),
        help="Path to KXMLGUI .rc file",
    )
    args = parser.parse_args(argv)

    rc_path = Path(args.rc).resolve()
    repo_root = _repo_root_from_script()
    po_dir = repo_root / "po"

    extracted = _extract_kxmlgui_texts(rc_path)
    pot_path = po_dir / "pineapple-notepad.pot"
    _write_po(pot_path, entries=extracted, is_pot=True, language=None)

    for lang in LANGUAGES:
        po_path = po_dir / lang / "pineapple-notepad.po"
        existing = _parse_po(po_path)
        merged = [PoEntry(msgid=e.msgid, msgstr=existing.get(e.msgid, ""), refs=e.refs) for e in extracted]
        _write_po(po_path, entries=merged, is_pot=False, language=lang)

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

