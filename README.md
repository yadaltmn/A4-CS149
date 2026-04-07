/**
 * Description: This project implements Assignment 4 for CS 149. The shell program
 * (shell.c) runs countnames in parallel across multiple input files. Instead of
 * using pipes, the parent allocates a shared memory region with mmap(), divides
 * it into child-specific subspaces, waits for all children to finish, and then
 * aggregates the name counts from shared memory into a final summary region.
 * GitHub : https://github.com/yadaltmn/A4-CS149
 *
 * Author names:
 * Jada-Lien Nguyen
 * Jesse Mendoza
 * Author emails:
 * jada-lien.nguyen@sjsu.edu
 * jesse.mendoza@sjsu.edu
 * Last modified date: 4/7/2026
 * Creation date: 4/7/2026
 **/

## Link to Output PDF

[output.pdf](output.pdf)

## Files Submitted

- `shell.c`
- `countnames.c`
- `README.md`
- `output.pdf`
- `test/` directory with all test files

## How to Compile

Run these commands in the project directory:

```bash
gcc -o shell shell.c -Wall -Werror
gcc -o countnames countnames.c -Wall -Werror
```

## How to Run

Start the shell:

```bash
./shell
```

Inside the shell, run `countnames` with one to three input files:

```bash
./countnames test/names.txt
./countnames test/names.txt test/names1.txt
./countnames test/testCase1.txt test/testCase3.txt
```

Exit the shell with:

```bash
exit
```

## Implementation Summary

- `shell.c` creates a large shared memory region using `mmap()`.
- The shared memory is divided into one region per child plus one summary region for the parent.
- Each child processes one file and writes its `NameCountData` array into only its assigned subspace.
- Each child region ends when a record has `name[0] == '\0'`.
- After `wait()` completes for all children, the parent reads each child region, merges duplicate names into the summary region, and prints the final combined output.
- `countnames.c` still creates PID-based `.out` and `.err` files for direct evidence of each child process.

## Test Cases

### Test 1

Command:

```bash
./countnames test/names.txt
```

What it tests:
- Empty lines
- Duplicate names
- Names containing spaces

Expected direct output in the child `.out` file:

```text
Nicky: 1
Dave Joe: 2
Yuan Cheng Chang: 3
John Smith: 1
```

Expected warnings in the child `.err` file:

```text
Warning - file test/names.txt line 2 is empty.
Warning - file test/names.txt line 5 is empty.
```

### Test 2

Command:

```bash
./countnames test/names.txt test/names1.txt
```

What it tests:
- Parallel processing with two files
- Shared-memory child subspaces
- Parent aggregation of duplicate names

Expected final output:

```text
Tom Wu: 3
Nicky: 1
Dave Joe: 2
Yuan Cheng Chang: 3
John Smith: 1
```

### Test 3

Command:

```bash
./countnames test/testCase2.txt test/namesB.txt
```

What it tests:
- Empty lines
- Leading spaces
- A line containing only a space
- Case sensitivity
- Parent aggregation from two children

Expected final output:

```text
Nicky: 1
Dave Joe: 2
Yuan Cheng Chang: 3
John Smith: 1
Jose Mendez: 1
Ana Nguyen: 2
Charlie Kirk: 1
Barrack Obama: 1
Tommy Obama: 1
 : 1
 Tim Tran: 1
Andrew White: 2
ANDREW WHITE: 1
Nguyen Ana: 1
```

### Test 4

Command:

```bash
./countnames test/testCase1.txt test/testCase3.txt
```

What it tests:
- Custom student-created test files
- Internal spacing differences
- Leading spaces
- Case sensitivity
- Multiple distinct names that look similar

Expected final output:

```text
ABRAHAM LINCOLN: 1
ABRAHAM RAMIREZ: 1
 ABRAHAM LINCOLN: 1
ADRIAN ADRIAN: 1
ADRIAN: 1
JADA NGUYEn: 1
GEORGE WASHINGTON: 2
JADA NGUYEN: 1
JORGE PEREZ: 1
JESSE MENDOZA: 1
MENDOZA JESSE: 1
Mike Lam: 1
Jimmy Le: 1
J i m m y L e: 1
M ike Lam: 1
JeSsE: 1
JESSE: 1
```

## Test Files and Edge Cases

- `test/names.txt` tests empty lines, duplicates, and names with spaces.
- `test/names1.txt` tests one repeated name and a smaller second input file.
- `test/names2.txt` tests duplicate values across files.
- `test/namesB.txt` tests duplicates and empty lines.
- `test/names_long.txt` tests many unique names.
- `test/names_long_redundant.txt` tests many unique names with repeated values.
- `test/names_long_redundant1.txt` tests many unique names with a few empty lines.
- `test/names_long_redundant2.txt` tests many unique names.
- `test/names_long_redundant3.txt` tests many unique names with duplicates.
- `test/testCase1.txt` is a custom file that tests spacing inside names.
- `test/testCase2.txt` is a custom file that tests leading spaces, blank lines, a line containing only a space, and case sensitivity.
- `test/testCase3.txt` is a custom file that tests leading spaces, repeated first names, empty lines, and duplicate-looking names.

## Lessons Learned

Jada Lien Nguyen - This assignment helped me understand how a parent process can manage a shared memory region and safely divide it into subspaces for child processes. I learned how `mmap()` changes the way processes communicate compared to pipes, and how careful memory layout and termination markers make shared-memory aggregation easier to debug.

Jesse Mendoza - This assignment helped me learn how `mmap()` can be used for shared memory between processes, how a parent can organize global memory into per-child sections, and how to combine those results after `wait()` completes. It also reinforced how parallel processing changes the way process communication must be designed.

## References

- [Linux man page for mmap](https://man7.org/linux/man-pages/man2/mmap.2.html)
- [Linux man page for wait](https://man7.org/linux/man-pages/man2/wait.2.html)
- Course materials: lecture slides, ZyBooks notes, and assignment instructions from CS 149 SP26.
- [Stack Overflow](https://stackoverflow.com) for clarification on string handling, `fgets`, and basic `mmap()` usage patterns.
- AI assistant (ChatGPT) for help clarifying documentation wording and implementation planning.

## Acknowledgements

Thanks to the course instructor for the assignment guidance, the class Discord for discussions about edge cases and debugging, ZyBooks for the assignment environment, and AI tools plus online references for helping clarify system call behavior and documentation wording.
