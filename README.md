# MyTar
A simple, fully functional GNU tar clone

## Usage
First, compile:
```bash
make clean && make
```

All the basic tar operations are supported:
```bash
./mytar cf archive.tar file1.txt dir1/     # Create archive.tar with file1.txt and file2.txt
./mytar tf archive.tar                     # List contents of archive.tar
./mytar xf archive.tar                     # Extract contents of archive.tar
```

Additional flags:
- `v`: verbose program output
- `S`: strict in interpretation of the Ustar POSIX standard

