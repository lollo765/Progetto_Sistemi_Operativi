#include <ncurses.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include "linked_list.h"
#include "pid_stats.h"
#include "pid_info.h"
#include <sys/ioctl.h>
#include <semaphore.h>

#define CMD_WIN_HEIGHT 10
#define CMD_WIN_WIDTH 170
#define PROC_WIN_HEAD_HEIGHT 4
#define PROC_WIN_HEAD_WIDTH 170
#define PROC_WIN_HEIGHT 20
#define PROC_WIN_WIDTH 170

#define SLEEP_INTERVAL 2

sem_t sem;


WINDOW* cmd;
WINDOW* proc_head;
WINDOW* proc;
pid_t CURRENT_PID = 1;

int scrollAmt = PROC_WIN_HEIGHT;
int processes_page = 0;
int processes_max_page = 0;


int scroll_top = 0;
int scroll_bot = PROC_WIN_HEIGHT - 1;


char cmd_prompted[256];
int  cmd_prompted_len = 0;

char error_prompted[256];
int error_prompted_len=0;

ListHead *current_processes_list_head;
ListHead *processes_stats;


// MATTEO
void PidListfree(ListHead* head) {
    sem_wait( &sem );
    ListItem* aux=head->first;
    while(aux){
        ListItem* tmp = aux->next;
        free(((PidListItem*)aux)->name);
        free(aux);
        aux = tmp;
    }
    sem_post( &sem );
}
//MATTEO
void render_processes(){

    

    ListItem* aux= current_processes_list_head -> first;
    int row=5;
    int render_row = 1;

    processes_max_page = (current_processes_list_head ->size)/scrollAmt;
    
    sem_wait( &sem );
    while(aux) {
        PidListItem* element = (PidListItem*) aux;
        PidStatListItem* s = PidListStat_find( processes_stats, element->pid);

        if(s == NULL) {
            s = intializeProcessStats( processes_stats, element->pid);
            continue;
        } else {
            pstat stat;
            if( getPidStats(element->pid, &stat) == -1 ){ // process was killed
                stat.status = -1;
            } else {
                stat.status = 1;
            }
            s->stat.prev = s->stat.current;
            s->stat.current = stat;
           
            
        }

        double usage = -1;
        if( &s->stat.current.status != -1 && &s->stat.prev.status != -1 ){
            calc_cpu_usage_pct(&s->stat.current, &s->stat.prev, &usage);
        }

        int first = processes_page * scrollAmt;
        int last = first + scrollAmt;
        
        
        
        if ( row >= first && row <= last ){
            //wmove(proc, current_row - scroll_top + 1, 0);
            if( s->stat.current.status != -1 ){
                mvwprintw( 
                    proc, 
                    render_row, 25,
                    "%3d | %5d            %.02f               %6.02f             %3i           %3i           %-35s       \n", 
                    row, element->pid, usage, s->stat.current.ramUsage, s->stat.current.priority, s->stat.current.nice, element->name
                );
            } else {
                mvwprintw( 
                    proc, 
                    render_row, 25,
                    "%3d | -----            --               ------             ---           ---           ----------------------       \n", 
                    row
                );
            }

            
            wrefresh(proc);
            render_row ++;
        }

        aux = aux->next;
        row++;
    }
    sem_post( &sem );    
        
    //wclear(proc);
}
//MARCO
void draw_proc_window(){
    ListHead headStats;
    processes_stats = &headStats; 
    List_init(&headStats); // inzializza lista stati (prec|corrente) dei vari processi

    int i=0;
    Memory mem;
    Swap swap;
    
    while(i==0){

        

        ListHead head;
        current_processes_list_head = &head;
        List_init(&head); // inzializza lista pid processi running
        getRunningPids(&head); // popola la lista con tutti i processi running

        getMemory(&mem); //popola la struct mem
        getSwap(&swap);//popola la struct swap

        init_pair(1, COLOR_GREEN, COLOR_BLACK); //COLORI PER MESSAGGIO STATICO
        wattron(proc_head,COLOR_PAIR(1));
        wclear(proc_head);
        mvwprintw(
            proc_head, 
            1, 5,
            "MemTotal: %d || MemFree: %d || MemAvailable: %d || Cached: %d || MemUsed: %d  || SwapTotal: %d || SwapFree: %d || SwapUsed: %d ", 
            mem.Total,mem.Free,mem.Avail,mem.Cache,mem.Used,swap.Total,swap.Free,swap.Used
        );
        mvwprintw(
            proc_head,
            3, 27, 
            "N |    PID     |      CPU        |         MEM         |    PRIO      |    NICE      |  NAME     "
        );
        wattroff(proc_head,COLOR_PAIR(1));
        wrefresh(proc_head);


        werase(proc);
        render_processes();
       
        sleep( SLEEP_INTERVAL );
        PidListfree( &head );
    }


}
//MARCO
draw_init_cmd_window(){
    wclear(cmd);
    box(cmd, 0, 0);
    wattron(cmd,COLOR_PAIR(1));
    mvwprintw(cmd,2,25, "Inserisci nel terminale la funzione da eseguire seguita dal process Id:");
    mvwprintw(cmd,3,25, "Premendo h potrai vedere le funzioni eseguibili");
    mvwprintw(cmd,4,25, "Premendo q potrai uscire dal programma");
    mvwprintw(cmd,6,25, cmd_prompted);
    mvwprintw(cmd,8,25,error_prompted);
    wattroff(cmd,COLOR_PAIR(1));
    wrefresh(cmd);
}
//MARCO
void h(){
    draw_init_cmd_window();
    mvwprintw( cmd, 6,25,"s: suspend  ||  r: resume || t: terminate || k: kill || q: quit");
    wrefresh(cmd);
}
//MARCO
static void check_error(WINDOW* cmd,long res, char* msg, int proc) {
    if (res != -1) {
        sprintf(error_prompted,"Il Processo PID( %d ) %s con successo",proc, msg);
        draw_init_cmd_window();
    }else{
        switch (errno){
                case EPERM: { 
                            sprintf(error_prompted,"Non hai il permesso di %s il processo PID( %d ) ", msg ,proc);
                            draw_init_cmd_window();
                            break;}
                case ESRCH: {
                            sprintf(error_prompted,"Il Processo PID( %d ) non esiste ",proc);
                            draw_init_cmd_window();
                            break;}
        }
    }
}
//MARCO
void draw_cmd_window() {
    init_pair(1,COLOR_GREEN, COLOR_BLACK);  //COLORI PER MESSAGGIO STATICO
    char fun;
    int  pid;
    
    draw_init_cmd_window();

    int ch;
    int scrollAmt = PROC_WIN_HEIGHT;
    while( (ch = getch()) != KEY_F(1)  ){
       
            if( ch == KEY_UP ){
                if (processes_page > 0) {
                    processes_page --;
                    render_processes();
                }
                
            } else if( ch == KEY_DOWN ){
                if( processes_page < processes_max_page ) {
                    processes_page ++;
                    render_processes();
                }
            } else if( ch == '\n' ){
                sscanf( cmd_prompted, "%c %d", &fun, &pid );

                cmd_prompted_len = 0;
                cmd_prompted[0] = '\0';
                error_prompted_len = 0;
                error_prompted[0] = '\0'; 
                switch (fun){
                    case 'h': h();
                        break;
                    case 't': check_error(cmd,kill(pid,SIGTERM),"terminate",pid);
                        break;
                    case 'k': check_error(cmd,kill(pid,SIGKILL),"kill",pid);
                        break;
                    case 'r': check_error(cmd,kill(pid,SIGCONT),"resume",pid);
                        break;
                    case 's': check_error(cmd,kill(pid,SIGSTOP),"suspend",pid);
                        break;
                    case 'q':
                        system("clear");
                        exit(0);
                        break;
                    default: 
                        draw_init_cmd_window();
                        break;
                }


                
                
            } else {
                
                if (ch == KEY_BACKSPACE || ch == 127) {
                    if (cmd_prompted_len > 0) {
                        cmd_prompted_len--;
                        cmd_prompted[cmd_prompted_len] = '\0';
                    }
                } else {
                    cmd_prompted[cmd_prompted_len] = ch;
                    cmd_prompted_len++;
                    cmd_prompted[cmd_prompted_len] = '\0';
                }
                draw_init_cmd_window();
                

            }
    }
    
}


