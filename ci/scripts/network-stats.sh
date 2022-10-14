#!/bin/bash -e

if test -z "$1"; then
  echo "Need a file to save/load data from!" >&2
  exit 2
fi

# Get the RX/TX bytes for each interface
declare -A rx tx
for iface in /sys/class/net/*; do
  iface="$(basename "$iface")"
  case "$iface" in
  lo)
    # We don't care about loopback data, ignore
    ;;
  *)
    if test -e /sys/class/net/"$iface"/statistics/rx_bytes; then
      rx["$iface"]="$(cat /sys/class/net/"$iface"/statistics/rx_bytes)"
      tx["$iface"]="$(cat /sys/class/net/"$iface"/statistics/tx_bytes)"
    fi
    ;;
  esac
done

# Compare against data from the previous run, if it exists
if test -e "$1"; then
  declare -A prev_rx prev_tx
  while read -r iface data_rx data_tx; do
    prev_rx["$iface"]="$data_rx"
    prev_tx["$iface"]="$data_tx"
  done < "$1"

  for iface in "${!rx[@]}"; do
    echo "Interface $iface:"
    printf "  RX: %'d bytes since last call (of %'d total)\n" $((rx[$iface] - prev_rx[$iface])) "${rx[$iface]}"
    printf "  TX: %'d bytes since last call (of %'d total)\n" $((tx[$iface] - prev_tx[$iface])) "${tx[$iface]}"
  done
fi

# Save the results for the next run
for iface in "${!rx[@]}"; do
  echo "$iface ${rx[$iface]} ${tx[$iface]}"
done > "$1"
