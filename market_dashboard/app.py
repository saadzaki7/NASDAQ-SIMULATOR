#!/usr/bin/env python3
import dash
from dash import dcc, html, dash_table
from dash.dependencies import Input, Output, State
import plotly.graph_objects as go
from plotly.subplots import make_subplots
import pandas as pd
import numpy as np
import os
import time
import datetime
import argparse
import logging
import json
from pathlib import Path

# Import the time simulator
from time_simulator import TimeSimulator

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger("market_dashboard")

# Default paths
DEFAULT_DATA_PATH = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'output')
DEFAULT_MEGA_CSV = os.path.join(DEFAULT_DATA_PATH, 'all_price_updates.csv')
DEFAULT_SIM_PATH = os.path.join(DEFAULT_DATA_PATH, 'time_simulation')

# Make these variables accessible to the main function
data_path = DEFAULT_DATA_PATH
mega_csv_path = DEFAULT_MEGA_CSV
sim_path = DEFAULT_SIM_PATH

# Initialize time simulator
simulator = None

# Global variables
app = dash.Dash(
    __name__,
    meta_tags=[{"name": "viewport", "content": "width=device-width, initial-scale=1"}],
    title="Market Order Book Dashboard"
)
server = app.server

# Initialize data
def load_data(mega_csv_path, use_simulator=True):
    """Load data from the mega CSV file or time simulator."""
    global simulator, sim_path
    
    try:
        # Check if we should use the time simulator
        if use_simulator and simulator is not None:
            # Use the current time slice from the simulator
            current_slice_path = os.path.join(sim_path, "current_slice.csv")
            if os.path.exists(current_slice_path):
                df = pd.read_csv(current_slice_path)
                
                # Load current time info
                time_info_path = os.path.join(sim_path, "current_time.json")
                if os.path.exists(time_info_path):
                    with open(time_info_path, 'r') as f:
                        time_info = json.load(f)
                        current_time_str = time_info.get('current_time_str', 'Unknown')
                    logger.info(f"Loaded {len(df)} stocks at time {current_time_str} from simulator")
                else:
                    logger.info(f"Loaded {len(df)} stocks from simulator")
                
                return df
        
        # Fall back to loading from the mega CSV file
        if os.path.exists(mega_csv_path):
            df = pd.read_csv(mega_csv_path)
            logger.info(f"Loaded {len(df)} price updates from {mega_csv_path}")
            return df
        else:
            logger.warning(f"Mega CSV file not found at {mega_csv_path}")
            return pd.DataFrame()
    except Exception as e:
        logger.error(f"Error loading data: {e}")
        return pd.DataFrame()

def get_stock_list(df):
    """Get list of unique stocks from the data."""
    if df.empty:
        return []
    return sorted(df['stock'].unique())

def get_latest_prices(df):
    """Get the latest prices for each stock."""
    if df.empty:
        return pd.DataFrame()
    
    # Group by stock and get the latest timestamp for each
    latest = df.groupby('stock')['timestamp'].max().reset_index()
    
    # Merge with original dataframe to get the latest price data for each stock
    latest_prices = pd.merge(df, latest, on=['stock', 'timestamp'])
    
    # Sort by mid price descending
    latest_prices = latest_prices.sort_values('mid', ascending=False)
    
    return latest_prices

def get_stock_data(df, stock):
    """Get data for a specific stock."""
    if df.empty or stock not in df['stock'].unique():
        return pd.DataFrame()
    
    return df[df['stock'] == stock].sort_values('timestamp')