//MATTEO MARCO
int main() {

    // intialize sem
    sem_init( &sem, 0, 1);

    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    mousemask(ALL_MOUSE_EVENTS, NULL);
    start_color();
    
    // create info window
    cmd = newwin(CMD_WIN_HEIGHT, CMD_WIN_WIDTH, 0 ,0);
    proc_head = newwin(PROC_WIN_HEAD_HEIGHT, PROC_WIN_HEAD_WIDTH, CMD_WIN_HEIGHT, 0);
    proc = newwin(PROC_WIN_HEIGHT, PROC_WIN_WIDTH, CMD_WIN_HEIGHT + PROC_WIN_HEAD_HEIGHT, 0);

    // Enable scrolling on proc window
    scrollok(proc, TRUE);
   

    box(proc_head,0,0);
    box(proc,0,0);
    box(cmd,0,0);
    
    wrefresh(proc_head);
    wrefresh(proc);
    wrefresh(cmd);
    
    // create threads for info window, process window, and command windowq
    pthread_t proc_thread, cmd_thread;
    
    pthread_create(&proc_thread, NULL,(void*) draw_proc_window, NULL);
    pthread_create(&cmd_thread, NULL,(void*) draw_cmd_window, NULL);

    sleep(1);
    draw_init_cmd_window();

    pthread_join(cmd_thread, NULL);
    endwin();
    sem_destroy( &sem );
    return 0;
}