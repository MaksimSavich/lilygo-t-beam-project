import os
import pandas as pd
from datetime import datetime

def save_reception_data(reception_data, file_prefix):
    """
    Save the received data as a Parquet file in the receiver_tests folder.
    """
    directory = "receiver_tests"
    os.makedirs(directory, exist_ok=True)
    
    # Append the date and time to the filename
    date_str = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    parquet_file = os.path.join(directory, f"{file_prefix}_{date_str}.parquet")
    
    df = pd.DataFrame(reception_data)
    df.to_parquet(parquet_file, index=False)
    
