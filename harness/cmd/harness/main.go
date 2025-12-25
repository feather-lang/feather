package main

import (
	"os"

	"github.com/feather-lang/feather/harness"
	"github.com/spf13/cobra"
)

func main() {
	var hostPath string
	var verbose bool
	var namePattern string

	rootCmd := &cobra.Command{
		Use:   "harness",
		Short: "Test harness for feather",
	}

	runCmd := &cobra.Command{
		Use:   "run [flags] <test-files-or-dirs>...",
		Short: "Run test cases",
		Args:  cobra.MinimumNArgs(1),
		Run: func(cmd *cobra.Command, args []string) {
			exitCode := harness.Run(harness.Config{
				HostPath:    hostPath,
				TestPaths:   args,
				NamePattern: namePattern,
				Output:      os.Stdout,
				ErrOutput:   os.Stderr,
				Verbose:     verbose,
			})
			os.Exit(exitCode)
		},
	}
	runCmd.Flags().StringVar(&hostPath, "host", "", "path to the host executable (required)")
	runCmd.MarkFlagRequired("host")
	runCmd.Flags().BoolVarP(&verbose, "verbose", "v", false, "show all test results, not just failures")
	runCmd.Flags().StringVar(&namePattern, "name", "", "regex pattern to filter test names")

	listCmd := &cobra.Command{
		Use:   "list <test-files-or-dirs>...",
		Short: "List all test case names",
		Args:  cobra.MinimumNArgs(1),
		Run: func(cmd *cobra.Command, args []string) {
			exitCode := harness.List(harness.Config{
				TestPaths:   args,
				NamePattern: namePattern,
				Output:      os.Stdout,
				ErrOutput:   os.Stderr,
			})
			os.Exit(exitCode)
		},
	}
	listCmd.Flags().StringVar(&namePattern, "name", "", "regex pattern to filter test names")

	var updateHostPath string
	var updateNamePattern string
	updateCmd := &cobra.Command{
		Use:   "update [flags] <test-files-or-dirs>...",
		Short: "Update test expectations based on actual host output",
		Long: `Update runs tests against the host and updates failing test expectations
to match the actual output. Use this to update tests after intentional behavior changes.`,
		Args: cobra.MinimumNArgs(1),
		Run: func(cmd *cobra.Command, args []string) {
			exitCode := harness.Update(harness.Config{
				HostPath:    updateHostPath,
				TestPaths:   args,
				NamePattern: updateNamePattern,
				Output:      os.Stdout,
				ErrOutput:   os.Stderr,
			})
			os.Exit(exitCode)
		},
	}
	updateCmd.Flags().StringVar(&updateHostPath, "host", "", "path to the host executable (required)")
	updateCmd.MarkFlagRequired("host")
	updateCmd.Flags().StringVar(&updateNamePattern, "name", "", "regex pattern to filter test names")

	rootCmd.AddCommand(runCmd, listCmd, updateCmd)
	rootCmd.Execute()
}
