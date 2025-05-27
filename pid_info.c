#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#include "pid_info.h"
#include <time.h>
#include <unistd.h>

//FILE MATTEO 
void PidList_print(ListHead* head){
  ListItem* aux=head->first;
  printf("[");
  while(aux){
    PidListItem* element = (PidListItem*) aux;
    printf("%d ", element->pid);
    aux=aux->next;
  }
  printf("]\n");
}
PidStatListItem* PidListStat_find(ListHead* head, pid_t pid){
  ListItem* aux=head->first;
  while(aux){
    PidStatListItem* element = (PidStatListItem*) aux;
    if( element->pid == pid ) return element;
    aux = aux->next;
  }
  return NULL;
}

int is_pid_dir(const struct dirent *entry) {
    const char *p;

    for (p = entry->d_name; *p; p++) {
        if (!isdigit(*p))
            return 0;
    }

    return 1;
}
int getRunningPids( ListHead* head ) {
    DIR *procdir;
    FILE *fp;

    

    struct dirent *entry;
    char path[256 + 5 + 5]; // d_name + /proc + /stat
    int pid;
    unsigned long maj_faults;

    // Open /proc directory.
    procdir = opendir("/proc");
    if (!procdir) {
        perror("opendir failed");
        return 1;
    }

    // Iterate through all files and directories of /proc.
    while ((entry = readdir(procdir))) {
        // Skip anything that is not a PID directory.
        if (!is_pid_dir(entry))
            continue;

        // Try to open /proc/<PID>/stat.
        snprintf(path, sizeof(path), "/proc/%s/stat", entry->d_name);
        fp = fopen(path, "r");
        if (fp == NULL) {
            perror("FOPEN ERROR 3");
            //fclose(fp);
            continue;
        }

        // Get PID, process name and number of faults.
        fscanf(fp, "%d %s %*c %*d %*d %*d %*d %*d %*u %*lu %*lu %lu",
            &pid, &path, &maj_faults
        );


        char * process_name = (char *)calloc(256, sizeof(char));
        strcpy(process_name, path);


        PidListItem* new_element= (PidListItem*) calloc(1, sizeof(PidListItem));
        if (! new_element) {
            printf("out of memory\n");
            break;
        }
        new_element->pid = pid;
        new_element->name = process_name;
        
        ListItem* result= List_insert(head, head -> last, (ListItem*) new_element);
        assert(result);

        // Pretty print.
        // printf("%5d %-20s: %lu\n", pid, path, maj_faults);
        fclose(fp);
    }

    

    closedir(procdir);
    return 0;
}

PidStatListItem* intializeProcessStats( ListHead *head, pid_t pid ){
    pstat stat;
    
    if( getPidStats( pid, &stat ) == -1 ){
        stat.status = -1;
    } else {
        stat.status = 1;
    }

    PidStat new_stat;
    new_stat.current = stat;


    PidStatListItem* new_element = (PidStatListItem*) calloc(1, sizeof(PidStatListItem));
    new_element->pid = pid;
    new_element->stat = new_stat;


    ListItem* result= List_insert( head, head->last, (ListItem*) new_element);
    assert(result);

    

    return new_element;
}



