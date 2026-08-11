package run

import (
	"bufio"
	"context"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"
	"time"
)

type Config struct {
	Kernel   string
	Repeat   int
	Timeout  int
	Seed     int64
	Out      string
	SkipBoot bool
	CPUs     int
	Chaos    int
	Command  string
}

type Outcome string

const (
	OutcomePass    Outcome = "pass"
	OutcomeFail    Outcome = "fail"
	OutcomeTimeout Outcome = "timeout"
)

type Result struct {
	Index    int
	Outcome  Outcome
	Duration time.Duration
	Detail   string
	Seed     int64
}

type Event struct {
	Kind   EventKind
	Result Result
	Err    error
}

type EventKind int

const (
	EventStarted EventKind = iota
	EventResult
	EventDone
	EventError
)

type Runner struct {
	Cfg      Config
	baseSeed int64
	lastSeed int64
}

func NewRunner(cfg Config) *Runner {
	if cfg.CPUs <= 0 {
		cfg.CPUs = 2
	}
	if cfg.Chaos < 0 {
		cfg.Chaos = 1
	}
	if strings.TrimSpace(cfg.Command) == "" {
		cfg.Command = "cow_test"
	}
	base := cfg.Seed
	if base == 0 {
		base = time.Now().UnixNano() & 0x7fffffff
		if base == 0 {
			base = 1
		}
	}
	return &Runner{Cfg: cfg, baseSeed: base, lastSeed: -1}
}

func (r *Runner) Run(ctx context.Context, out chan<- Event) {
	defer close(out)
	out <- Event{Kind: EventStarted}

	timeout := time.Duration(r.Cfg.Timeout) * time.Second
	for i := 1; i <= r.Cfg.Repeat; i++ {
		select {
		case <-ctx.Done():
			out <- Event{Kind: EventError, Err: ctx.Err()}
			return
		default:
		}

		res := r.one(ctx, i, timeout)
		out <- Event{Kind: EventResult, Result: res}
		if res.Outcome == OutcomeTimeout {
			out <- Event{Kind: EventDone}
			return
		}
	}
	out <- Event{Kind: EventDone}
}

func (r *Runner) runSeed(index int) int64 {
	seed := r.baseSeed + int64(index-1)
	if seed <= 0 {
		seed = 1
	}
	return seed & 0xffffffff
}

func (r *Runner) one(ctx context.Context, index int, timeout time.Duration) Result {
	start := time.Now()
	seed := r.runSeed(index)

	if err := r.ensureBuilt(ctx, seed); err != nil {
		return Result{
			Index:    index,
			Outcome:  OutcomeFail,
			Duration: time.Since(start),
			Detail:   "build: " + err.Error(),
			Seed:     seed,
		}
	}

	res := r.runQEMU(ctx, index, seed, timeout)
	res.Duration = time.Since(start)
	return res
}

func (r *Runner) ensureBuilt(ctx context.Context, seed int64) error {
	dir := r.Cfg.Kernel
	if r.lastSeed != seed {
		for _, rel := range []string{
			"kernel/chaos.o",
			"kernel/printf.o",
			"user/cow_test.o",
			"user/_cow_test",
			"kernel/kernel",
			"fs.img",
		} {
			_ = os.Remove(filepath.Join(dir, rel))
		}
	}

	args := []string{
		"kernel/kernel", "fs.img",
		"CPUS=" + strconv.Itoa(r.Cfg.CPUs),
		"SEED=" + strconv.FormatInt(seed, 10),
		"CHAOS=" + strconv.Itoa(r.Cfg.Chaos),
	}
	cmd := exec.CommandContext(ctx, "make", args...)
	cmd.Dir = dir
	out, err := cmd.CombinedOutput()
	if err != nil {
		msg := strings.TrimSpace(string(out))
		if msg == "" {
			msg = err.Error()
		}
		if len(msg) > 400 {
			msg = msg[len(msg)-400:]
		}
		return fmt.Errorf("%s", msg)
	}
	r.lastSeed = seed
	return nil
}

