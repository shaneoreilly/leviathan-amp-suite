#!/bin/sh
# Mic access needs a secure origin; localhost counts.
cd "$(dirname "$0")"
PORT=${PORT:-8765}
(sleep 1; open "http://localhost:$PORT") &
exec python3 -m http.server "$PORT" --bind 127.0.0.1
