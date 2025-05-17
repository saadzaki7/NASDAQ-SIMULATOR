#!/usr/bin/env python3
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
import argparse
from pathlib import Path
import os
import plotly.graph_objects as go
from plotly.subplots import make_subplots


def parse_arguments():
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(
        description="Visualize order book data from CSV files."
    )
    parser.add_argument(
        "input_dir", 
        type=str, 
        help="Directory containing order book CSV files"
    )
    parser.add_argument(
        "-s", "--stocks", 
        type=str, 
        nargs="+", 
        help="Filter for specific stock symbols"
    )
    parser.add_argument(
        "-o", "--output-dir", 
        type=str, 
        default=None, 
        help="Directory to save output files (defaults to input_dir/visualizations)"
    )
    parser.add_argument(
        "--interactive", 
        action="store_true", 
        help="Generate interactive Plotly visualizations"
    )
    
    return parser.parse_args()


def load_data(input_dir, stocks=None):
    """
    Load order book data from CSV files.
    
    Args:
        input_dir: Directory containing CSV files
        stocks: List of stock symbols to filter for
        
    Returns:
        Dictionary of DataFrames with stock symbols as keys
    """
    input_path = Path(input_dir)
    data = {}
    
    # Load summary statistics
    summary_path = input_path / "summary_statistics.csv"
    if summary_path.exists():
        data["summary"] = pd.read_csv(summary_path)
        
        # Filter by stocks if specified
        if stocks:
            data["summary"] = data["summary"][data["summary"]["stock"].isin(stocks)]
    
    # Get list of stocks to process
    if stocks:
        stock_list = stocks
    elif "summary" in data:
        stock_list = data["summary"]["stock"].tolist()
    else:
        # Try to infer from filenames
        stock_list = []
        for file in input_path.glob("*_order_book.csv"):
            stock = file.name.split("_order_book.csv")[0]
            stock_list.append(stock)
    
    # Load data for each stock
    for stock in stock_list:
        stock_data = {}
        
        # Order book
        ob_path = input_path / f"{stock}_order_book.csv"
        if ob_path.exists():
            stock_data["order_book"] = pd.read_csv(ob_path)
        
        # Price history
        price_path = input_path / f"{stock}_price_history.csv"
        if price_path.exists():
            stock_data["price_history"] = pd.read_csv(price_path)
        
        if stock_data:
            data[stock] = stock_data
    
    return data


def create_order_book_visualization(order_book_df, stock, output_dir, interactive=False):
    """
    Create visualization of the order book.
    
    Args:
        order_book_df: DataFrame containing order book data
        stock: Stock symbol
        output_dir: Directory to save output files
        interactive: Whether to generate interactive Plotly visualizations
    """
    if order_book_df.empty:
        return
    
    # Ensure output directory exists
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)
    
    # Prepare data
    bids = order_book_df[order_book_df["side"] == "bid"].sort_values("price", ascending=False)
    asks = order_book_df[order_book_df["side"] == "ask"].sort_values("price")
    
    if interactive:
        # Create interactive order book visualization with Plotly
        fig = go.Figure()
        
        # Add bids
        fig.add_trace(go.Bar(
            x=bids["price"],
            y=bids["volume"],
            name="Bids",
            marker_color="green",
            opacity=0.7
        ))
        
        # Add asks
        fig.add_trace(go.Bar(
            x=asks["price"],
            y=asks["volume"],
            name="Asks",
            marker_color="red",
            opacity=0.7
        ))
        
        # Update layout
        fig.update_layout(
            title=f"Order Book for {stock}",
            xaxis_title="Price",
            yaxis_title="Volume",
            barmode="group",
            template="plotly_white"
        )
        
        # Save as HTML
        fig.write_html(output_path / f"{stock}_order_book.html")
    else:
        # Create static visualization with Matplotlib
        plt.figure(figsize=(12, 6))
        
        # Plot bids in green
        plt.bar(bids["price"], bids["volume"], color="green", alpha=0.7, label="Bids")
        
        # Plot asks in red
        plt.bar(asks["price"], asks["volume"], color="red", alpha=0.7, label="Asks")
        
        plt.title(f"Order Book for {stock}")
        plt.xlabel("Price")
        plt.ylabel("Volume")
        plt.legend()
        plt.grid(True, alpha=0.3)
        plt.tight_layout()
        
        # Save figure
        plt.savefig(output_path / f"{stock}_order_book.png")
        plt.close()


