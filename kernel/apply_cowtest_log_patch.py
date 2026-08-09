import re
from pathlib import Path


def log(fname, item, status, detail=""):
    mark = {"OK": "✅", "SKIP": "⏭️ ", "FAIL": "❌"}[status]
    print(f"{mark} [{fname}] {item}" + (f" — {detail}" if detail else ""))


def patch_cow_test(path="user/cow_test.c"):
    fname = "user/cow_test.c"
    text = Path(path).read_text()

    if "RACEGRADER_FAIL" in text:
        log(fname, "passfail() 교체", "SKIP", "이미 존재함")
        return

    # 1) passfail()을 passfail_impl()로 교체 + 매크로로 기존 호출부 자동 확장
    old_passfail = '''static void passfail(const char *label, int pass){
  printf("%s: %s\\n", label, pass ? "OK" : "FAIL");
  if(!pass) exit(1);
}'''
    if text.count(old_passfail) != 1:
        log(fname, "passfail() 교체", "FAIL", f"anchor count={text.count(old_passfail)}")
        return

    new_passfail = '''#ifndef CHAOS_SEED
#define CHAOS_SEED 1
#endif

// RaceGrader: passfail()을 매크로로 감싸서 기존 호출부 전부 그대로 두고
// __FILE__/__LINE__을 자동 주입한다 (panic() 패치와 동일한 방식).
// 실패 시: [RACEGRADER_FAIL] ASSERT|file:line|SEED|PID|label
static void passfail_impl(const char *label, int pass, const char *file, int line){
  printf("%s: %s\\n", label, pass ? "OK" : "FAIL");
  if(!pass){
    printf("[RACEGRADER_FAIL] ASSERT|%s:%d|%d|%d|%s\\n", file, line, CHAOS_SEED, getpid(), label);
    exit(1);
  }
}
#define passfail(label, pass) passfail_impl((label), (pass), __FILE__, __LINE__)'''

    text = text.replace(old_passfail, new_passfail, 1)

    # 2) 전체 통과 시 [RACEGRADER_DONE] 마커 추가
    old_done = '''    line();
    printf("== ALL COW CHECKS PASSED ==\\n");
    exit(0);'''
    if text.count(old_done) != 1:
        log(fname, "[RACEGRADER_DONE] 추가", "FAIL", f"anchor count={text.count(old_done)}")
        return
    new_done = '''    line();
    printf("== ALL COW CHECKS PASSED ==\\n");
    printf("[RACEGRADER_DONE] %d|%d\\n", CHAOS_SEED, getpid());
    exit(0);'''
    text = text.replace(old_done, new_done, 1)

    Path(path).write_text(text)
    log(fname, "passfail() 교체 + [RACEGRADER_DONE] 추가", "OK")


if __name__ == "__main__":
    print("=== cow_test.c 로그 형식 패치 시작 ===\n")
    patch_cow_test()
    print("\n다음 명령으로 실제 변경 내용을 반드시 확인하세요:  git diff")
