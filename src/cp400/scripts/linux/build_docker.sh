SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" &>/dev/null && pwd)
docker build -t sh4-cross "$SCRIPT_DIR/.."