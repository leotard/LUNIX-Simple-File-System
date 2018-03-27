To build you code run the following commands

$ make clean
$ make

This will give you an executable named "sfs" in your current directory 
which you can use to mount your filesystem

$ ./sfs /tmp/mymountpoint -d

You can test your code by running the test case file provided. You can do that 
with the following commands

$ make test1

This will create a file named "sfs_test1" in your current directory. Run the test by running the file

$ ./sfs_test1

similarly for running test2 execute

$ make test2
$ ./sfs_test2
