package harness

import (
	"os"
	"path/filepath"
)

// CollectTestFiles recursively finds all .html test files in the given paths.
// Paths can be files or directories.
func CollectTestFiles(paths []string) ([]string, error) {
	var files []string
	for _, path := range paths {
		info, err := os.Stat(path)
		if err != nil {
			return nil, err
		}

		if info.IsDir() {
			dirFiles, err := collectFromDir(path)
			if err != nil {
				return nil, err
			}
			files = append(files, dirFiles...)
		} else {
			files = append(files, path)
		}
	}
	return files, nil
}

func collectFromDir(dir string) ([]string, error) {
	var files []string
	err := filepath.Walk(dir, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}
		if !info.IsDir() && filepath.Ext(path) == ".html" {
			files = append(files, path)
		}
		return nil
	})
	return files, err
}
