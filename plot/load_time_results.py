import pandas as pd
import os
import json
import glob

def load_injection_time_results(base_dir, approaches=None):
    """
    Load time data from injection results (JSON file format)
    
    Args:
        base_dir: Base directory containing results
        approaches: List of approaches to load. If None, loads all available approaches.
                   Available approaches: 'inject_poison', 'consecutive_w_endpoints', 
                   'consecutive_w_endpoints_duplicate_allowed', 'consecutive_w_endpoints_using_relaxed_solution', 'duplicate_allowed'
    """
    if approaches is None:
        approaches = ['inject_poison', 'consecutive_w_endpoints', 'consecutive_w_endpoints_duplicate_allowed', 'consecutive_w_endpoints_using_relaxed_solution', 'duplicate_allowed']
    
    # Map approach names to directory names
    approach_dirs = {
        'inject_poison': 'inject_poison',
        'consecutive_w_endpoints': 'inject_poison_consecutive_w_endpoints',
        'consecutive_w_endpoints_duplicate_allowed': 'inject_poison_consecutive_w_endpoints_duplicate_allowed',
        'consecutive_w_endpoints_using_relaxed_solution': 'inject_poison_consecutive_w_endpoints_using_relaxed_solution',
        'duplicate_allowed': 'inject_poison_duplicate_allowed'
    }
    
    all_results = []
    
    for approach in approaches:
        if approach not in approach_dirs:
            print(f"Warning: Unknown approach '{approach}', skipping")
            continue
            
        injection_dir = os.path.join(base_dir, "results", approach_dirs[approach])
        
        if not os.path.exists(injection_dir):
            print(f"Warning: Directory not found: {injection_dir}")
            continue
        
        print(f"Loading {approach} time results from: {injection_dir}")
        
        # Get files matching JSON file pattern
        json_files = glob.glob(os.path.join(injection_dir, "**/*.json"), recursive=True)
        
        if not json_files:
            print(f"Warning: No JSON files found in {injection_dir}")
            continue
        
        results = []
        
        for json_file in json_files:
            try:
                with open(json_file, 'r') as f:
                    data = json.load(f)
                
                # Check required columns and add
                expected_keys = ["dataset_name", "n", "R", "seed", "data_type", "lambda", "time"]
                if all(key in data for key in expected_keys):
                    # Add approach information
                    data['approach'] = approach
                    results.append(data)
                else:
                    print(f"Warning: Missing keys in {json_file}")
                    
            except (json.JSONDecodeError, IOError) as e:
                print(f"Error reading {json_file}: {e}")
        
        if results:
            all_results.extend(results)
            print(f"Loaded {len(results)} results for {approach}")
    
    if not all_results:
        raise FileNotFoundError("No valid injection time results found")
    
    df = pd.DataFrame(all_results)
    # Select only required columns
    expected_columns = ["dataset_name", "n", "R", "seed", "data_type", "lambda", "time", "approach"]
    df = df[expected_columns]
    
    return df

def load_upper_bound_time_results(base_dir):
    """
    Load time data from upper bound results (JSON file format)
    """
    upper_bound_dir = os.path.join(base_dir, "results", "upper_bound")
    
    if not os.path.exists(upper_bound_dir):
        raise FileNotFoundError(f"Upper bound directory not found: {upper_bound_dir}")
    
    print(f"Loading upper bound time results from: {upper_bound_dir}")
    
    # Get files matching JSON file pattern
    json_files = glob.glob(os.path.join(upper_bound_dir, "**/*.json"), recursive=True)
    
    if not json_files:
        raise FileNotFoundError("No upper bound JSON files found")
    
    results = []
    
    for json_file in json_files:
        try:
            with open(json_file, 'r') as f:
                data = json.load(f)
            
            # Check required columns and add
            expected_keys = ["dataset_name", "n", "R", "seed", "data_type", "lambda", "algorithm", "time"]
            if all(key in data for key in expected_keys):
                results.append(data)
            else:
                print(f"Warning: Missing keys in {json_file}")
                
        except (json.JSONDecodeError, IOError) as e:
            print(f"Error reading {json_file}: {e}")
    
    if not results:
        raise FileNotFoundError("No valid upper bound time results found")
    
    df = pd.DataFrame(results)
    # Select only required columns
    expected_columns = ["dataset_name", "n", "R", "seed", "data_type", "lambda", "time", "algorithm"]
    df = df[expected_columns]
    
    return df

if __name__ == "__main__":
    base_dir = ".."
    injection_time_df = load_injection_time_results(base_dir)
    injection_time_df.to_csv("../results/injection_time.csv", index=False)

    upper_bound_time_df = load_upper_bound_time_results(base_dir)
    upper_bound_time_df.to_csv("../results/upper_bound_time.csv", index=False)
