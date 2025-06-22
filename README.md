# FAT32 Emulator
This is a FAT32 filesystem emulator. It creates a file containing a FAT32 image and allows basic operations to be performed on it.

# Build Instruction (Linux)
To build the project, run the following commands in `bash` or `sh`:

``` sh
git clone 'https://github.com/ksldmitriy/fat32-emulator.git'
cd fat32-emulator
./build.sh
```

The executable will be created in the `bin` directory inside the project root.

# Example Usage
```
./bin/fat32 filesystem.fat32
/>ls
.
/>mkdir /dir1
/>touch /file1
/>ls
. DIR1 FILE1
/>ls /dir1
. ..
/>cd /dir1
/dir1>touch /dir1/file2
/dir1>ls
. .. FILE2
/dir1>format
/>ls
.
/>
```
