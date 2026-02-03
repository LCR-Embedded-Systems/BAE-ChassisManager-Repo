#!/bin/bash
LOCKFILE=~/projects/build.lock  # Adjust this path as needed
QUEUEDIR=~/projects/build_queue  # Directory for queue files; adjust as needed
SCRIPT_NAME=$(basename "$0")    # Name of this script for any additional checks

# Ensure queue directory exists
mkdir -p "$QUEUEDIR"

# Generate a unique queue file name: timestamp_nanoseconds_PID
TIMESTAMP=$(date +%Y%m%d%H%M%S%N)
QUEUEFILE="$QUEUEDIR/${TIMESTAMP}_$$"

# Add to queue
touch "$QUEUEFILE" || { echo "Error: Failed to create queue file."; exit 1; }

# Function to count builds ahead (files with earlier timestamps)
builds_ahead() {
    # List and sort files by name (timestamp order)
    ls -1 "$QUEUEDIR" | sort | awk -v myfile="${TIMESTAMP}_$$" '
    BEGIN { count = 0; found = 0 }
    {
        if (found) next;  # Skip after finding ours
        if ($0 == myfile) { found = 1; next }  # Ours: stop counting
        if ($0 ~ /^[0-9]+_[0-9]+$/) count++  # Valid queue file
    }
    END { print count }
    '
}

# Display initial queue position
AHEAD=$(builds_ahead)
echo "Build queued. Currently $AHEAD build(s) ahead of this one."

# Acquire exclusive lock (-x) on file descriptor 200, waiting until available
echo "Acquiring lock. Please wait until other builds are finished..."
(
    # While waiting for the lock, periodically update the queue position
    while ! flock -n 200; do
        AHEAD=$(builds_ahead)
        if [ "$AHEAD" -eq 1 ]; then
            S=""; 
        else 
            S="s"; 
        fi
        printf "\rStill waiting... %d build%s ahead." "$AHEAD" "$S"
        sleep 3
    done || { echo "Error: Failed to acquire lock."; rm "$QUEUEFILE"; exit 1; }

    # Once lock is acquired, we're building
    echo "\nLock acquired. Starting build (your position: 0 ahead)."

    BRANCH="$1"

    if [ -n "$BRANCH" ]; then
        echo "Switching to branch: $BRANCH"
        git fetch
        git checkout "$BRANCH" || { echo "Error: Failed to checkout branch $BRANCH"; rm "$QUEUEFILE"; exit 1; }
    else
        echo "No branch specified; using current branch: $(git branch --show-current)"
    fi

    git fetch && git pull && ./patchmover.sh && ./buildImage.sh

) 200>"$LOCKFILE"

# Cleanup queue file regardless of success (but after build attempt)
rm "$QUEUEFILE"

if [ $? -ne 0 ]; then
  echo "Build failed."
  exit 1
fi