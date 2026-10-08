import pandas as pd
import os
import json
import glob

def load_optimal_poison(base_dir):
    """
    Load optimal poison results from JSON files
    Read results from optimal_poison and optimal_poison_duplicate_allowed directories
    """
    optimal_poison_dir = os.path.join(base_dir, "results", "optimal_poison")
    optimal_poison_duplicate_allowed_dir = os.path.join(base_dir, "results", "optimal_poison_duplicate_allowed")
    
    if not os.path.exists(optimal_poison_dir):
        raise FileNotFoundError(f"Optimal poison directory not found: {optimal_poison_dir}")
    
    if not os.path.exists(optimal_poison_duplicate_allowed_dir):
        raise FileNotFoundError(f"Optimal poison duplicate allowed directory not found: {optimal_poison_duplicate_allowed_dir}")
    
    print(f"Loading optimal poison results from: {optimal_poison_dir}")
    print(f"Loading optimal poison duplicate allowed results from: {optimal_poison_duplicate_allowed_dir}")
    
    # Get files matching JSON file pattern for both directories
    json_files = glob.glob(os.path.join(optimal_poison_dir, "**/*.json"), recursive=True)
    json_files_duplicate_allowed = glob.glob(os.path.join(optimal_poison_duplicate_allowed_dir, "**/*.json"), recursive=True)
    
    if not json_files:
        raise FileNotFoundError("No optimal poison JSON files found")
    
    if not json_files_duplicate_allowed:
        raise FileNotFoundError("No optimal poison duplicate allowed JSON files found")
    
    results = []
    
    # Load regular optimal poison results
    for json_file in json_files:
        try:
            with open(json_file, 'r') as f:
                data = json.load(f)
            
            # Check if required keys exist
            expected_keys = ["dataset_name", "n", "R", "seed", "data_type", "lambda", "algorithm", "loss"]
            if all(key in data for key in expected_keys):
                # Remove time key (not needed in results)
                if 'time' in data:
                    del data['time']
                results.append(data)
            else:
                print(f"Warning: Missing keys in {json_file}")
                
        except (json.JSONDecodeError, IOError) as e:
            print(f"Error reading {json_file}: {e}")
    
    # Load duplicate allowed optimal poison results
    for json_file in json_files_duplicate_allowed:
        try:
            with open(json_file, 'r') as f:
                data = json.load(f)
            
            # Check if required keys exist
            expected_keys = ["dataset_name", "n", "R", "seed", "data_type", "lambda", "algorithm", "loss"]
            if all(key in data for key in expected_keys):
                # Remove time key (not needed in results)
                if 'time' in data:
                    del data['time']
                results.append(data)
            else:
                print(f"Warning: Missing keys in {json_file}")
                
        except (json.JSONDecodeError, IOError) as e:
            print(f"Error reading {json_file}: {e}")
    
    if not results:
        raise FileNotFoundError("No valid optimal poison results found")
    
    df = pd.DataFrame(results)
    
    # Sort required columns (include algorithm column)
    expected_columns = ["dataset_name", "n", "R", "seed", "data_type", "lambda", "loss", "algorithm"]
    df = df[expected_columns]
    
    # Handle duplicate data (take average if same parameter set)
    duplicate_key_columns = ["dataset_name", "n", "R", "seed", "data_type", "lambda", "algorithm"]
    duplicates = df.duplicated(subset=duplicate_key_columns, keep=False)
    
    if duplicates.any():
        print(f"Found {duplicates.sum()} duplicate optimal poison entries. Taking average of duplicate values.")
        # If there are duplicates, take the average
        df = df.groupby(duplicate_key_columns, as_index=False)['loss'].mean()
        # Reorder columns
        df = df[expected_columns]
    
    return df

if __name__ == "__main__":
    base_dir = ".."
    optimal_poison_df = load_optimal_poison(base_dir)
    optimal_poison_df.to_csv("../results/optimal_poison.csv", index=False) 