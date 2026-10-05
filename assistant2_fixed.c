#define _GNU_SOURCE


#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>


#define LOG_FILE "userlog.txt"
#define MAX_USERNAME_LENGTH 64
#define MAX_MESSAGE_LENGTH 1024
#define MAX_ENTRY_LENGTH (MAX_USERNAME_LENGTH + MAX_MESSAGE_LENGTH + 3)


static int
bounded_length(const char *input, size_t maximum, size_t *length)
{
   size_t i;


   if (input == NULL || length == NULL) {
       return -1;
   }


   for (i = 0; i <= maximum; ++i) {
       if (input[i] == '\0') {
           *length = i;
           return 0;
       }
   }


   return -1;
}


static int
valid_username(const char *username)
{
   size_t length;
   size_t i;


   if (bounded_length(username, MAX_USERNAME_LENGTH, &length) != 0 ||
       length == 0) {
       return 0;
   }


   for (i = 0; i < length; ++i) {
       unsigned char character = (unsigned char)username[i];


       if (!(isalnum(character) ||
             character == '_' ||
             character == '-' ||
             character == '.')) {
           return 0;
       }
   }


   return 1;
}


static int
valid_message(const char *message)
{
   size_t length;
   size_t i;


   if (bounded_length(message, MAX_MESSAGE_LENGTH, &length) != 0 ||
       length == 0) {
       return 0;
   }


   /*
    * Reject control characters, including newline and carriage return,
    * to prevent log injection and malformed log entries.
    */
   for (i = 0; i < length; ++i) {
       unsigned char character = (unsigned char)message[i];


       if (character < 0x20 || character == 0x7f) {
           return 0;
       }
   }


   return 1;
}


static int
write_all(int file_descriptor, const char *buffer, size_t length)
{
   size_t offset = 0;


   while (offset < length) {
       ssize_t result = write(file_descriptor,
                              buffer + offset,
                              length - offset);


       if (result < 0) {
           if (errno == EINTR) {
               continue;
           }
           return -1;
       }


       if (result == 0) {
           errno = EIO;
           return -1;
       }


       offset += (size_t)result;
   }


   return 0;
}


int
main(int argc, char *argv[])
{
   int file_descriptor = -1;
   int exit_status = EXIT_FAILURE;
   int lock_acquired = 0;
   int close_result;
   int formatted_length;
   size_t username_length;
   size_t message_length;
   char entry[MAX_ENTRY_LENGTH];


   if (argc != 3) {
       fprintf(stderr, "Usage: %s <username> <log-message>\n", argv[0]);
       return EXIT_FAILURE;
   }


   if (!valid_username(argv[1])) {
       fprintf(stderr, "Invalid username.\n");
       return EXIT_FAILURE;
   }


   if (!valid_message(argv[2])) {
       fprintf(stderr, "Invalid log message.\n");
       return EXIT_FAILURE;
   }


   /*
    * Recheck lengths before formatting so the size calculation is explicit
    * and the output buffer remains bounded.
    */
   if (bounded_length(argv[1], MAX_USERNAME_LENGTH, &username_length) != 0 ||
       bounded_length(argv[2], MAX_MESSAGE_LENGTH, &message_length) != 0) {
       fprintf(stderr, "Input is too long.\n");
       return EXIT_FAILURE;
   }


   if (username_length + message_length + 3 > sizeof(entry)) {
       fprintf(stderr, "Log entry is too long.\n");
       return EXIT_FAILURE;
   }


   formatted_length = snprintf(entry,
                               sizeof(entry),
                               "%s: %s\n",
                               argv[1],
                               argv[2]);


   if (formatted_length < 0 ||
       (size_t)formatted_length >= sizeof(entry)) {
       fprintf(stderr, "Failed to format log entry.\n");
       return EXIT_FAILURE;
   }


   /*
    * O_NOFOLLOW prevents opening a symbolic link in place of the log file.
    * O_APPEND makes each write append to the file.
    * O_CLOEXEC prevents descriptor inheritance across exec.
    */
   file_descriptor = open(LOG_FILE,
                          O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC |
                          O_NOFOLLOW,
                          S_IRUSR | S_IWUSR);


   if (file_descriptor < 0) {
       perror("open");
       return EXIT_FAILURE;
   }


   {
       struct stat file_status;


       if (fstat(file_descriptor, &file_status) != 0) {
           perror("fstat");
           goto cleanup;
       }


       if (!S_ISREG(file_status.st_mode)) {
           fprintf(stderr, "Log path is not a regular file.\n");
           goto cleanup;
       }
       if ((file_status.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
        fprintf(stderr, "Log file has insecure permissions.\n");
        goto cleanup;
   


       }
   }


   /*
    * Serialize writers so a partially written entry cannot be interleaved
    * with another process writing to the same log.
    */
   if (flock(file_descriptor, LOCK_EX) != 0) {
       perror("flock");
       goto cleanup;
   }
   lock_acquired = 1;


   if (write_all(file_descriptor, entry, (size_t)formatted_length) != 0) {
       perror("write");
       goto cleanup;
   }


   if (fsync(file_descriptor) != 0) {
       perror("fsync");
       goto cleanup;
   }


   exit_status = EXIT_SUCCESS;


cleanup:
   if (lock_acquired && flock(file_descriptor, LOCK_UN) != 0) {
       perror("flock unlock");
       exit_status = EXIT_FAILURE;
   }


   close_result = close(file_descriptor);
   if (close_result != 0) {
       perror("close");
       exit_status = EXIT_FAILURE;
   }


   return exit_status;
}
