#!/usr/bin/env python3
"""Render the offline HTML guide from the Markdown sources."""

import argparse
import html
import re
from pathlib import Path
from urllib.parse import unquote, urlsplit

from markdown_it import MarkdownIt
from pygments import highlight
from pygments.formatters import HtmlFormatter
from pygments.lexers import get_lexer_by_name
from pygments.util import ClassNotFound

GUIDE = Path(__file__).resolve().parent
ROOT = GUIDE.parent.parent
GITHUB = "https://github.com/husams/clang-tracer/blob/main/"
CHAPTERS = {
    "Get started": {
        "user": "docs/guide/user.md",
        "tutorials": "docs/guide/tutorials.md",
    },
    "Reference": {
        "commands": "docs/guide/commands.md",
        "developer": "docs/guide/developer.md",
    },
    "Walkthroughs": {
        "install": "docs/build-rhel.md",
        "repl": "docs/native-repl.md",
        "python": "python/README.md",
        "storage": "docs/event-storage.md",
        "context": "docs/shared-context.md",
    },
    "Runnable examples": {
        "example-callback": "examples/basic-callbacks/README.md",
        "example-legacy": "examples/legacy-callbacks/README.md",
        "example-interceptor": "examples/cpp-interceptors/README.md",
        "example-arguments": "examples/order-arguments/README.md",
        "example-prepared": "examples/prepared-injectors/README.md",
        "example-values": "examples/native-repl-values/README.md",
        "example-stack": "examples/call-stacks/README.md",
        "example-shared": "examples/shared-context/README.md",
        "example-jit": "examples/native-repl-jit/README.md",
        "example-python": "examples/python/README.md",
        "validation": "docs/validation.md",
    },
}
CHAPTER_IDS = {ROOT / path: key for group in CHAPTERS.values() for key, path in group.items()}


def slug(title):
    return re.sub(r"[^\w -]", "", title.lower()).replace(" ", "-")


def format_code(tokens, index, options, env):
    token = tokens[index]
    language = token.info.split()[0] if token.info.strip() else "text"
    if language == "text" and re.search(r"^%[a-z]", token.content, re.MULTILINE):
        language = "bash"
    try:
        lexer = get_lexer_by_name(language)
    except ClassNotFound:
        lexer = get_lexer_by_name("text")
    source = highlight(token.content, lexer, HtmlFormatter(nowrap=True))
    return (
        '<div class="code-block"><div class="code-bar">'
        f'<span>{html.escape(language)}</span><button class="copy" type="button">Copy</button>'
        f'</div><pre><code>{source}</code></pre></div>\n'
    )


def rewrite_links(tokens, path):
    for token in tokens:
        if token.children:
            rewrite_links(token.children, path)
        if token.type != "link_open":
            continue
        target = urlsplit(token.attrGet("href"))
        if target.scheme or target.netloc:
            continue
        destination = (path.parent / unquote(target.path)).resolve() if target.path else path
        chapter = CHAPTER_IDS.get(destination)
        if chapter:
            suffix = f"--{target.fragment}" if target.fragment else ""
            token.attrSet("href", f"#{chapter}{suffix}")
        elif destination == GUIDE / "index.html":
            token.attrSet("href", "#user")
        elif destination.is_relative_to(ROOT):
            suffix = f"#{target.fragment}" if target.fragment else ""
            token.attrSet("href", f"{GITHUB}{destination.relative_to(ROOT).as_posix()}{suffix}")


def render():
    markdown = MarkdownIt("commonmark", {"html": False}).enable("table")
    markdown.renderer.rules["fence"] = format_code
    sections = []
    navigation = []
    for group, chapters in CHAPTERS.items():
        links = []
        for key, filename in chapters.items():
            path = ROOT / filename
            tokens = markdown.parse(path.read_text(encoding="utf-8"))
            rewrite_links(tokens, path)
            headings = []
            used_ids = set()
            for index, token in enumerate(tokens):
                if token.type == "heading_close":
                    token.tag = f"h{min(int(token.tag[1:]) + 1, 6)}"
                if token.type != "heading_open":
                    continue
                title = "".join(child.content for child in tokens[index + 1].children)
                anchor = key if token.tag == "h1" else f"{key}--{slug(title)}"
                base = anchor
                duplicate = 1
                while anchor in used_ids:
                    anchor = f"{base}-{duplicate}"
                    duplicate += 1
                used_ids.add(anchor)
                token.attrSet("id", anchor)
                if token.tag in {"h1", "h2"}:
                    label = "Overview" if token.tag == "h1" else html.escape(title)
                    headings.append(f'<a href="#{anchor}">{label}</a>')
                if token.tag == "h1":
                    chapter_title = html.escape(title)
                token.tag = f"h{min(int(token.tag[1:]) + 1, 6)}"
            links.append(f'<li><details><summary>{chapter_title}</summary>'
                         f'<div class="subnav">{"".join(headings)}</div></details></li>')
            body = markdown.renderer.render(tokens, markdown.options, {})
            sections.append(f'<article aria-labelledby="{key}">{body}</article>')
        navigation.append(f'<div class="nav-group"><h2>{group}</h2><ul>{"".join(links)}</ul></div>')
    return (
        (GUIDE / "template.html").read_text(encoding="utf-8")
        .replace("<!-- HIGHLIGHT -->", HtmlFormatter(style="github-dark").get_style_defs(".code-block"))
        .replace("<!-- NAVIGATION -->", "\n".join(navigation))
        .replace("<!-- CHAPTERS -->", "\n".join(sections))
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="fail if index.html is stale")
    args = parser.parse_args()
    output = GUIDE / "index.html"
    content = render()
    if args.check:
        if not output.exists() or output.read_text(encoding="utf-8") != content:
            parser.exit(1, "index.html is stale. Run docs/guide/build.py.\n")
    else:
        output.write_text(content, encoding="utf-8")


if __name__ == "__main__":
    main()
