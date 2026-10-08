# Plot configuration constants
TICK_SIZE = 20
LEGEND_SEED0_SIZE = 28
LEGEND_SIZE = 32
XLABEL_SIZE = 28
FONT_SIZE = 30

# Data column names
LOSS_COLUMN = "loss"
UPPER_BOUND_COLUMN = "upper_bound"

# Dataset name mapping
DATASET_NAMES = {
    ("uniform", "uint64", 100000): (0, rf'$\mathbf{{Uniform}}$'),
    ("normal", "uint64", 100000): (1, rf'$\mathbf{{Normal}}$'),
    ("exponential", "uint64", 100000): (2, rf'$\mathbf{{Exponential}}$'),
    ("books", "uint64", 0): (5, rf'$\mathbf{{Amzn}}$'),
    ("fb", "uint64", 0): (6, rf'$\mathbf{{Face}}$'),
    ("osm", "uint64", 0): (7, rf'$\mathbf{{Osmc}}$')
}

DATASET_NAMES_BRUTE_FORCE = {
    ("uniform", "uint64", 1000): (0, rf'$\mathbf{{Uniform}}$'),
    ("normal", "uint64", 1000): (1, rf'$\mathbf{{Normal}}$'),
    ("exponential", "uint64", 1000): (2, rf'$\mathbf{{Exponential}}$'),
    ("books", "uint64", 0): (5, rf'$\mathbf{{Amzn}}$'),
    ("fb", "uint64", 0): (6, rf'$\mathbf{{Face}}$'),
    ("osm", "uint64", 0): (7, rf'$\mathbf{{Osmc}}$')
}

MIN_N = 100
MAX_N = 10000

# Dataset order definition
DATASET_ORDER = {
    ("uniform", "uint64"): (0, "Uniform"),
    ("normal", "uint64"): (1, "Normal"),
    ("exponential", "uint64"): (2, "Exponential"),
    ("books", "uint64"): (5, "Amzn"),
    ("fb", "uint64"): (6, "Face"),
    ("osm", "uint64"): (7, "Osmc")
}

# Artificial dataset names
ARTIFICIAL_DATASET_NAMES = {
    ("uniform", "uint64"): (0, rf'$\mathbf{{Uniform}}$'),
    ("normal", "uint64"): (1, rf'$\mathbf{{Normal}}$'),
    ("exponential", "uint64"): (2, rf'$\mathbf{{Exponential}}$'),
}

BOXPLOT = True
ERROR_BARS = False

def get_title(dataset_name, data_type, R_value):
    """Generate title using DATASET_NAMES mapping"""
    if R_value is None:
        R_value = 0
    if (dataset_name, data_type, R_value) in DATASET_NAMES:
        return DATASET_NAMES[(dataset_name, data_type, R_value)][1]
    else:
        # Fallback for unknown combinations
        if R_value == 0 or R_value is None:
            if dataset_name == "books":
                return rf'$\mathbf{{Amzn~({data_type})}}$'
            else:
                return rf'$\mathbf{{{dataset_name.title()}}}$'
        else:
            return rf'$\mathbf{{{dataset_name.title()}~(R={int(R_value)})}}$'

def calc_widths_for_boxplot(values):
    """Calculate widths for boxplot based on values"""
    import numpy as np
    return np.array(values) * 0.3