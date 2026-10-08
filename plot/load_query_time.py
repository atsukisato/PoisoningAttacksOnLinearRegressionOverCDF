import pandas as pd
import os
import json
import glob
import re


def _safe_load_json_or_fallback(path):
    """Try to load a JSON file, fall back to regex-based parsing if it's malformed.

    The measure_query_time binary sometimes emits JSON without commas between lines; to be robust
    we try json.load first and if it fails we extract simple key: value pairs.
    """
    try:
        with open(path, 'r') as f:
            return json.load(f)
    except Exception:
        # fallback: extract "key": value pairs (strings or numbers)
        data = {}
        try:
            with open(path, 'r') as f:
                text = f.read()
        except Exception:
            return None

        # match "key": "value" or "key": value (float/int)
        for m in re.finditer(r'"(?P<k>[^"]+)"\s*:\s*(?:"(?P<s>[^"]+)"|(?P<num>-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?))', text):
            k = m.group('k')
            if m.group('s') is not None:
                v = m.group('s')
            else:
                num = m.group('num')
                # parse as int when possible
                if '.' in num or 'e' in num or 'E' in num:
                    v = float(num)
                else:
                    v = int(num)
            data[k] = v

        return data


def load_query_time_results(base_dir, approaches=None):
    """
    Load query time JSON results and return a pandas DataFrame.

    Args:
        base_dir: base path containing the `results/` folder
        approaches: list of approach keys to load. If None, loads common approaches.

    Returns:
        pd.DataFrame with columns: dataset_name, n, R, seed, data_type, lambda,
        avg_query_time_us, avg_std_lower_bound_query_time_us, approach
    """
    if approaches is None:
        approaches = [
            'inject_poison',
            'inject_poison_random',
            'consecutive_w_endpoints',
            'consecutive_w_endpoints_duplicate_allowed',
            'consecutive_w_endpoints_using_relaxed_solution',
            'duplicate_allowed',
            'consecutive'
        ]

    approach_dirs = {
        'inject_poison': 'query_time',
        'inject_poison_random': 'query_time_random',
        'inject_poison_random_std_lower_bound': 'query_time_random',
        'consecutive_w_endpoints': 'query_time_consecutive_w_endpoints',
        'consecutive_w_endpoints_duplicate_allowed': 'query_time_consecutive_w_endpoints_duplicate_allowed',
        'consecutive_w_endpoints_using_relaxed_solution': 'query_time_consecutive_w_endpoints_using_relaxed_solution',
        'duplicate_allowed': 'query_time_duplicate_allowed',
        'consecutive': 'query_time_consecutive'
    }

    all_results = []

    for approach in approaches:
        if approach not in approach_dirs:
            print(f"Warning: Unknown approach '{approach}', skipping")
            continue

        qry_dir = os.path.join(base_dir, 'results', approach_dirs[approach])
        if not os.path.exists(qry_dir):
            print(f"Warning: Directory not found: {qry_dir}")
            continue

        print(f"Loading {approach} query time results from: {qry_dir}")

        json_files = glob.glob(os.path.join(qry_dir, '**/*_query_time.json'), recursive=True)
        # also accept any json files just in case
        if not json_files:
            json_files = glob.glob(os.path.join(qry_dir, '**/*.json'), recursive=True)

        if not json_files:
            print(f"Warning: No JSON files found in {qry_dir}")
            continue

        results = []
        for jf in json_files:
            data = _safe_load_json_or_fallback(jf)
            if not data:
                print(f"Warning: Could not parse {jf}")
                continue

            # expected keys from measure_query_time: dataset_name, n, R, seed, data_type, lambda, avg_query_time_us
            expected = ["dataset_name", "n", "R", "seed", "data_type", "lambda"]
            if not all(k in data for k in expected):
                print(f"Warning: Missing keys in {jf}")
                continue

            # safe extraction of optional keys
            lambda_val = int(data.get('lambda')) if data.get('lambda') is not None else 0
            # If lambda == 0, mark as legitimate (no poisoning)
            approach_val = 'legitimate' if lambda_val == 0 else approach

            entry = {
                'dataset_name': data.get('dataset_name'),
                'n': int(data.get('n')),
                'R': int(data.get('R')),
                'seed': int(data.get('seed')),
                'data_type': data.get('data_type'),
                'lambda': lambda_val,
                'avg_query_time_us': float(data.get('avg_query_time_us')) if data.get('avg_query_time_us') is not None else None,
                'avg_std_lower_bound_query_time_us': float(data.get('avg_std_lower_bound_query_time_us')) if data.get('avg_std_lower_bound_query_time_us') is not None else None,
                'approach': approach_val
            }

            results.append(entry)

        if results:
            all_results.extend(results)
            print(f"Loaded {len(results)} results for {approach}")

    if not all_results:
        raise FileNotFoundError("No valid query time results found")

    df = pd.DataFrame(all_results)

    expected_columns = [
        'dataset_name', 'n', 'R', 'seed', 'data_type', 'lambda',
        'avg_query_time_us', 'avg_std_lower_bound_query_time_us', 'approach'
    ]
    # Ensure all expected columns exist
    for col in expected_columns:
        if col not in df.columns:
            df[col] = None

    return df[expected_columns]


if __name__ == '__main__':
    base = '..'
    df = load_query_time_results(base)
    # df.to_csv('../results/query_time_all.csv', index=False)
    # print(f"Saved query time results to ../results/query_time_all.csv (Total: {len(df)} entries)")

    for col in df.columns:
        unique_vals = df[col].nunique()
        if unique_vals < 20:
            print(f"Column '{col}': {df[col].unique().tolist()} unique values")
        else:
            print(f"Column '{col}': {unique_vals} unique values")

    sample_df = df[
        (df['dataset_name'] == 'books') &
        (df['n'] == 1000) &
        (df['seed'] == 0) &
        (df['lambda'] == 100) &
        (df['R'] == 0)
    ]
    print(sample_df)
