package run

import (
	"context"
	"fmt"
	"math/rand"
	"time"
)

type Config struct {
	Kernel   string
	Repeat   int
	Timeout  int
	Seed     int64
	Out      string
	SkipBoot bool
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
	Cfg Config
	rng *rand.Rand
}

func NewRunner(cfg Config) *Runner {
	seed := cfg.Seed
	if seed == 0 {
		seed = time.Now().UnixNano()
	}
	return &Runner{Cfg: cfg, rng: rand.New(rand.NewSource(seed))}
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

func (r *Runner) one(ctx context.Context, index int, timeout time.Duration) Result {
	start := time.Now()
	work := time.Duration(80+r.rng.Intn(320)) * time.Millisecond
	if work > timeout {
		work = timeout
	}

	timer := time.NewTimer(work)
	defer timer.Stop()

	select {
	case <-ctx.Done():
		return Result{Index: index, Outcome: OutcomeFail, Duration: time.Since(start), Detail: "Cancelled"}
	case <-timer.C:
	}

	roll := r.rng.Float64()
	switch {
	case work >= timeout && roll < 0.3:
		return Result{
			Index:    index,
			Outcome:  OutcomeTimeout,
			Duration: time.Since(start),
			Detail:   fmt.Sprintf("Exceeded %s; remaining simulations skipped", timeout),
		}
	case roll < 0.012:
		return Result{Index: index, Outcome: OutcomeFail, Duration: time.Since(start), Detail: "Panic: acquire"}
	default:
		return Result{Index: index, Outcome: OutcomePass, Duration: time.Since(start), Detail: "Ok"}
	}
}
