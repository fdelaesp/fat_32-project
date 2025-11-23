#!/bin/bash
# FAT32 File System Test Script
# This script tests all commands in the FAT32 utility

echo "================================"
echo "FAT32 File System Test Script"
echo "================================"
echo ""

# Check if test image exists
if [ ! -f "test.img" ]; then
    echo "Creating test FAT32 image..."
    dd if=/dev/zero of=test.img bs=1M count=10 2>/dev/null
    mkfs.vfat -F 32 test.img > /dev/null 2>&1
    echo "✓ Test image created"
    echo ""
fi

# Check if filesys executable exists
if [ ! -f "bin/filesys" ]; then
    echo "Error: bin/filesys not found. Run 'make' first."
    exit 1
fi

echo "Running tests..."
echo ""

# Create test commands file
cat > test_commands.txt << 'EOF'
info
ls
mkdir testdir
ls
mkdir docs
mkdir projects
ls
cd testdir
ls
creat file1.txt
creat file2.txt
ls
open file1.txt -w
write file1.txt "Hello, this is a test file!"
close file1.txt
open file1.txt -r
lsof
read file1.txt 27
close file1.txt
open file2.txt -rw
write file2.txt "Another test"
lseek file2.txt 0
read file2.txt 12
close file2.txt
cd ..
ls
cd docs
ls
creat doc1
creat doc2
ls
cd ..
mv testdir/file1.txt docs
cd docs
ls
cd ..
cd testdir
ls
rm file2.txt
ls
cd ..
cd docs
rm doc1
rm doc2
rm file1.txt
cd ..
rmdir docs
rmdir testdir
ls
rmdir projects
ls
exit
EOF

echo "Test Plan:"
echo "=========="
echo "1. Display file system info"
echo "2. Create directories (testdir, docs, projects)"
echo "3. Create files in testdir"
echo "4. Write to files"
echo "5. Read from files"
echo "6. Use lseek to change position"
echo "7. Move file between directories"
echo "8. Delete files"
echo "9. Delete directories"
echo ""

echo "Executing test commands..."
echo "=========================="
echo ""

# Run the tests
./bin/filesys test.img < test_commands.txt

EXIT_CODE=$?

echo ""
echo "================================"
if [ $EXIT_CODE -eq 0 ]; then
    echo "✓ All tests completed successfully!"
else
    echo "✗ Tests failed with exit code: $EXIT_CODE"
fi
echo "================================"

# Cleanup
rm -f test_commands.txt

echo ""
echo "To verify the image integrity, you can:"
echo "  1. Mount it: sudo mount -o loop test.img mnt/"
echo "  2. Check with hexdump: hexdump -C test.img | less"
echo "  3. Run manual tests: ./bin/filesys test.img"
echo ""
