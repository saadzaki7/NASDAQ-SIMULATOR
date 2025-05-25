#!/bin/bash
# Script to process parser output and feed it into the order book processor

if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <parser_output_file> [num_messages] [output_file] [stocks...]"
    exit 1
fi

PARSER_OUTPUT="$1"
NUM_MESSAGES="${2:-0}"
OUTPUT_FILE="${3:-market_data.jsonl}"
shift 3 2>/dev/null || shift "$#"
STOCKS="$@"

# Check if the order book processor is built
if [ ! -f "./order_book_processor" ]; then
    echo "Building order book processor..."
    ./build.sh
fi

# Run the order book processor with the parser output
./order_book_processor "$PARSER_OUTPUT" "$NUM_MESSAGES" "$OUTPUT_FILE" $STOCKS

echo "Processing complete. Market data saved to $OUTPUT_FILE"
