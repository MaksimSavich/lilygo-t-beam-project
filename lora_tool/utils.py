def parse_boolean_input(value: str) -> bool:
    true_values = ["true", "t", "yes", "y", "1"]
    false_values = ["false", "f", "no", "n", "0"]

    value = value.lower().strip()  # Normalize the input
    if value in true_values:
        return True
    elif value in false_values:
        return False
    else:
        raise ValueError(f"Invalid boolean input: {value}. Expected 'true' or 'false'.")
