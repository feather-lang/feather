package main

import (
	"os"

	"github.com/dhamidi/tclc/harness"
	"github.com/spf13/cobra"
)

func main() {
	var hostPath string

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
			})
			os.Exit(exitCode)
		},
	}

	cmd.Flags().StringVar(&hostPath, "host", "", "path to the host executable (required)")
	cmd.MarkFlagRequired("host")

	cmd.Execute()
}
