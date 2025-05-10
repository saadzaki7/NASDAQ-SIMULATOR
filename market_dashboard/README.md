# Market Dashboard for Order Book Simulation

A real-time dashboard for visualizing NASDAQ ITCH 5.0 order book data processed by the order book simulator.

## Overview

This dashboard provides a visual interface to monitor:
- Current prices (bid, ask, mid) for all stocks
- Order book imbalance and volume
- Price movements over time
- Market overview of top stocks

## Components

The market dashboard consists of the following components:

1. **Data Generation Script**: Processes ITCH data and creates a timestamp-sorted mega CSV file
2. **Dash Application**: Interactive web dashboard for visualizing the order book data
3. **CSS Styling**: Modern, responsive design for the dashboard

## Installation

```bash
# Make sure you're in the virtual environment
source ../order_book_env/bin/activate

# Install additional dependencies for the dashboard
pip install dash dash-bootstrap-components
```

## Usage

### Step 1: Generate the Mega CSV File

First, process the ITCH data and generate the timestamp-sorted mega CSV file:

```bash
python generate_data.py /path/to/itch_data.json -n 1000000 -o ../output
```

Options:
- `-n, --num-messages`: Number of messages to process (default: all)
- `-s, --stocks`: Filter for specific stock symbols (e.g., `-s AAPL MSFT`)
- `-o, --output-dir`: Directory to save the mega CSV file (default: `../output`)
- `--verbose`: Enable verbose output

### Step 2: Run the Dashboard

Once the mega CSV file is generated, run the dashboard:

```bash
python app.py
```

Options:
- `--data-path`: Path to the data directory (default: `../output`)
- `--mega-csv`: Path to the mega CSV file (default: `../output/all_price_updates.csv`)
- `--port`: Port to run the dashboard on (default: 8050)
- `--debug`: Run in debug mode

### Step 3: View the Dashboard

Open your web browser and navigate to:
```
http://localhost:8050
```

## Dashboard Features

### Real-time Market Data
- Current time display
- Stock selector dropdown
- Current prices (bid, ask, mid, spread)

### Interactive Charts
- Price chart with bid, ask, and mid prices
- Volume chart showing bid and ask volumes
- Order book imbalance chart
- Market overview of top stocks by price

### Data Refresh
- Auto-refresh every 10 seconds
- Manual refresh button

## Example

To run the dashboard with a subset of the ITCH data focusing on specific stocks:

```bash
# Generate data for AAPL, MSFT, and NFLX with 500,000 messages
python generate_data.py ../itch_raw_file/01302019.NASDAQ_ITCH50.json -n 500000 -s AAPL MSFT NFLX -o ../output

# Run the dashboard
python app.py
```

## Customization

You can customize the dashboard by modifying:
- `assets/style.css`: Change the visual appearance
- `app.py`: Add new visualizations or features
- Update the refresh interval in the app.py file (currently set to 10 seconds)

## License

MIT
