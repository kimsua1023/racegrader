import re
from pathlib import Path


def log(item, status, detail=""):
    mark = {"OK": "✅", "SKIP": "⏭️ ", "FAIL": "❌"}[status]
    print(f"{mark} {item}" + (f" — {detail}" if detail else ""))


def patch_makefile(path="Makefile"):
    text = Path(path).read_text()

    if "_race_stress" in text:
        log("UPROGS에 race_stress 등록", "SKIP", "이미 존재함")
        return

    m = re.search(r"([ \t]*)\$U/_cow_test\\\n", text)
    if not m:
        log("UPROGS에 race_stress 등록", "FAIL", "anchor(_cow_test)를 못 찾음")
        return

    old_line = m.group(0)
    if text.count(old_line) != 1:
        log("UPROGS에 race_stress 등록", "FAIL", f"anchor count={text.count(old_line)}")
        return

    indent = m.group(1)
    insertion = f"{indent}$U/_cow_test\\\n{indent}$U/_race_stress\\\n"
    text = text.replace(old_line, insertion, 1)
    Path(path).write_text(text)
    log("UPROGS에 race_stress 등록", "OK")


if __name__ == "__main__":
    print("=== race_stress UPROGS 등록 시작 ===\n")
    patch_makefile()
    print("\n다음 명령으로 실제 변경 내용을 반드시 확인하세요:  git diff")