func (r *Runner) runQEMU(ctx context.Context, index int, seed int64, timeout time.Duration) Result {
	dir := r.Cfg.Kernel
	args := []string{
		"qemu",
		"CPUS=" + strconv.Itoa(r.Cfg.CPUs),
		"SEED=" + strconv.FormatInt(seed, 10),
		"CHAOS=" + strconv.Itoa(r.Cfg.Chaos),
	}
	cmd := exec.CommandContext(ctx, "make", args...)
	cmd.Dir = dir
	cmd.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}

	stdin, err := cmd.StdinPipe()
	if err != nil {
		return Result{Index: index, Outcome: OutcomeFail, Detail: "stdin: " + err.Error(), Seed: seed}
	}
	stdout, err := cmd.StdoutPipe()
	if err != nil {
		return Result{Index: index, Outcome: OutcomeFail, Detail: "stdout: " + err.Error(), Seed: seed}
	}
	cmd.Stderr = cmd.Stdout

	if err := cmd.Start(); err != nil {
		return Result{Index: index, Outcome: OutcomeFail, Detail: "start qemu: " + err.Error(), Seed: seed}
	}

	runCtx, cancel := context.WithTimeout(ctx, timeout)
	defer cancel()

	type verdict struct {
		outcome Outcome
		detail  string
	}
	done := make(chan verdict, 1)

	go func() {
		o, d := scanQEMU(stdout, stdin, r.Cfg.Command)
		done <- verdict{outcome: o, detail: d}
	}()

	var v verdict
	select {
	case <-runCtx.Done():
		killProcessGroup(cmd)
		_ = cmd.Wait()
		if ctx.Err() != nil {
			return Result{Index: index, Outcome: OutcomeFail, Detail: "Cancelled", Seed: seed}
		}
		return Result{
			Index:   index,
			Outcome: OutcomeTimeout,
			Detail:  fmt.Sprintf("Exceeded %s; remaining simulations skipped", timeout),
			Seed:    seed,
		}
	case v = <-done:
		killProcessGroup(cmd)
		_ = cmd.Wait()
	}

	return Result{Index: index, Outcome: v.outcome, Detail: v.detail, Seed: seed}
}

func scanQEMU(stdout io.Reader, stdin io.WriteCloser, command string) (outcome Outcome, detail string) {
	defer stdin.Close()

	reader := bufio.NewReader(stdout)
	var buf strings.Builder
	sent := false
	tmp := make([]byte, 256)

	for {
		n, err := reader.Read(tmp)
		if n > 0 {
			buf.Write(tmp[:n])
			text := buf.String()

			if !sent && shellReady(text) {
				_, _ = io.WriteString(stdin, command+"\n")
				sent = true
			}

			if m := FindMarker(text); m != "" {
				return ClassifyMarker(m)
			}
		}
		if err != nil {
			if !sent {
				return OutcomeFail, "qemu exited before shell prompt"
			}
			snippet := strings.TrimSpace(buf.String())
			if len(snippet) > 240 {
				snippet = snippet[len(snippet)-240:]
			}
			if snippet == "" {
				snippet = "qemu exited without RaceGrader markers"
			}
			return OutcomeFail, snippet
		}
	}
}

func shellReady(text string) bool {
	if strings.Contains(text, "init: starting sh") {
		return true
	}
	trimmed := strings.TrimRight(text, " \t\r\n")
	return strings.HasSuffix(trimmed, "$") || strings.Contains(text, "\n$ ")
}

func FindMarker(text string) string {
	parts := strings.Split(text, "\n")
	limit := len(parts)
	if !strings.HasSuffix(text, "\n") {
		limit--
	}
	for _, line := range parts[:limit] {
		if i := strings.Index(line, "[RACEGRADER_DONE]"); i >= 0 {
			return strings.TrimSpace(line[i:])
		}
		if i := strings.Index(line, "[RACEGRADER_FAIL]"); i >= 0 {
			return strings.TrimSpace(line[i:])
		}
	}
	return ""
}

func ClassifyMarker(line string) (Outcome, string) {
	if strings.HasPrefix(line, "[RACEGRADER_DONE]") {
		return OutcomePass, line
	}
	return OutcomeFail, line
}

func killProcessGroup(cmd *exec.Cmd) {
	if cmd.Process == nil {
		return
	}
	pgid, err := syscall.Getpgid(cmd.Process.Pid)
	if err == nil {
		_ = syscall.Kill(-pgid, syscall.SIGKILL)
		return
	}
	_ = cmd.Process.Kill()
}
