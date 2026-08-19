package report

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"

	"racegrader/cli/internal/run"
)

func DefaultPath(now time.Time) string {
	return fmt.Sprintf("racegrader-%s.log", now.Format("20060102-150405"))
}

func Resolve(out string, now time.Time) (string, error) {
	if strings.TrimSpace(out) == "" {
		return filepath.Abs(DefaultPath(now))
	}
	abs, err := filepath.Abs(out)
	if err != nil {
		return "", err
	}
	name := DefaultPath(now)
	if info, err := os.Stat(abs); err == nil && info.IsDir() {
		return filepath.Join(abs, name), nil
	}
	if strings.HasSuffix(abs, string(filepath.Separator)) {
		return filepath.Join(abs, name), nil
	}
	return abs, nil
}

type Summary struct {
	Config   run.Config
	Results  []run.Result
	Passes   int
	Failures int
	Timeouts int
	Elapsed  time.Duration
	Passed   bool
	Started  time.Time
	Finished time.Time
}

func Write(path string, s Summary) error {
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		return err
	}

	var b strings.Builder
	b.Grow(256 + len(s.Results)*48)
	rule := strings.Repeat("=", 48)
	b.WriteString("RaceGrader Run Report\n")
	b.WriteString(rule)
	b.WriteByte('\n')
	fmt.Fprintf(&b, "Kernel:   %s\n", s.Config.Kernel)
	fmt.Fprintf(&b, "Repeat:   %d\n", s.Config.Repeat)
	fmt.Fprintf(&b, "Timeout:  %ds\n", s.Config.Timeout)
	fmt.Fprintf(&b, "CPUs:     %d\n", s.Config.CPUs)
	fmt.Fprintf(&b, "Chaos:    %d\n", s.Config.Chaos)
	fmt.Fprintf(&b, "Command:  %s\n", s.Config.Command)
	if s.Config.Seed == 0 {
		b.WriteString("Seed:     unset (time-based base)\n")
	} else {
		fmt.Fprintf(&b, "Seed:     %d\n", s.Config.Seed)
	}
	fmt.Fprintf(&b, "Started:  %s\n", s.Started.Format(time.RFC3339))
	fmt.Fprintf(&b, "Finished: %s\n", s.Finished.Format(time.RFC3339))
	fmt.Fprintf(&b, "Elapsed:  %s\n", s.Elapsed.Round(time.Millisecond))
	fmt.Fprintf(&b, "Pass:     %d\n", s.Passes)
	fmt.Fprintf(&b, "Fail:     %d\n", s.Failures)
	fmt.Fprintf(&b, "Timeout:  %d\n", s.Timeouts)
	if s.Passed {
		b.WriteString("Result:   PASS\n")
	} else {
		b.WriteString("Result:   FAIL\n")
	}
	b.WriteString("\nSimulations\n")
	b.WriteString(rule)
	b.WriteByte('\n')
	for _, r := range s.Results {
		fmt.Fprintf(&b, "Simulation #%-4d  %-7s  seed=%-4d  %8s  %s\n",
			r.Index, outcomeLabel(r.Outcome), r.Seed, r.Duration.Round(time.Millisecond), r.Detail)
	}
	return os.WriteFile(path, []byte(b.String()), 0o644)
}

func outcomeLabel(o run.Outcome) string {
	switch o {
	case run.OutcomePass:
		return "Pass"
	case run.OutcomeFail:
		return "Fail"
	case run.OutcomeTimeout:
		return "Timeout"
	default:
		return string(o)
	}
}
