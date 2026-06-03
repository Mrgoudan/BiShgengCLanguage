#!/usr/bin/env python3
"""
将 BiShengCLanguageUserManual.md 拆分为 src/ 中的多个子文件。

用法:
    python split_manual.py <source_file> <output_dir>
"""

import re
import os
import argparse
from collections import OrderedDict

CHAPTER_DIR_MAP = {
    "1": "chapter-1-getting-started",
    "2": "chapter-2-development-efficiency",
    "3": "chapter-3-memory-safety",
    "4": "chapter-4-concurrency",
    "5": "chapter-5-toolchain",
    "6": "chapter-6-standard-library",
    "附录": "appendix",
}

# Maps section identifiers (from ### X.Y. Title) to output files
SECTION_MAP = OrderedDict({
    "1.1": (CHAPTER_DIR_MAP["1"], "1-build"),
    "1.2": (CHAPTER_DIR_MAP["1"], "2-hello-bsc"),
    "2.1": (CHAPTER_DIR_MAP["2"], "1-member-functions"),
    "2.2": (CHAPTER_DIR_MAP["2"], "2-generic"),
    "2.3": (CHAPTER_DIR_MAP["2"], "3-constexpr"),
    "2.4": (CHAPTER_DIR_MAP["2"], "4-trait"),
    "2.5": (CHAPTER_DIR_MAP["2"], "5-operator-overloading"),
    "3.1": (CHAPTER_DIR_MAP["3"], "1-ownership"),
    "3.2": (CHAPTER_DIR_MAP["3"], "2-borrowing"),
    "3.3": (CHAPTER_DIR_MAP["3"], "3-nonnull-pointer"),
    "3.4": (CHAPTER_DIR_MAP["3"], "4-owned-struct"),
    "3.5": (CHAPTER_DIR_MAP["3"], "5-safe-zone"),
    "3.6": (CHAPTER_DIR_MAP["3"], "6-initial-analysis"),
    "4.1": (CHAPTER_DIR_MAP["4"], "1-stackless-coroutine"),
    "5.1": (CHAPTER_DIR_MAP["5"], "1-bsc2c"),
    "5.2": (CHAPTER_DIR_MAP["5"], "2-debugging"),
    "5.3": (CHAPTER_DIR_MAP["5"], "3-ide"),
    "6.1": (CHAPTER_DIR_MAP["6"], "1-safe-api"),
    "6.2": (CHAPTER_DIR_MAP["6"], "2-safe-container"),
    "6.3": (CHAPTER_DIR_MAP["6"], "3-smart-pointer"),
    "6.4": (CHAPTER_DIR_MAP["6"], "4-coroutine-scheduler"),
    "6.5": (CHAPTER_DIR_MAP["6"], "5-network-library"),
})

APPENDIX_MAP = OrderedDict({
    "附录A": (CHAPTER_DIR_MAP["附录"], "01-keywords"),
    "附录B": (CHAPTER_DIR_MAP["附录"], "02-grammar"),
})

CHAPTER_HEADING_RE = re.compile(r'##\s+(\d+)\.\s+(.+)')
SECTION_HEADING_RE = re.compile(r'###\s+(\d+\.\d+|附录[ABCDEF])\.\s+(.+)')
SUBHEADING_RE = re.compile(r'^(#{4,6})\s+(?:([\dA-Z]+(?:\.[\dA-Z]+)*)\.\s+)?(.+)')


def extract_section_id(heading: str) -> str | None:
    m = SECTION_HEADING_RE.match(heading)
    return m.group(1) if m else None


def extract_section_title(heading: str) -> str | None:
    """Extract the display title from a ### heading.
    For regular sections returns the title after the number.
    For appendix sections returns '{letter} - {title}' like 'A - 关键字'.
    """
    m = SECTION_HEADING_RE.match(heading)
    if not m:
        return None
    sec_id = m.group(1)
    title = m.group(2)
    if sec_id.startswith('附录'):
        letter = sec_id[2:]
        return f'{letter} - {title}'
    return title


def extract_chapter_id(heading: str) -> str | None:
    if heading == '## 附录':
        return '附录'
    m = CHAPTER_HEADING_RE.match(heading)
    return m.group(1) if m else None


def convert_section_heading(heading: str) -> str:
    return SECTION_HEADING_RE.sub(r'# \2', heading)


def convert_chapter_heading(heading: str) -> str:
    if heading == '## 附录':
        return '# 附录'
    return CHAPTER_HEADING_RE.sub(r'# \2', heading)


def extract_chapter_title(heading: str) -> str:
    if heading == '## 附录':
        return '附录'
    m = CHAPTER_HEADING_RE.match(heading)
    return m.group(2) if m else heading.removeprefix('## ')


def process_subheadings(lines: list[str]) -> list[str]:
    result = []
    for line in lines:
        m = SUBHEADING_RE.match(line)
        if m:
            hashes = m.group(1)
            title = m.group(3)
            new_level = max(1, len(hashes) - 2)
            result.append(f'{"#" * new_level} {title}\n')
        else:
            result.append(line)
    return result


