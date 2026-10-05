#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>




int main(int argc, char *argv[])
{
   FILE *file;
   int failed = 0;


   if (argc != 3) {
       fprintf(stderr, "Usage: %s <username> \"<log message>\"\n", argv[0]);
       return EXIT_FAILURE;
   }
int fd = open("userlog.txt", O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR); 
if (fd == -1) { 
    perror("open"); 
    return EXIT_FAILURE; 
} 
file = fdopen(fd, "a");
   


   if (fprintf(file, "%s: %s\n", argv[1], argv[2]) < 0) {
       fprintf(stderr, "Failed to write to userlog.txt\n");
       failed = 1;
   }


   if (fclose(file) == EOF) {
       perror("Failed to close userlog.txt");
       failed = 1;
   }


   return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
