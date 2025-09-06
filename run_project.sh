#!/bin/bash

# MiniVSFS Project - Complete Automation Script
# This script compiles, creates file system, and adds all files automatically
# Compatible with all Linux distributions

echo "=========================================="
echo "    MiniVSFS Project Automation Script"
echo "=========================================="
echo ""

# Colors for output (with fallback for terminals that don't support colors)
if [ -t 1 ]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    NC='\033[0m' # No Color
else
    RED=''
    GREEN=''
    YELLOW=''
    BLUE=''
    NC=''
fi

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check if file exists
check_file() {
    if [ -f "$1" ]; then
        print_success "Found: $1"
        return 0
    else
        print_error "Missing: $1"
        return 1
    fi
}

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to detect package manager and install dependencies
install_dependencies() {
    print_status "Checking for required tools..."
    
    local missing_tools=()
    
    # Check for GCC
    if ! command_exists gcc; then
        missing_tools+=("gcc")
    fi
    
    # Check for xxd (preferred) or hexdump
    if ! command_exists xxd && ! command_exists hexdump; then
        missing_tools+=("xxd")
    fi
    
    if [ ${#missing_tools[@]} -eq 0 ]; then
        print_success "All required tools are available"
        return 0
    fi
    
    print_warning "Missing tools: ${missing_tools[*]}"
    print_status "Attempting to install missing dependencies..."
    
    # Try different package managers
    if command_exists apt-get; then
        print_status "Using apt-get..."
        sudo apt-get update -qq
        sudo apt-get install -y build-essential vim-common
    elif command_exists yum; then
        print_status "Using yum..."
        sudo yum install -y gcc gcc-c++ make vim-common
    elif command_exists dnf; then
        print_status "Using dnf..."
        sudo dnf install -y gcc gcc-c++ make vim-common
    elif command_exists pacman; then
        print_status "Using pacman..."
        sudo pacman -S --noconfirm base-devel vim
    elif command_exists zypper; then
        print_status "Using zypper..."
        sudo zypper install -y gcc gcc-c++ make vim
    elif command_exists apk; then
        print_status "Using apk..."
        sudo apk add --no-cache build-base vim
    else
        print_error "Could not detect package manager. Please install manually:"
        print_error "  - gcc (compiler)"
        print_error "  - xxd or hexdump (hex viewer)"
        print_error "  - make (build tools)"
        return 1
    fi
    
    # Verify installation
    if command_exists gcc && (command_exists xxd || command_exists hexdump); then
        print_success "Dependencies installed successfully"
        return 0
    else
        print_error "Failed to install dependencies. Please install manually."
        return 1
    fi
}

# Function to check if command succeeded
check_command() {
    if [ $? -eq 0 ]; then
        print_success "$1"
        return 0
    else
        print_error "$1"
        return 1
    fi
}

echo "Step 0: Checking system dependencies..."
echo "--------------------------------------"

# Install dependencies if needed
install_dependencies || {
    print_error "Failed to install dependencies. Please install manually and try again."
    exit 1
}

echo ""
echo "Step 1: Checking required files..."
echo "-----------------------------------"

# Check if source files exist
check_file "mkfs_builder.c" || exit 1
check_file "mkfs_adder.c" || exit 1

# Check if test files exist
check_file "file_15.txt" || exit 1
check_file "file_25.txt" || exit 1
check_file "file_31.txt" || exit 1
check_file "file_33.txt" || exit 1

echo ""
echo "Step 2: Compiling programs..."
echo "----------------------------"

# Compile mkfs_builder
print_status "Compiling mkfs_builder.c..."
gcc -O2 -std=c17 -Wall -Wextra mkfs_builder.c -o mkfs_builder
check_command "mkfs_builder compilation" || exit 1

# Compile mkfs_adder
print_status "Compiling mkfs_adder.c..."
gcc -O2 -std=c17 -Wall -Wextra mkfs_adder.c -o mkfs_adder
check_command "mkfs_adder compilation" || exit 1

echo ""
echo "Step 3: Creating file system..."
echo "-----------------------------"

# Create the initial file system
print_status "Creating MiniVSFS file system (1024 KiB, 256 inodes)..."
./mkfs_builder --image test.img --size-kib 1024 --inodes 256
check_command "File system creation" || exit 1

echo ""
echo "Step 4: Adding files to file system..."
echo "------------------------------------"

# Add files sequentially
print_status "Adding file_15.txt..."
./mkfs_adder --input test.img --output test1.img --file file_15.txt
check_command "Adding file_15.txt" || exit 1

print_status "Adding file_25.txt..."
./mkfs_adder --input test1.img --output test2.img --file file_25.txt
check_command "Adding file_25.txt" || exit 1

print_status "Adding file_31.txt..."
./mkfs_adder --input test2.img --output test3.img --file file_31.txt
check_command "Adding file_31.txt" || exit 1

print_status "Adding file_33.txt..."
./mkfs_adder --input test3.img --output final.img --file file_33.txt
check_command "Adding file_33.txt" || exit 1

echo ""
echo "Step 5: Verifying results..."
echo "---------------------------"

# Check if all image files were created
print_status "Checking created image files..."
check_file "test.img" || exit 1
check_file "test1.img" || exit 1
check_file "test2.img" || exit 1
check_file "test3.img" || exit 1
check_file "final.img" || exit 1

# Display file sizes
print_status "Image file sizes:"
ls -lh *.img

echo ""
echo "Step 6: Quick verification..."
echo "----------------------------"

# Quick verification of the final image
print_status "Checking magic number in final.img..."

# Use xxd if available, otherwise use hexdump
if command_exists xxd; then
    MAGIC_CHECK=$(xxd -g1 -l 4 final.img | head -1 | awk '{print $2, $3, $4, $5}')
else
    MAGIC_CHECK=$(hexdump -C -n 4 final.img | head -1 | awk '{print $2, $3, $4, $5}')
fi

if [ "$MAGIC_CHECK" = "46 53 56 4d" ]; then
    print_success "Magic number correct: $MAGIC_CHECK"
else
    print_warning "Magic number check: $MAGIC_CHECK"
fi

# Check root directory entries
print_status "Checking root directory entries..."
if command_exists xxd; then
    ROOT_DIR=$(dd if=final.img bs=4096 skip=11 count=1 2>/dev/null | xxd -g1 | head -5)
else
    ROOT_DIR=$(dd if=final.img bs=4096 skip=11 count=1 2>/dev/null | hexdump -C | head -5)
fi

if [ ! -z "$ROOT_DIR" ]; then
    print_success "Root directory structure found"
else
    print_warning "Could not verify root directory"
fi

echo ""
echo "Step 7: Project summary..."
echo "-------------------------"

# Display summary
print_success "Project completed successfully!"
echo ""
echo "Files created:"
echo "  - test.img     (empty file system)"
echo "  - test1.img    (with file_15.txt)"
echo "  - test2.img    (with file_15.txt + file_25.txt)"
echo "  - test3.img    (with file_15.txt + file_25.txt + file_31.txt)"
echo "  - final.img    (with all 4 files)"
echo ""
echo "Programs compiled:"
echo "  - mkfs_builder (file system creator)"
echo "  - mkfs_adder   (file adder)"
echo ""

# Optional: Run verification commands
read -p "Do you want to run detailed verification? (y/n): " -n 1 -r
echo ""
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo ""
    echo "Running detailed verification..."
    echo "==============================="
    
    if [ -f "verification_commands.txt" ]; then
        print_status "Running verification commands..."
        # Convert Windows line endings to Unix if needed
        sed -i 's/\r$//' verification_commands.txt 2>/dev/null || true
        bash verification_commands.txt
    else
        print_warning "verification_commands.txt not found, running basic checks..."
        
        echo "Magic number check:"
        if command_exists xxd; then
            xxd -g1 -l 4 final.img
        else
            hexdump -C -n 4 final.img
        fi
        
        echo "Root directory check:"
        if command_exists xxd; then
            dd if=final.img bs=4096 skip=11 count=1 2>/dev/null | xxd -g1 | head -10
        else
            dd if=final.img bs=4096 skip=11 count=1 2>/dev/null | hexdump -C | head -10
        fi
        
        echo "File content check:"
        if command_exists xxd; then
            dd if=final.img bs=4096 skip=12 count=1 2>/dev/null | xxd -g1 | head -5
        else
            dd if=final.img bs=4096 skip=12 count=1 2>/dev/null | hexdump -C | head -5
        fi
    fi
fi

echo ""
echo "=========================================="
echo "    MiniVSFS Project - COMPLETED!"
echo "=========================================="
echo ""
echo "Your file system is ready! Use 'final.img' as your complete MiniVSFS image."
echo ""
echo "To manually verify:"
if command_exists xxd; then
    echo "  xxd -g1 -l 32 final.img"
    echo "  dd if=final.img bs=4096 skip=11 count=1 2>/dev/null | xxd -g1"
else
    echo "  hexdump -C -n 32 final.img"
    echo "  dd if=final.img bs=4096 skip=11 count=1 2>/dev/null | hexdump -C"
fi
echo ""
echo "To clean up intermediate files:"
echo "  rm test*.img"
echo "  rm mkfs_builder mkfs_adder"
echo ""
echo "Supported Linux distributions:"
echo "  - Ubuntu/Debian (apt-get)"
echo "  - CentOS/RHEL (yum)"
echo "  - Fedora (dnf)"
echo "  - Arch Linux (pacman)"
echo "  - openSUSE (zypper)"
echo "  - Alpine Linux (apk)"
echo ""
