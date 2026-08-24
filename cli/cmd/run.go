package cmd

import (
	"os"
	"path/filepath"
	"time"

	"github.com/spf13/cobra"

	"racegrader/cli/internal/headless"
	"racegrader/cli/internal/report"
	"racegrader/cli/internal/run"
	"racegrader/cli/internal/tui"
)

var (
	flagKernel   string
	flagRepeat   int
	flagTimeout  int
	flagSeed     int64
	flagOut      string
	flagSkipBoot bool
	flagHeadless bool
	flagCPUs     int
	flagChaos    int
	flagCommand  string
)

var runCmd = &cobra.Command{
	Use:   "run",
	Short: "Run repeated kernel simulations",
	Long: `Run walks through --repeat simulations against the xv6 tree at --kernel.
Each run is killed if it exceeds --timeout seconds; when that happens the
remaining repeats are skipped.

Use --headless for CI (plain stderr progress, no Bubble Tea terminal UI).
A report is written to --out when the session finishes.`,
	Example: `  racegrader run --kernel ../kernel --repeat 500 --timeout 10 --headless
  racegrader run --kernel ../kernel --repeat 100 --timeout 5 --seed 42
  racegrader run --kernel ../kernel --cpus 2 --chaos 1 --command cow_test
  racegrader run --kernel ../kernel --repeat 50 --out ./my-run.log --headless
  racegrader run --kernel ../kernel --skip-boot`,
	RunE: func(cmd *cobra.Command, args []string) error {
		cfg, err := buildConfig()
		if err != nil {
			return err
		}
		if flagHeadless {
			return headless.Run(cfg)
		}
		return tui.Start(cfg)
	},
}

func init() {
	runCmd.Flags().StringVar(&flagKernel, "kernel", "", "Path to the xv6 tree under test (required)")
	runCmd.Flags().IntVar(&flagRepeat, "repeat", 100, "How many simulations to run")
	runCmd.Flags().IntVar(&flagTimeout, "timeout", 30, "Per-run timeout in seconds (aborts remaining runs)")
	runCmd.Flags().Int64Var(&flagSeed, "seed", 0, "Base CHAOS_SEED for make qemu (0 = time-based; each run uses seed+i-1)")
	runCmd.Flags().IntVar(&flagCPUs, "cpus", 2, "SMP CPUs passed to make qemu (CPUS=)")
	runCmd.Flags().IntVar(&flagChaos, "chaos", 1, "Chaos engine toggle passed to make qemu (CHAOS=)")
	runCmd.Flags().StringVar(&flagCommand, "command", "cow_test", "xv6 shell command to run each simulation")
	runCmd.Flags().StringVar(&flagOut, "out", "", "Run report path (default: ./racegrader-<timestamp>.log)")
	runCmd.Flags().BoolVar(&flagSkipBoot, "skip-boot", false, "Skip the boot splash screen (TUI only)")
	runCmd.Flags().BoolVar(&flagHeadless, "headless", false, "Headless mode for CI (no interactive terminal UI)")

	_ = runCmd.MarkFlagRequired("kernel")
	rootCmd.AddCommand(runCmd)
}

func buildConfig() (run.Config, error) {
	if flagRepeat < 1 {
		return run.Config{}, die("--repeat must be >= 1")
	}
	if flagTimeout < 1 {
		return run.Config{}, die("--timeout must be >= 1")
	}
	if flagCPUs < 1 {
		return run.Config{}, die("--cpus must be >= 1")
	}
	if flagChaos != 0 && flagChaos != 1 {
		return run.Config{}, die("--chaos must be 0 or 1")
	}

	abs, err := filepath.Abs(flagKernel)
	if err != nil {
		return run.Config{}, die("resolve --kernel: %w", err)
	}
	info, err := os.Stat(abs)
	if err != nil {
		return run.Config{}, die("--kernel %q: %w", flagKernel, err)
	}
	if !info.IsDir() {
		return run.Config{}, die("--kernel must be a directory, got %s", abs)
	}

	outPath, err := report.Resolve(flagOut, time.Now())
	if err != nil {
		return run.Config{}, die("resolve --out: %w", err)
	}

	return run.Config{
		Kernel:   abs,
		Repeat:   flagRepeat,
		Timeout:  flagTimeout,
		Seed:     flagSeed,
		Out:      outPath,
		SkipBoot: flagSkipBoot,
		CPUs:     flagCPUs,
		Chaos:    flagChaos,
		Command:  flagCommand,
	}, nil
}