def build_chapter_sections_map():
    """Return {chapter_num: [(display_title, relative_file_path), ...]}."""
    mapping = {}
    for sec_id, (dir_name, file_name) in SECTION_MAP.items():
        chapter_num = sec_id.split('.')[0]
        mapping.setdefault(chapter_num, []).append((sec_id, file_name))
    for sec_id, (dir_name, file_name) in APPENDIX_MAP.items():
        mapping.setdefault('附录', []).append((sec_id, file_name))
    return mapping


def main(source_file: str, output_dir: str):
    with open(source_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    os.makedirs(output_dir, exist_ok=True)

    # First pass: collect display titles for all sections and chapters
    section_titles = {}
    chapter_titles = {}
    for line in lines:
        stripped = line.strip()
        if stripped.startswith('## ') and stripped != '## 简介':
            ch_id = extract_chapter_id(stripped)
            if ch_id:
                chapter_titles[ch_id] = extract_chapter_title(stripped)
        elif stripped.startswith('### '):
            sec_id = extract_section_id(stripped)
            title = extract_section_title(stripped)
            if sec_id and title:
                section_titles[sec_id] = title

    chapter_sections = build_chapter_sections_map()

    # Second pass: split into sections
    sections = []
    current_start = None
    current_id = None

    for i, line in enumerate(lines):
        stripped = line.strip()

        if stripped == '## 简介':
            if current_start is not None and current_id is not None:
                sections.append((current_start, i, current_id))
            current_start = i
            current_id = 'intro'

        elif stripped.startswith('## ') and stripped != '## 简介':
            if current_start is not None and current_id is not None:
                sections.append((current_start, i, current_id))
            chapter_id = extract_chapter_id(stripped)
            current_start = i
            current_id = f'ch:{chapter_id}' if chapter_id else None

        elif stripped.startswith('### '):
            if current_start is not None and current_id is not None:
                sections.append((current_start, i, current_id))
            current_start = i
            current_id = extract_section_id(stripped)

    if current_start is not None and current_id is not None:
        sections.append((current_start, len(lines), current_id))

    # Third pass: Generate SUMMARY.md
    summary_lines = ['# Summary\n', '\n', '[简介](README.md)\n\n']
    for chapter_num in CHAPTER_DIR_MAP:
        dir_name = CHAPTER_DIR_MAP[chapter_num]
        ch_title = chapter_titles.get(chapter_num, dir_name)
        summary_lines.append(f'- [{ch_title}](./{dir_name}/README.md)\n')
        for sec_id, file_name in chapter_sections.get(chapter_num, []):
            sec_title = section_titles.get(sec_id, file_name)
            summary_lines.append(f'  - [{sec_title}](./{dir_name}/{file_name}.md)\n')

    summary_path = os.path.join(output_dir, 'SUMMARY.md')
    with open(summary_path, 'w', encoding='utf-8') as f:
        f.writelines(summary_lines)
    print(f"Written: {summary_path}")

    # Fourth pass: write files
    for start, end, section_id in sections:
        content_lines = lines[start:end]
        heading = content_lines[0].strip()
        body_lines = process_subheadings(content_lines[1:])

        if section_id == 'intro':
            combined = ['# 简介\n'] + body_lines
            output_path = os.path.join(output_dir, 'README.md')

        elif section_id.startswith('ch:'):
            chapter_num = section_id[3:]
            dir_name = CHAPTER_DIR_MAP.get(chapter_num)
            if not dir_name:
                print(f"Warning: unknown chapter '{chapter_num}', skipping")
                continue

            # Build link list from chapter's sub-sections
            link_lines = []
            section_entries = chapter_sections.get(chapter_num, [])
            for sec_id, file_name in section_entries:
                title = section_titles.get(sec_id, file_name)
                link_lines.append(f'- [{title}](./{file_name}.md)\n')

            # Insert link list after heading, before body
            combined = [convert_chapter_heading(heading) + '\n']
            if link_lines:
                combined.append('\n')
                combined.extend(link_lines)
                combined.append('\n')
            combined.extend(body_lines)
            output_path = os.path.join(output_dir, dir_name, 'README.md')

        elif section_id in SECTION_MAP:
            dir_name, file_name = SECTION_MAP[section_id]
            combined = [convert_section_heading(heading) + '\n'] + body_lines
            output_path = os.path.join(output_dir, dir_name, file_name + '.md')

        elif section_id in APPENDIX_MAP:
            dir_name, file_name = APPENDIX_MAP[section_id]
            combined = [convert_section_heading(heading) + '\n'] + body_lines
            output_path = os.path.join(output_dir, dir_name, file_name + '.md')

        else:
            print(f"Warning: unknown section '{section_id}', skipping")
            continue

        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        with open(output_path, 'w', encoding='utf-8') as f:
            f.writelines(combined)
        print(f"Written: {output_path}")

    print(f"Done! Total files written: {len(sections) + 1}")


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Split a manual into chapters.")
    parser.add_argument("source_file", type=str, help="Path to the source Markdown file.")
    parser.add_argument("output_dir", type=str, help="Path to the output directory.")
    args = parser.parse_args()
    main(args.source_file, args.output_dir)
