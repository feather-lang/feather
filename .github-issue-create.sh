#!/bin/bash
# Script to create GitHub issue with benchmark results
# Run this after ensuring gh CLI is authenticated

TITLE="Performance Benchmark Results - 2026-01-03"
BODY_FILE="BENCHMARK_RESULTS.md"

# Check if gh is available and authenticated
if ! command -v gh &> /dev/null; then
    echo "Error: gh CLI is not installed or not in PATH"
    echo "Install it with: mise use -g gh@latest"
    exit 1
fi

if ! gh auth status &> /dev/null; then
    echo "Error: gh CLI is not authenticated"
    echo "Run: gh auth login"
    exit 1
fi

# Create the issue
echo "Creating GitHub issue..."
gh issue create \
    --repo feather-lang/feather \
    --title "$TITLE" \
    --body-file "$BODY_FILE" \
    --label "performance" \
    --label "benchmarks"

if [ $? -eq 0 ]; then
    echo "✓ Issue created successfully!"
else
    echo "✗ Failed to create issue"
    exit 1
fi
