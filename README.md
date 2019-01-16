# poolf
URL dumper for e621 pools

Using libcurl and jansson, this trivial C program fetches the paginated pool
json data and dumps the direct file URL for each post page of the pool to
standard output with one URL for each line.

Only a single command line argument (the pool URL or the pool id) is taken.
The provided `poolfdl` script uses the `poolf` program to obtain the file urls
and enumerates and fetches the files in order to the current directory.

If a fatal error occurs (i.e. a connection timeout or the received json data
was faulty in some way), the program exits with `EXIT_FAILURE` and prints error
messages to standard error. Otherwise, if only a singular file URL couldn't be
obtained, an empty line is printed in its place.

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
- libcurl 7.62.0 or later
- jansson

# license
see the LICENSE file
