package cmd

import (
	"fmt"
	"os"

	"github.com/spf13/cobra"
)

var rootCmd = &cobra.Command{
	Use:   "racegrader",
	Short: "Repeat-run xv6 kernels to catch flaky concurrency bugs",
	Long: `RaceGrader shakes scheduling timing across many xv6 runs and reports
how often the kernel fails (panic, assert, invariant break).

One pass is not a grade.`,
}

func Execute() error {
	return rootCmd.Execute()
}

func init() {
	rootCmd.SilenceUsage = true
	rootCmd.SetOut(os.Stdout)
	rootCmd.SetErr(os.Stderr)
}

func die(format string, args ...any) error {
	return fmt.Errorf(format, args...)
}
