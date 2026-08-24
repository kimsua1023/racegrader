package headless

import (
	"context"
	"fmt"
	"os"
	"time"

	"racegrader/cli/internal/report"
	"racegrader/cli/internal/run"
)

func Run(cfg run.Config) error {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	events := make(chan run.Event, 64)
	go run.NewRunner(cfg).Run(ctx, events)

	started := time.Now()
	results := make([]run.Result, 0, cfg.Repeat)
	passes, failures, timeouts := 0, 0, 0

	for ev := range events {
		switch ev.Kind {
		case run.EventStarted:
		case run.EventResult:
			results = append(results, ev.Result)
			switch ev.Result.Outcome {
			case run.OutcomePass:
				passes++
			case run.OutcomeFail:
				failures++
			case run.OutcomeTimeout:
				timeouts++
				failures++
			}
			fmt.Fprintf(
				os.Stderr,
				"simulation #%d %s (%s)\n",
				ev.Result.Index,
				ev.Result.Outcome,
				ev.Result.Duration.Round(time.Millisecond),
			)
		case run.EventDone:
			finished := time.Now()
			return report.Write(cfg.Out, report.Summary{
				Config:   cfg,
				Results:  results,
				Passes:   passes,
				Failures: failures,
				Timeouts: timeouts,
				Elapsed:  finished.Sub(started),
				Passed:   failures == 0,
				Started:  started,
				Finished: finished,
			})
		case run.EventError:
			return ev.Err
		}
	}
	return fmt.Errorf("racegrader: runner stopped unexpectedly")
}
