# poolf
URL dumper for e621 pools

Using libcurl and jansson, this trivial C program fetches the paginated pool
json data and dumps the direct file URL for each post page of the pool to
standard output with one URL for each line.

Only a single command line argument (the pool URL or the pool id) is taken.

If a fatal error occurs (i.e. a connection timeout or the received json data
was faulty in some way), the program exits with `EXIT_FAILURE` and prints error
messages to standard error. Otherwise, if only a singular file URL couldn't be
obtained, an empty line is printed in its place.

The order in which the pages occur in the pool is preserved, meaning a quick
shell script can easily enumerate and rename the resulting files (make sure to
check for empty one lines in case of errors).

# installation

```
$ git clone https://github.com/Muffindrake/poolf
$ cd poolf
$ su -c "make install"
```

# dependencies
- compiler that supports C11
- GNU make

# library dependencies (runtime and development headers
- libcurl
- jansson

# license
see the LICENSE file
