import pandas as pd
import os
import json
import glob

def load_upper_bound(base_dir):
    """
    Load upper bounds from JSON files
    Read results from upper_bound directory for each algorithm
    """
    upper_bound_dir = os.path.join(base_dir, "results", "upper_bound")
    
    if not os.path.exists(upper_bound_dir):
        raise FileNotFoundError(f"Upper bound directory not found: {upper_bound_dir}")
    
    print(f"Loading upper bounds from: {upper_bound_dir}")
    
    # Get files matching JSON file pattern
    json_files = glob.glob(os.path.join(upper_bound_dir, "**/*.json"), recursive=True)
    
    if not json_files:
        raise FileNotFoundError("No upper bound JSON files found")
    
    results = []
    
    for json_file in json_files:
        try:
            with open(json_file, 'r') as f:
                data = json.load(f)
            
            # Check if required keys exist
            expected_keys = ["dataset_name", "n", "R", "seed", "data_type", "lambda", "algorithm", "mse_upper_bound"]
            if all(key in data for key in expected_keys):
                # Rename mse_upper_bound to upper_bound
                data['upper_bound'] = data.pop('mse_upper_bound')
                # Remove time key (not needed in results)
                if 'time' in data:
                    del data['time']
                results.append(data)
            else:
                print(f"Warning: Missing keys in {json_file}")
                
        except (json.JSONDecodeError, IOError) as e:
            print(f"Error reading {json_file}: {e}")
    
    if not results:
        raise FileNotFoundError("No valid upper bound results found")
    
    df = pd.DataFrame(results)
    
    # Sort required columns (include algorithm column)
    expected_columns = ["dataset_name", "n", "R", "seed", "data_type", "lambda", "upper_bound", "algorithm"]
    df = df[expected_columns]
    
    # Handle duplicate data (take average if same parameter set)
    duplicate_key_columns = ["dataset_name", "n", "R", "seed", "data_type", "lambda", "algorithm"]
    duplicates = df.duplicated(subset=duplicate_key_columns, keep=False)
    
    if duplicates.any():
        print(f"Found {duplicates.sum()} duplicate upper bound entries. Taking average of duplicate values.")
        # If there are duplicates, take the average
        df = df.groupby(duplicate_key_columns, as_index=False)['upper_bound'].mean()
        # Reorder columns
        df = df[expected_columns]
    
    return df

if __name__ == "__main__":
    base_dir = ".."
    upper_bound_df = load_upper_bound(base_dir)
    upper_bound_df.to_csv("../results/upper_bound.csv", index=False)
