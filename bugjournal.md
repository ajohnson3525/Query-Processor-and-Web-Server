# Bug 1

## A) How is your program acting differently than you expect it to?
- HttpUtils is failing the test on line 99 for IsPathSafe.

## B) Brainstorm a few possible causes of the bug
- We could be incorrectly using realpath().
- We might need a separate case for if there is not a slash (in the test file
path) after the found root directory.
- We may be incorrectly checking for the substring.

## C) How you fixed the bug and why the fix was necessary
- I fixed the bug by adding a slash to the root directory before searching for
it. This way, the IsPathSafe function will not incorrectly return true if it is
given a file path including a diretory that starts the same way as the one we
are looking for but ends differently.

# Bug 2

## A) How is your program acting differently than you expect it to?
- ServerSocket is returning the incorrect server address via the return
parameter.

## B) Brainstorm a few possible causes of the bug
- This could be caused by using the incorrect function to get the address.
- We could be incorrectly splitting up the socket families.

## C) How you fixed the bug and why the fix was necessary
- It turns out we were trying to get the server address using the
listen_sock_fd_ instead of the client_fd so we switched those out. This was
necessary because the listening socket can just listen for and recieve
connections, it can't give you the server address.

# Bug 3

## A) How is your program acting differently than you expect it to?
- We are failing the HttpConnection.cc tests; specifically, <>.

## B) Brainstorm a few possible causes of the bug
- This could have something to do with how we are reading the requests in
GetNewRequest. There is some tricky string/char* manipulation going on, since
we're reading into a string, but the WrappedRead function takes a char* parameter.
- There could be an issue with the implementation of the ParseRequest function.
Perhaps we are splitting up the request at the wrong spots, or accessing the
information incorrectly. We should double check the format of requests that
was provided to us.
- Another possible problem in ParseRequest is our usage of Boost algorithms.
Since we're fairly new to these algorithms, we could be using incorrect syntax.

## C) How you fixed the bug and why the fix was necessary
- The issue turned out to be in GetNewRequest. We were adding in unneccesary
information into the buffer because we used WrappedRead to read into a large
char array, which contains arbitrary data when it is created. Therefore it
was also adding some of this unnecessary data to our buffer when we appended
the char array to the buffer string. We fixed this keeping track of the
return value of WrappedRead, which tells us how much was actually written,
and changing the character at that index of the char array to the null
terminator. This way, we're only adding the actual data that was read
into our buffer.

