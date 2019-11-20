# Evil tests for lab 4

## Preconditions

1. You have Python 3 installed (which is already satisfied in the given docker image)
2. All the .py and .sh files have "x" permissions

## Usage

Inside test folder, type:

```bash
./evil_grade.sh
```

## Illustrations of testcases

### Evil Part 1

In this part, one simple thing is tested (although I believe it will crash many students' code).

First,  simply open a new file, move file cursor to pos 1, write "1" and close the file.

Then, I read it again to make sure that the file contains two bytes, first is "\0" and second is "1".

Many programs will crash in the create step, as they may misunderstand what `bytes_written` means in `yfs_client.cc`, which was not tested by TA.

### Evil Part 2

In this part, I tested the concurrency issues.

First, create a file filled with 1024 "?".

Second, create a process pool, which contains the process number up to `8 * cpu_count()`.

Third, create 1024 processes, each has a unique ID, just a number from 0 to 1023. For each process, it will write one byte to the file at offset specified by ID, that is, if ID is 233, the process will write at offset = 233. The content equals `chr((ID % 26) + ord('A'))`. So the file will finally become "ABC...XYZABC...".

Then I examine the file content to ensure the content meets my expectation.

### Evil Part 3

Now this is actually a **REAL EVIL PART**.

As defined in `inode_manager.h`, yfs can have up to 1024 inodes, but this number can be changed easily.

In this case, I just created 1050 files in sequence, and if `INODE_NUM` is less than 1050, it will definitely fail to pass this test.