def create_price_chart(stock_data):
    """Create a price chart for a stock."""
    if stock_data.empty:
        return go.Figure()
    
    fig = make_subplots(rows=3, cols=1, 
                       shared_xaxes=True, 
                       vertical_spacing=0.05,
                       row_heights=[0.6, 0.2, 0.2],
                       subplot_titles=("Price", "Volume", "Order Book Imbalance"))
    
    # Add price data
    fig.add_trace(
        go.Scatter(
            x=stock_data['datetime'],
            y=stock_data['mid'],
            mode='lines',
            name='Mid Price',
            line=dict(color='blue', width=2)
        ),
        row=1, col=1
    )
    
    fig.add_trace(
        go.Scatter(
            x=stock_data['datetime'],
            y=stock_data['bid'],
            mode='lines',
            name='Bid Price',
            line=dict(color='green', width=1)
        ),
        row=1, col=1
    )
    
    fig.add_trace(
        go.Scatter(
            x=stock_data['datetime'],
            y=stock_data['ask'],
            mode='lines',
            name='Ask Price',
            line=dict(color='red', width=1)
        ),
        row=1, col=1
    )
    
    # Add volume data
    fig.add_trace(
        go.Bar(
            x=stock_data['datetime'],
            y=stock_data['bid_volume'],
            name='Bid Volume',
            marker_color='green',
            opacity=0.7
        ),
        row=2, col=1
    )
    
    fig.add_trace(
        go.Bar(
            x=stock_data['datetime'],
            y=stock_data['ask_volume'],
            name='Ask Volume',
            marker_color='red',
            opacity=0.7
        ),
        row=2, col=1
    )
    
    # Add imbalance data
    fig.add_trace(
        go.Scatter(
            x=stock_data['datetime'],
            y=stock_data['imbalance'],
            mode='lines',
            name='Order Book Imbalance',
            line=dict(color='purple', width=1.5)
        ),
        row=3, col=1
    )
    
    # Add a reference line at 0 for imbalance
    fig.add_shape(
        type="line",
        x0=stock_data['datetime'].min(),
        y0=0,
        x1=stock_data['datetime'].max(),
        y1=0,
        line=dict(color="black", width=1, dash="dash"),
        row=3, col=1
    )
    
    # Update layout
    fig.update_layout(
        height=800,
        template='plotly_white',
        legend=dict(orientation="h", yanchor="bottom", y=1.02, xanchor="right", x=1),
        margin=dict(l=40, r=40, t=60, b=40)
    )
    
    # Update y-axis labels
    fig.update_yaxes(title_text="Price", row=1, col=1)
    fig.update_yaxes(title_text="Volume", row=2, col=1)
    fig.update_yaxes(title_text="Imbalance", row=3, col=1)
    
    # Update x-axis
    fig.update_xaxes(title_text="Time", row=3, col=1)
    
    return fig

def create_market_overview(latest_prices):
    """Create a market overview chart showing price changes for top stocks."""
    if latest_prices.empty:
        return go.Figure()
    
    # Get top 20 stocks by mid price
    top_stocks = latest_prices.head(20)
    
    fig = go.Figure()
    
    # Add bars for each stock
    fig.add_trace(
        go.Bar(
            x=top_stocks['stock'],
            y=top_stocks['mid'],
            marker_color='blue',
            name='Mid Price'
        )
    )
    
    # Update layout
    fig.update_layout(
        title="Top Stocks by Price",
        xaxis_title="Stock",
        yaxis_title="Mid Price",
        template='plotly_white',
        height=400,
        margin=dict(l=40, r=40, t=60, b=40)
    )
    
    return fig

def create_order_book_snapshot(df, stock):
    """Create an order book snapshot visualization for a stock."""
    if df.empty or stock not in df['stock'].unique():
        return go.Figure()
    
    # Get the latest data for the stock
    stock_data = df[df['stock'] == stock].sort_values('timestamp').iloc[-1]
    
    # Create a figure with two subplots side by side
    fig = make_subplots(rows=1, cols=2, 
                        subplot_titles=("Bid Side", "Ask Side"),
                        specs=[[{"type": "bar"}, {"type": "bar"}]])
    
    # Add bid volume
    fig.add_trace(
        go.Bar(
            x=[stock_data['bid']],
            y=[stock_data['bid_volume']],
            name='Bid Volume',
            marker_color='green',
            orientation='v'
        ),
        row=1, col=1
    )
    
    # Add ask volume
    fig.add_trace(
        go.Bar(
            x=[stock_data['ask']],
            y=[stock_data['ask_volume']],
            name='Ask Volume',
            marker_color='red',
            orientation='v'
        ),
        row=1, col=2
    )
    
    # Update layout
    fig.update_layout(
        title=f"Order Book Snapshot for {stock}",
        height=300,
        template='plotly_white',
        showlegend=True,
        margin=dict(l=40, r=40, t=60, b=40)
    )
    
    # Update axes
    fig.update_xaxes(title_text="Bid Price", row=1, col=1)
    fig.update_xaxes(title_text="Ask Price", row=1, col=2)
    fig.update_yaxes(title_text="Volume", row=1, col=1)
    fig.update_yaxes(title_text="Volume", row=1, col=2)
    
    return fig

