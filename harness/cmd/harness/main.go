package main

import (
	"os"

	"github.com/dhamidi/tclc/harness"
	"github.com/spf13/cobra"
)

func main() {
	var hostPath string
	var verbose bool

	cmd := &cobra.Command{
		Use:   "harness [flags] <test-files-or-dirs>...",
		Short: "Test harness for tclc",
		Args:  cobra.MinimumNArgs(1),
		Run: func(cmd *cobra.Command, args []string) {
			exitCode := harness.Run(harness.Config{
				HostPath:  hostPath,
				TestPaths: args,
				Output:    os.Stdout,
				ErrOutput: os.Stderr,
				Verbose:   verbose,
			})
			os.Exit(exitCode)
		},
	}

	cmd.Flags().StringVar(&hostPath, "host", "", "path to the host executable (required)")
	cmd.MarkFlagRequired("host")
	cmd.Flags().BoolVarP(&verbose, "verbose", "v", false, "show all test results, not just failures")

	cmd.Execute()
}
