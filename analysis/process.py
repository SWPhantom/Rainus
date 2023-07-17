import matplotlib.pyplot as plt
import pandas as pd
import matplotlib.dates as mdates
import numpy as np
from statsmodels.sandbox.regression.predstd import wls_prediction_std
import statsmodels.api as sm
import argparse

# Argument parsing for optional regression
parser = argparse.ArgumentParser()
parser.add_argument("-r", "--regression", help="Enable regression analysis", action="store_true")
args = parser.parse_args()

# Load data
file_location = '../logs/rainLogOvernight.txt'
df = pd.read_csv(file_location, names=["chipId", "timestamp", "unixtime", "secondstime", "temperature", "humidity"])
df['timestamp'] = pd.to_datetime(df['timestamp'])

# Extract date from the first entry in 'timestamp' column
start_date = df['timestamp'].iloc[0].date()

# Calculate the time difference between entries in seconds
df['time_diff'] = df['timestamp'].diff().dt.total_seconds()

fig, ax = plt.subplots(2, 1, figsize=(15, 10), constrained_layout=True)

# Plot temperature
color = 'tab:red'
ax[0].plot(df['timestamp'], df['temperature'], color=color)
ax[0].set_ylabel('Temperature (°C)', color=color)
ax[0].set_xlabel(f'Time (Starting on {start_date})')


# Plot humidity on a different y-axis
ax_twin = ax[0].twinx()
color = 'tab:blue'
ax_twin.plot(df['timestamp'], df['humidity'], color=color)
ax_twin.set_ylabel('Humidity (%)', color=color)

# Draw vertical lines at the first and last timestamp
ax[0].axvline(df['timestamp'][1], color='green', linestyle='--')
ax[0].axvline(df['timestamp'].iloc[-1], color='green', linestyle='--')


# Plot the time difference between entries
ax[1].scatter(df['timestamp'][1:], df['time_diff'][1:], color='tab:blue', alpha=0.3)
ax[1].set_ylabel('Time Diff (s)', color='tab:blue')
ax[1].set_xlabel(f'Time (Starting on {start_date})')


# Draw a vertical line at the first timestamp
ax[1].axvline(df['timestamp'][1], color='green', linestyle='--') 

# Calculate the number of entries per section
number_of_sections = 6
entries_per_section = len(df) // number_of_sections

# Store the previous timestamp for calculating the elapsed time
prev_timestamp = df['timestamp'][1]
prev_index = 0

# Calculate total elapsed time
total_elapsed_time = (df['timestamp'].iloc[-1] - df['timestamp'].iloc[1]).total_seconds()

# Draw vertical lines to divide the plot into sections with an equal number of entries
for i in range(entries_per_section, len(df), entries_per_section):
    # Calculate the elapsed time in seconds
    elapsed_time = (df['timestamp'].iloc[i] - prev_timestamp).total_seconds()
    # Calculate total elapsed time up until the current timestamp
    total_elapsed_time = (df['timestamp'].iloc[i] - df['timestamp'].iloc[1]).total_seconds()

    # Draw a vertical line and add the number of events and elapsed time as text
    ax[1].axvline(df['timestamp'].iloc[i], color='green', linestyle='--') 
    ax[1].text(df['timestamp'].iloc[i] + pd.Timedelta(minutes=2), ax[1].get_ylim()[1], f"{i-prev_index}/{i} events; {elapsed_time:.1f}/{total_elapsed_time:.1f}s", color='red', rotation=90, va='top') 

    # Update the previous timestamp and index
    prev_timestamp = df['timestamp'].iloc[i]
    prev_index = i

# Plot regression if enabled
if args.regression:
    df = df.dropna()
    model = sm.OLS(df['time_diff'][1:], sm.add_constant(df['unixtime'][1:]))
    result = model.fit()
    prstd, iv_l, iv_u = wls_prediction_std(result)
    ax[1].plot(pd.to_datetime(df['unixtime'][1:], unit='s'), result.fittedvalues, 'g', alpha=0.9, label="OLS")
    ax[1].fill_between(pd.to_datetime(df['unixtime'][1:], unit='s'), iv_l, iv_u, color='g', alpha=.2)

    # Display the regression equation
    ax[1].text(0.05, 0.95, f'y = {result.params[1]:.2e}x + {result.params[0]:.2e}', transform=ax[1].transAxes)

# Format the x-axis to show the time (in hours, minutes, and seconds)
myFmt = mdates.DateFormatter("%H:%M:%S")  
ax[0].xaxis.set_major_formatter(myFmt)
ax[1].xaxis.set_major_formatter(myFmt)

# Set x-ticks to 20-minute intervals
minutes = mdates.MinuteLocator(interval = 20)
ax[0].xaxis.set_major_locator(minutes)
ax[1].xaxis.set_major_locator(minutes)

# Rotate xtick labels for readability
plt.setp(ax[0].get_xticklabels(), rotation=75, ha="right")
plt.setp(ax[1].get_xticklabels(), rotation=75, ha="right")

# Add grid lines
ax[0].grid(True, linestyle='--', alpha=0.6)
ax[1].grid(True, linestyle='--', alpha=0.6)

plt.show()