# Create app layout
def create_layout(df):
    """Create the dashboard layout."""
    stock_list = get_stock_list(df)
    latest_prices = get_latest_prices(df)
    
    return html.Div(
        [
            # Header
            html.Div(
                [
                    html.H1("Market Order Book Dashboard", className="header-title"),
                    html.P(
                        "Real-time visualization of NASDAQ ITCH 5.0 order book data",
                        className="header-description",
                    ),
                    html.Div(
                        [
                            html.Span("Current Time: ", className="current-time-label"),
                            html.Span(id="current-time", className="current-time-value"),
                        ],
                        className="current-time-container",
                    ),
                ],
                className="header",
            ),
            
            # Main content
            html.Div(
                [
                    # Left column - Stock selector and market overview
                    html.Div(
                        [
                            html.Div(
                                [
                                    html.H3("Select Stock"),
                                    dcc.Dropdown(
                                        id="stock-selector",
                                        options=[{"label": s, "value": s} for s in stock_list],
                                        value=stock_list[0] if stock_list else None,
                                        clearable=False,
                                        className="dropdown",
                                    ),
                                ],
                                className="stock-selector-container",
                            ),
                            html.Div(
                                [
                                    html.H3("Market Overview"),
                                    dcc.Graph(
                                        id="market-overview",
                                        figure=create_market_overview(latest_prices),
                                        config={"displayModeBar": False},
                                    ),
                                ],
                                className="market-overview-container",
                            ),
                        ],
                        className="left-column",
                    ),
                    
                    # Right column - Stock details
                    html.Div(
                        [
                            html.Div(
                                [
                                    html.H3(id="stock-title"),
                                    html.Div(
                                        [
                                            html.Div(
                                                [
                                                    html.Span("Bid: ", className="price-label"),
                                                    html.Span(id="current-bid", className="price-value bid"),
                                                ],
                                                className="price-container",
                                            ),
                                            html.Div(
                                                [
                                                    html.Span("Ask: ", className="price-label"),
                                                    html.Span(id="current-ask", className="price-value ask"),
                                                ],
                                                className="price-container",
                                            ),
                                            html.Div(
                                                [
                                                    html.Span("Mid: ", className="price-label"),
                                                    html.Span(id="current-mid", className="price-value mid"),
                                                ],
                                                className="price-container",
                                            ),
                                            html.Div(
                                                [
                                                    html.Span("Spread: ", className="price-label"),
                                                    html.Span(id="current-spread", className="price-value"),
                                                ],
                                                className="price-container",
                                            ),
                                        ],
                                        className="current-prices-container",
                                    ),
                                ],
                                className="stock-header",
                            ),
                            html.Div(
                                [
                                    html.H3("Price Chart"),
                                    dcc.Graph(
                                        id="price-chart",
                                        config={"displayModeBar": "hover"},
                                    ),
                                ],
                                className="price-chart-container",
                            ),
                            html.Div(
                                [
                                    html.H3("Order Book Snapshot"),
                                    dcc.Graph(
                                        id="order-book-snapshot",
                                        config={"displayModeBar": False},
                                    ),
                                ],
                                className="order-book-snapshot-container",
                            ),
                        ],
                        className="right-column",
                    ),
                ],
                className="main-content",
            ),
            
            # Data refresh controls
            html.Div(
                [
                    html.Button("Refresh Data", id="refresh-button", className="refresh-button"),
                    dcc.Interval(
                        id="interval-component",
                        interval=10*1000,  # in milliseconds (10 seconds)
                        n_intervals=0
                    ),
                    html.Div(id="refresh-status", className="refresh-status"),
                ],
                className="refresh-controls",
            ),
            
            # Store components for data
            dcc.Store(id="store-data"),
            dcc.Store(id="store-selected-stock"),
        ],
        className="app-container",
    )

# Callbacks
@app.callback(
    [
        Output("store-data", "data"),
        Output("refresh-status", "children"),
    ],
    [
        Input("refresh-button", "n_clicks"),
        Input("interval-component", "n_intervals"),
    ],
    [
        State("store-data", "data"),
    ],
)
def update_data(n_clicks, n_intervals, stored_data):
    """Update data from the mega CSV file and advance the time simulator."""
    global mega_csv_path, simulator
    
    # Advance the time simulator if it exists
    if simulator is not None:
        simulator.advance_time()
        status_msg = f"Data refreshed at {time.strftime('%H:%M:%S')} - Simulation time: {pd.to_datetime(simulator.current_time_ns / 1e9, unit='s').strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]}"
    else:
        status_msg = f"Data refreshed at {time.strftime('%H:%M:%S')}"
    
    # Load data from the mega CSV file or time simulator
    df = load_data(mega_csv_path)
    
    if df.empty:
        return None, "No data available. Please check the data file."
    
    # Convert to JSON for storage
    data_json = df.to_json(date_format='iso', orient='split')
    
    # Return the data and a success message
    return data_json, status_msg

