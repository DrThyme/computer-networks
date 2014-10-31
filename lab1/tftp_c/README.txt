FILES
=====

This archive contains the following five files:

tftp.c
This is the only file that you should edit. Look through it and read
the comments. Note that /* ... */ marks places where you should insertcode.

tftp.h
This files contains all the predefined variables and data structures
that you need, you should use them throughout your code. Note that some
names match what you have read in the RFC. Do not add anything in this file
since you will not hand it in. But you need to edit the port number to change
between ports when testing.

Makefile
This file is used to compile your program using make, do not
edit it.

additional_info.pdf
More useful links are provided in the additional_info.pdf.

README.txt
This file.

Familiarize yourself with them, they are an essential help in completing the
assignment.

 
PROGRAMMING
===========
Now you are ready to start programming the client that should be able to GET a
file from a server as well as PUT a file onto a server.

You will need to extend the skeleton code in tftp.c and do the following:
 * Write the socket handling code to open the socket to the server (code for
   closing the socket is provided).
 * Open and close the file being dealt with.
 * Construct packets using the pre-defined packet header and send the following
    - Read request packets
    - Write request packets
    - Acknowledgment packets
    - Data packets
 * Handle receiving data and perform the appropriate action
    - Write data to file
    - Read data from file and send to server
    - Handle errors from server

COMPILING
=========

The skeleton code already contains a Makefile for building. Run the program make
to compile your code. Compiler warnings: The code is complied with the -Wall
flag, your code needs to compile without any warnings when you hand it in.

RUNNING
=======


To run the program and get the file small.txt from the server joshua.it.uu.se
simply type (this will of course not work until you have implemented what is
necessary)

 ./tftp -g small.txt joshua.it.uu.se