def create_price_history_visualization(price_history_df, stock, output_dir, interactive=False):
    """
    Create visualization of price history.
    
    Args:
        price_history_df: DataFrame containing price history data
        stock: Stock symbol
        output_dir: Directory to save output files
        interactive: Whether to generate interactive Plotly visualizations
    """
    if price_history_df.empty:
        return
    
    # Ensure output directory exists
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)
    
    # Convert timestamp to datetime if it's numeric
    if "timestamp" in price_history_df.columns and pd.api.types.is_numeric_dtype(price_history_df["timestamp"]):
        # Convert nanoseconds to seconds and then to datetime
        price_history_df["datetime"] = pd.to_datetime(price_history_df["timestamp"] / 1e9, unit="s")
    else:
        price_history_df["datetime"] = pd.to_datetime(price_history_df["timestamp"])
    
    if interactive:
        # Create interactive price history visualization with Plotly
        fig = make_subplots(rows=2, cols=1, 
                           shared_xaxes=True, 
                           vertical_spacing=0.1,
                           subplot_titles=(f"Price History for {stock}", "Bid-Ask Spread"))
        
        # Add mid price
        fig.add_trace(
            go.Scatter(
                x=price_history_df["datetime"],
                y=price_history_df["mid"],
                mode="lines",
                name="Mid Price",
                line=dict(color="blue", width=2)
            ),
            row=1, col=1
        )
        
        # Add bid and ask prices
        fig.add_trace(
            go.Scatter(
                x=price_history_df["datetime"],
                y=price_history_df["bid"],
                mode="lines",
                name="Bid Price",
                line=dict(color="green", width=1)
            ),
            row=1, col=1
        )
        
        fig.add_trace(
            go.Scatter(
                x=price_history_df["datetime"],
                y=price_history_df["ask"],
                mode="lines",
                name="Ask Price",
                line=dict(color="red", width=1)
            ),
            row=1, col=1
        )
        
        # Add spread
        price_history_df["spread"] = price_history_df["ask"] - price_history_df["bid"]
        fig.add_trace(
            go.Scatter(
                x=price_history_df["datetime"],
                y=price_history_df["spread"],
                mode="lines",
                name="Spread",
                line=dict(color="purple", width=1.5)
            ),
            row=2, col=1
        )
        
        # Update layout
        fig.update_layout(
            height=800,
            template="plotly_white",
            legend=dict(orientation="h", yanchor="bottom", y=1.02, xanchor="right", x=1)
        )
        
        # Update y-axis labels
        fig.update_yaxes(title_text="Price", row=1, col=1)
        fig.update_yaxes(title_text="Spread", row=2, col=1)
        
        # Save as HTML
        fig.write_html(output_path / f"{stock}_price_history.html")
    else:
        # Create static visualization with Matplotlib
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 10), sharex=True, gridspec_kw={"height_ratios": [3, 1]})
        
        # Plot prices
        ax1.plot(price_history_df["datetime"], price_history_df["mid"], label="Mid Price", color="blue", linewidth=2)
        ax1.plot(price_history_df["datetime"], price_history_df["bid"], label="Bid Price", color="green", linewidth=1)
        ax1.plot(price_history_df["datetime"], price_history_df["ask"], label="Ask Price", color="red", linewidth=1)
        
        ax1.set_title(f"Price History for {stock}")
        ax1.set_ylabel("Price")
        ax1.legend()
        ax1.grid(True, alpha=0.3)
        
        # Plot spread
        price_history_df["spread"] = price_history_df["ask"] - price_history_df["bid"]
        ax2.plot(price_history_df["datetime"], price_history_df["spread"], color="purple", linewidth=1.5)
        ax2.set_title("Bid-Ask Spread")
        ax2.set_xlabel("Time")
        ax2.set_ylabel("Spread")
        ax2.grid(True, alpha=0.3)
        
        plt.tight_layout()
        
        # Save figure
        plt.savefig(output_path / f"{stock}_price_history.png")
        plt.close()


def create_summary_visualization(summary_df, output_dir, interactive=False):
    """
    Create summary visualizations.
    
    Args:
        summary_df: DataFrame containing summary statistics
        output_dir: Directory to save output files
        interactive: Whether to generate interactive Plotly visualizations
    """
    if summary_df.empty:
        return
    
    # Ensure output directory exists
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)
    
    # Create visualizations for various metrics
    metrics = [
        ("spread_pct", "Spread Percentage by Stock", "Spread %"),
        ("mid_price", "Mid Price by Stock", "Price"),
        ("imbalance", "Order Book Imbalance by Stock", "Imbalance (Bid-Ask)/(Bid+Ask)"),
        ("price_volatility", "Price Volatility by Stock", "Volatility (Std Dev)")
    ]
    
    for metric, title, ylabel in metrics:
        if metric not in summary_df.columns:
            continue
            
        # Filter out NaN values
        plot_df = summary_df.dropna(subset=[metric])
        if plot_df.empty:
            continue
            
        # Sort by metric value
        plot_df = plot_df.sort_values(metric)
        
        if interactive:
            # Create interactive bar chart with Plotly
            fig = go.Figure()
            
            fig.add_trace(go.Bar(
                x=plot_df["stock"],
                y=plot_df[metric],
                marker_color="royalblue"
            ))
            
            fig.update_layout(
                title=title,
                xaxis_title="Stock",
                yaxis_title=ylabel,
                template="plotly_white"
            )
            
            # Save as HTML
            fig.write_html(output_path / f"summary_{metric}.html")
        else:
            # Create static bar chart with Matplotlib
            plt.figure(figsize=(12, 6))
            
            plt.bar(plot_df["stock"], plot_df[metric], color="royalblue")
            
            plt.title(title)
            plt.xlabel("Stock")
            plt.ylabel(ylabel)
            plt.xticks(rotation=45, ha="right")
            plt.grid(True, alpha=0.3)
            plt.tight_layout()
            
            # Save figure
            plt.savefig(output_path / f"summary_{metric}.png")
            plt.close()


def main():
    """Main function to run the visualization."""
    args = parse_arguments()
    
    # Set output directory
    if args.output_dir:
        output_dir = args.output_dir
    else:
        output_dir = os.path.join(args.input_dir, "visualizations")
    
    # Load data
    data = load_data(args.input_dir, args.stocks)
    
    if not data:
        print(f"No data found in {args.input_dir}")
        return
    
    # Create visualizations
    if "summary" in data:
        print("Creating summary visualizations...")
        create_summary_visualization(data["summary"], output_dir, args.interactive)
    
    # Process each stock
    for stock, stock_data in data.items():
        if stock == "summary":
            continue
            
        print(f"Creating visualizations for {stock}...")
        
        if "order_book" in stock_data:
            create_order_book_visualization(stock_data["order_book"], stock, output_dir, args.interactive)
        
        if "price_history" in stock_data:
            create_price_history_visualization(stock_data["price_history"], stock, output_dir, args.interactive)
    
    print(f"Visualizations saved to {output_dir}")


if __name__ == "__main__":
    main()