@app.callback(
    [
        Output("current-time", "children"),
        Output("stock-title", "children"),
        Output("current-bid", "children"),
        Output("current-ask", "children"),
        Output("current-mid", "children"),
        Output("current-spread", "children"),
        Output("price-chart", "figure"),
        Output("order-book-snapshot", "figure"),
        Output("market-overview", "figure"),
    ],
    [
        Input("store-data", "data"),
        Input("stock-selector", "value"),
    ],
)
def update_charts(data_json, selected_stock):
    """Update charts and displays based on the selected stock."""
    if data_json is None or selected_stock is None:
        return (
            datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "No Stock Selected",
            "N/A",
            "N/A",
            "N/A",
            "N/A",
            go.Figure(),
            go.Figure(),
            go.Figure(),
        )
    
    # Parse the JSON data
    df = pd.read_json(data_json, orient='split')
    
    # Get current time
    current_time = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    
    # Get data for the selected stock
    stock_data = get_stock_data(df, selected_stock)
    
    if stock_data.empty:
        return (
            current_time,
            f"{selected_stock} - No Data",
            "N/A",
            "N/A",
            "N/A",
            "N/A",
            go.Figure(),
            go.Figure(),
            create_market_overview(get_latest_prices(df)),
        )
    
    # Get the latest data point for the selected stock
    latest_data = stock_data.iloc[-1]
    
    # Create the price chart
    price_chart = create_price_chart(stock_data)
    
    # Create the order book snapshot
    order_book_snapshot = create_order_book_snapshot(df, selected_stock)
    
    # Create the market overview
    market_overview = create_market_overview(get_latest_prices(df))
    
    return (
        current_time,
        f"{selected_stock} - {latest_data['datetime']}",
        f"${latest_data['bid']:.4f}",
        f"${latest_data['ask']:.4f}",
        f"${latest_data['mid']:.4f}",
        f"${latest_data['spread']:.4f} ({latest_data['spread_pct']:.2f}%)",
        price_chart,
        order_book_snapshot,
        market_overview,
    )

# Main function
def main():
    """Main function to run the dashboard."""
    parser = argparse.ArgumentParser(description="Market Order Book Dashboard")
    parser.add_argument(
        "--data-path",
        type=str,
        default=DEFAULT_DATA_PATH,
        help="Path to the data directory (default: ../output)"
    )
    parser.add_argument(
        "--mega-csv",
        type=str,
        default=DEFAULT_MEGA_CSV,
        help="Path to the mega CSV file (default: ../output/all_price_updates.csv)"
    )
    parser.add_argument(
        "--port",
        type=int,
        default=8050,
        help="Port to run the dashboard on (default: 8050)"
    )
    parser.add_argument(
        "--debug",
        action="store_true",
        help="Run in debug mode"
    )
    parser.add_argument(
        "--sim-path",
        type=str,
        default=DEFAULT_SIM_PATH,
        help="Path to the time simulation directory (default: ../output/time_simulation)"
    )
    parser.add_argument(
        "--time-step",
        type=float,
        default=5.0,
        help="Time step in seconds for the simulation (default: 5.0)"
    )
    parser.add_argument(
        "--no-simulation",
        action="store_true",
        help="Disable time simulation"
    )
    
    args = parser.parse_args()
    
    global data_path, mega_csv_path, sim_path, simulator
    data_path = args.data_path
    mega_csv_path = args.mega_csv
    sim_path = args.sim_path
    
    # Initialize time simulator if not disabled
    if not args.no_simulation:
        logger.info(f"Initializing time simulator with mega CSV: {mega_csv_path}, output dir: {sim_path}")
        simulator = TimeSimulator(
            mega_csv_path=mega_csv_path,
            output_dir=sim_path,
            time_step_seconds=args.time_step
        )
        # Create initial time slice
        simulator.create_time_slice()
        logger.info("Time simulator initialized and initial time slice created")
    
    # Load initial data
    df = load_data(mega_csv_path)
    
    # Create app layout
    app.layout = create_layout(df)
    
    # Run the app
    app.run(debug=args.debug, port=args.port)

if __name__ == "__main__":
    main()
