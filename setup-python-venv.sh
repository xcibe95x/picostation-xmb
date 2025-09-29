#!/usr/bin/env bash
set -e

# Go to the folder where the script is located
cd "$(dirname "$0")"

# Create venv if missing
if [ ! -d "env" ]; then
    echo "[+] Creating virtual environment..."
    python3 -m venv env
fi

# Activate venv
source env/bin/activate

# Upgrade pip and install deps
echo "[+] Upgrading pip..."
python3 -m pip install --upgrade pip

echo "[+] Installing Python dependencies..."
pip install numpy pillow

echo "[+] Virtual environment ready! To activate later, run:"
echo "source env/bin/activate"
