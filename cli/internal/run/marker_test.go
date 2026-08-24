package run

import "testing"

func TestFindMarkerWaitsForNewline(t *testing.T) {
	if got := FindMarker("[RACEGRADER_DONE]"); got != "" {
		t.Fatalf("partial line matched too early: %q", got)
	}
	if got := FindMarker("[RACEGRADER_DONE] 7|3"); got != "" {
		t.Fatalf("unterminated line matched too early: %q", got)
	}
	got := FindMarker("noise\n[RACEGRADER_DONE] 7|3\n$ ")
	if got != "[RACEGRADER_DONE] 7|3" {
		t.Fatalf("got %q", got)
	}
}

func TestFindMarkerAssertFail(t *testing.T) {
	got := FindMarker("x\n[RACEGRADER_FAIL] ASSERT|user/cow_test.c:23|7|3|[C1] child W=0 after fork\n")
	want := "[RACEGRADER_FAIL] ASSERT|user/cow_test.c:23|7|3|[C1] child W=0 after fork"
	if got != want {
		t.Fatalf("got %q want %q", got, want)
	}
	o, detail := ClassifyMarker(got)
	if o != OutcomeFail || detail != want {
		t.Fatalf("classify: %s %q", o, detail)
	}
}

func TestFindMarkerPanicFailWithPromptGlue(t *testing.T) {
	got := FindMarker("$ kill 1\n$ [RACEGRADER_FAIL] PANIC|kernel/proc.c:329|1|1|init exiting\n")
	want := "[RACEGRADER_FAIL] PANIC|kernel/proc.c:329|1|1|init exiting"
	if got != want {
		t.Fatalf("got %q", got)
	}
	o, _ := ClassifyMarker(got)
	if o != OutcomeFail {
		t.Fatalf("want fail, got %s", o)
	}
}

func TestClassifyDone(t *testing.T) {
	o, d := ClassifyMarker("[RACEGRADER_DONE] 7|3")
	if o != OutcomePass || d != "[RACEGRADER_DONE] 7|3" {
		t.Fatalf("got %s %q", o, d)
	}
}
