import pandas as pd
import os
import json
import glob

def load_loss(base_dir, duplicate_allowed=False, approach=None):
    """
    Load results from JSON files
    Combine data from loss directory (lambda=0) and inject_poison directory (lambda>0)
    
    Args:
        base_dir: Base directory containing results
        duplicate_allowed: If True, load from loss_duplicate_allowed directory
        approach: Specific approach to load ("original", "consecutive_w_endpoints",
                 "consecutive_w_endpoints_using_relaxed_solution",
                 "consecutive_w_endpoints_duplicate_allowed", "duplicate_allowed",
                 "random", or None for original)
    """
    # Map approach names to directory paths
    approach_dirs = {
        "original": "loss",
        "consecutive_w_endpoints": "loss_consecutive_w_endpoints",
        "consecutive_w_endpoints_using_relaxed_solution": "loss_consecutive_w_endpoints_using_relaxed_solution",
        "consecutive_w_endpoints_duplicate_allowed": "loss_consecutive_w_endpoints_duplicate_allowed",
        "duplicate_allowed": "loss_duplicate_allowed",
        "random": "loss_random"
    }
    
    if approach is None:
        if duplicate_allowed:
            approach = "duplicate_allowed"
        else:
            approach = "original"
    
    if approach not in approach_dirs:
        raise ValueError(f"Unknown approach: {approach}. Available approaches: {list(approach_dirs.keys())}")
    
    loss_dir = os.path.join(base_dir, "results", approach_dirs[approach])
    
    results = []
    expected_keys = [
        "dataset_name", "n", "R", "seed", "data_type", "lambda",
        "loss", "max_point_error_MSE_train", "max_point_error_minimax_train", "mean_point_error_MSE_train", "mean_log_point_error_MSE_train",
        "loss_on_legitimate", "max_point_error_MSE_train_on_legitimate", "max_point_error_minimax_train_on_legitimate", "mean_point_error_MSE_train_on_legitimate", "mean_log_point_error_MSE_train_on_legitimate"
    ]
    
    # Load data from loss directory (lambda=0)
    if os.path.exists(loss_dir):
        print(f"Loading legitimate results (lambda=0) from: {loss_dir}")
        json_files = glob.glob(os.path.join(loss_dir, "**/*.json"), recursive=True)
        
        for json_file in json_files:
            try:
                with open(json_file, 'r') as f:
                    data = json.load(f)
                
                # Check if required keys exist
                if all(key in data for key in expected_keys):
                    results.append(data)
                else:
                    print(f"Warning: Missing keys in {json_file}")
                    
            except (json.JSONDecodeError, IOError) as e:
                print(f"Error reading {json_file}: {e}")
    else:
        print(f"Loss directory not found: {loss_dir}")
    
    if not results:
        raise FileNotFoundError("No valid results found")
    
    df = pd.DataFrame(results)
    
    # Sort required columns
    df = df[expected_keys]
    
    # Handle duplicate data (take average if same parameter set)
    duplicate_key_columns = ["dataset_name", "n", "R", "seed", "data_type", "lambda"]
    duplicates = df.duplicated(subset=duplicate_key_columns, keep=False)
    
    if duplicates.any():
        print(f"Found {duplicates.sum()} duplicate entries. Taking average of duplicate values.")
        # If there are duplicates, take the average
        df = df.groupby(duplicate_key_columns, as_index=False)['loss'].mean()
        # Reorder columns
        df = df[expected_keys]
    
    return df

if __name__ == "__main__":
    base_dir = ".."
    df = load_loss(base_dir)
    df = df.sort_values(by=["dataset_name", "n", "R", "seed", "data_type", "lambda"])
    df.to_csv("../results/loss.csv", index=False)

