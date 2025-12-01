#include<stdio.h>
#include<unistd.h>
#include<pwd.h>
#include<sys/types.h>
#include "prompt.h"
#include <string.h>
 
void display_prompt(char* home_dir) {
    char hostname[256];
    // gethostname returns 0 on success, -1 on error.
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        // If it fails, we can use a default or print an error.
        // perror is great because it prints our message, a colon,
        // and then the system error message.
        perror("gethostname");
        return; 
    }

    // getpwuid is more reliable than getlogin
    uid_t uid = getuid();
    struct passwd *pw = getpwuid(uid);
    if (pw == NULL) {
        perror("getpwuid");
        return;
    }
 
    char current_dir[1024];
    if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
        perror("getcwd");
        return;
    }
    
    // Replace the home directory with a tilde if the CWD is inside it.
    if (strncmp(current_dir, home_dir, strlen(home_dir)) == 0) {
        char temp_path[1024];
        snprintf(temp_path, sizeof(temp_path), "~%s", current_dir + strlen(home_dir));
        strncpy(current_dir, temp_path, sizeof(current_dir));
    }

    // pw->pw_name contains the username string
    printf("<%s@%s:%s> ", pw->pw_name, hostname, current_dir);
    fflush(stdout); // Ensure the prompt is displayed immediately.
}