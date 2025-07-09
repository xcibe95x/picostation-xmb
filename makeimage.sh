#!/bin/bash
# Script equivalent to makeimage.bat

# Run the MKPSxiso with the configuration file
mkpsxiso -y "assets/xiso/isoconfig.xml" -o "build/picostation-menu.bin" -c "build/picostation-menu.cue"

# Successful message
echo "ISO image generated successfully."
