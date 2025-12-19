package main

import (
	"fmt"
	"os"
)

func main() {
	if len(os.Args) < 2 {
		printUsage()
		os.Exit(1)
	}

	cmd := os.Args[1]
	args := os.Args[2:]

	var err error
	switch cmd {
	case "oracle":
		if len(args) == 0 {
			err = GenerateAllOracles()
		} else {
			err = GenerateOracle(args[0])
		}

	case "diff":
		if len(args) == 0 {
			fmt.Fprintln(os.Stderr, "Usage: harness diff <feature>")
			os.Exit(1)
		}
		err = RunDiff(args[0])

	case "prompt":
		if len(args) == 0 {
			fmt.Fprintln(os.Stderr, "Usage: harness prompt <feature>")
			os.Exit(1)
		}
		err = GeneratePrompt(args[0])

	case "loop":
		if len(args) == 0 {
			fmt.Fprintln(os.Stderr, "Usage: harness loop <feature>")
			os.Exit(1)
		}
		err = RunLoop(args[0])

	case "features":
		err = ListFeatures()

	case "deps":
		if len(args) == 0 {
			fmt.Fprintln(os.Stderr, "Usage: harness deps <feature>")
			os.Exit(1)
		}
		err = ShowDeps(args[0])

	case "help", "-h", "--help":
		printUsage()
		os.Exit(0)

	default:
		fmt.Fprintf(os.Stderr, "Unknown command: %s\n", cmd)
		printUsage()
		os.Exit(1)
	}

	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: %v\n", err)
		os.Exit(1)
	}
}

func printUsage() {
	fmt.Println(`TCLC Harness - Test orchestration for TCL Core implementation

Usage: harness <command> [arguments]

Commands:
  oracle [feature]   Generate expected outputs from tclsh
                     Without feature: generate for all features
  
  diff <feature>     Run differential tests against oracle
  
  prompt <feature>   Generate agent prompt for failing tests
  
  loop <feature>     Run interactive feedback loop until all tests pass
  
  features           List all features and their status
  
  deps <feature>     Show dependencies for a feature
  
  help               Show this help message

Environment Variables:
  TCLSH              Path to tclsh (default: tclsh)
  TCLC_INTERP        Path to our interpreter (required for diff)

Examples:
  harness oracle lexer      # Generate oracle for lexer feature
  harness diff lexer        # Compare implementation vs oracle
  harness loop lexer        # Run feedback loop for lexer
  harness features          # List all features`)
}
