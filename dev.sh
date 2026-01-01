#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/saffron/build"
NRO_FILE="${BUILD_DIR}/saffron.nro"

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

print_status() { echo -e "${GREEN}[*]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[!]${NC} $1"; }
print_error() { echo -e "${RED}[x]${NC} $1"; }

SWITCH_IP="${1:-$SWITCH_IP}"

show_usage() {
    echo "Usage: $0 [SWITCH_IP] [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --rebuild      Force full rebuild"
    echo "  --build-only   Build without deploying"
    echo "  --deploy-only  Deploy without rebuilding"
    echo "  --shell        Open shell in build container"
    echo "  --clean        Clean build directory"
    echo ""
    echo "Environment:"
    echo "  SWITCH_IP     IP address of Nintendo Switch"
    echo ""
    echo "On your Switch:"
    echo "  1. Open Homebrew Menu"
    echo "  2. Press Y for NetLoader mode"
    echo "  3. Note the IP address shown"
}

BUILD_ONLY=false
FORCE_REBUILD=false
DEPLOY_ONLY=false
OPEN_SHELL=false
CLEAN_BUILD=false

for arg in "$@"; do
    case $arg in
        --rebuild)
            FORCE_REBUILD=true
            shift
            ;;
        --build-only)
            BUILD_ONLY=true
            shift
            ;;
        --deploy-only)
            DEPLOY_ONLY=true
            shift
            ;;
        --shell)
            OPEN_SHELL=true
            shift
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --help|-h)
            show_usage
            exit 0
            ;;
    esac
done

if [ "$CLEAN_BUILD" = true ]; then
    print_status "Cleaning build directory..."
    rm -rf "${BUILD_DIR}"
    print_status "Build directory cleaned"
fi

DOCKER_IMAGE="saffron-builder"

if [ ! -f "${SCRIPT_DIR}/saffron/library/curl/README" ]; then
    print_status "Initializing submodules..."
    git -C "${SCRIPT_DIR}" submodule update --init --recursive
fi

IMAGE_EXISTS=$(docker image inspect "$DOCKER_IMAGE" &>/dev/null && echo "yes" || echo "no")

if [ "$FORCE_REBUILD" = "true" ] || [ "$IMAGE_EXISTS" = "no" ]; then
    print_status "Building Docker image..."
    docker build -t "$DOCKER_IMAGE" "${SCRIPT_DIR}"
    if [ $? -ne 0 ]; then
        print_error "Docker build failed"
        exit 1
    fi
fi

if [ "$OPEN_SHELL" = true ]; then
    print_status "Opening shell in build container..."
    docker run --rm -it \
        -v "${SCRIPT_DIR}:/workspace" \
        -w /workspace/saffron \
        "$DOCKER_IMAGE" \
        bash
    exit 0
fi

if [ "$DEPLOY_ONLY" = false ]; then
    print_status "Building..."

    docker run --rm \
        -v "${SCRIPT_DIR}:/workspace" \
        -w /workspace/saffron \
        "$DOCKER_IMAGE" \
        bash -c "
            set -e
            git config --global --add safe.directory /workspace
            git config --global --add safe.directory /workspace/saffron
            git config --global --add safe.directory /workspace/saffron/library/borealis
            mkdir -p build && cd build
            cmake .. -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake
            make -j\$(nproc)
            make saffron.nro
        "

    if [ ! -f "$NRO_FILE" ]; then
        print_error "Build failed - NRO not found at $NRO_FILE"
        exit 1
    fi

    print_status "Build successful: $NRO_FILE"
fi

if [ "$BUILD_ONLY" = true ]; then
    print_status "Build-only mode - skipping deployment"
    exit 0
fi

if [ "$DEPLOY_ONLY" = true ]; then
    if [ ! -f "$NRO_FILE" ]; then
        print_error "NRO not found at $NRO_FILE - build first"
        exit 1
    fi
    print_status "Deploy-only mode - using existing build"
fi

if [ -z "$SWITCH_IP" ]; then
    print_warning "No SWITCH_IP provided - skipping deployment"
    echo ""
    echo "To deploy, run: $0 <SWITCH_IP>"
    echo "   or: SWITCH_IP=192.168.x.x $0"
    exit 0
fi

pkill -f nxlink 2>/dev/null || true
sudo fuser -k 28280/tcp 2>/dev/null || true
sleep 0.5

LOGS_DIR="${SCRIPT_DIR}/logs"
mkdir -p "$LOGS_DIR"
LOG_FILE="${LOGS_DIR}/$(date +%d%m%y%H%M%S).log"

print_status "Deploying to Switch at ${SWITCH_IP}..."
print_status "Logging to: ${LOG_FILE}"
print_status "Press Ctrl+C to stop receiving logs"

docker run --rm -it --init \
    --network host \
    -v "${SCRIPT_DIR}:/workspace" \
    -w /workspace/saffron \
    "$DOCKER_IMAGE" \
    nxlink -s -a "$SWITCH_IP" /workspace/saffron/build/saffron.nro 2>&1 | tee "$LOG_FILE"

print_status "Done"
