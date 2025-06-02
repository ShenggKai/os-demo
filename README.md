# os-demo

## How to Run This Project on Ubuntu

### Prerequisites

- Ubuntu (or any Linux distribution with GCC and make)
- GCC compiler (`sudo apt update && sudo apt install build-essential`)

### Build and Run

1. **Open Terminal** and navigate to the project directory:

```sh
cd /path/to/os-demo
```

2. Compile the program:

```
gcc -o os-demo main.c
```

3. Run the program:

```
./os-demo
```

**Notes**:
The program uses System V shared memory. Make sure you have permissions to use shared memory on your system.
To clean up shared memory segments left after abnormal termination, you can use:

```
ipcs -m
# Find the shmid and remove it with:
ipcrm -m <shmid>
```
