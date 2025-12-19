package main

import (
	"fmt"
	"os"
	"path/filepath"

	"gopkg.in/yaml.v3"
)

// LoadFeatures loads all features from spec/features.yaml
func LoadFeatures() ([]Feature, error) {
	data, err := os.ReadFile(filepath.Join("spec", "features.yaml"))
	if err != nil {
		return nil, fmt.Errorf("reading features.yaml: %w", err)
	}

	var ff FeaturesFile
	if err := yaml.Unmarshal(data, &ff); err != nil {
		return nil, fmt.Errorf("parsing features.yaml: %w", err)
	}

	return ff.Features, nil
}

// GetFeature returns a feature by ID
func GetFeature(featureID string) (*Feature, error) {
	features, err := LoadFeatures()
	if err != nil {
		return nil, err
	}

	for _, f := range features {
		if f.ID == featureID {
			return &f, nil
		}
	}

	return nil, fmt.Errorf("feature %q not found", featureID)
}

// ListFeatures prints all features and their status
func ListFeatures() error {
	features, err := LoadFeatures()
	if err != nil {
		return err
	}

	fmt.Println("Feature Status:")
	fmt.Println("===============")
	for _, f := range features {
		status := f.Status
		if status == "" {
			status = "pending"
		}
		fmt.Printf("[%-10s] %-15s %s\n", status, f.ID, f.Description)
	}

	return nil
}

// ShowDeps prints dependencies for a feature
func ShowDeps(featureID string) error {
	f, err := GetFeature(featureID)
	if err != nil {
		return err
	}

	fmt.Printf("Dependencies for %s:\n", featureID)
	if len(f.Depends) == 0 {
		fmt.Println("  (none)")
	} else {
		for _, dep := range f.Depends {
			fmt.Printf("  - %s\n", dep)
		}
	}

	return nil
}
