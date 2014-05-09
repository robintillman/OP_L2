/*
 *   Namn: Small-Shell v2.51 för UNIX
 *
 *   Beskrivning:
 *       Detta program fungerar som en liten kommandotolk (shell) och kommer
 *       exekvera givna kommandon tills dess att programmet avslutas med
 *       kommandot <exit>.
 *   
 *   Av: Robin Tillman, robint@kth.se
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/time.h>

/*
 * Node är en länkad lista
 */
typedef struct Node {
	struct Node *next; /* Pekare på nästa nod i den länkade listan */
	int val;           /* värde sparat i noden */
} Node;

Node *bgChildProcesses = NULL; /* Lista att spara childprocesserna i*/

/* addFirst
 * 
 * Lägger till ett element först i en lista.
 *
 */
Node * 
addFirst(
	Node *list, /* Listan att stoppa element i */
	int val) 	/* Elementet att stoppa i listan */
{
	Node *node = malloc(sizeof(Node));
	
	if (!node) {
	    fprintf(stderr, "Failed to allocate ram\n"); 
	    exit(1);
	}
	
	node -> val = val;
	node -> next = list;
	return node;
}

/* zombieFilter
 *
 * Går igenom en lista av Nodes och tar bort alla där Node.val är true.
 *
 * Returnerar den rensade listan.
 *
 */
Node * 
zombieFilter(
	Node *list, 		/* Listan att rensa */
	int(*filter)(int)) 	/* En funktion som kollar om nod är zombie */
{
	Node *current = list;
	Node *prev = list;
	
	/* Iterera genom alla noder i den länkade listan */
	while (current) {
		if (!filter(current->val)) {
			if (current == list) {
				list = current->next;
				prev = list;
			}else{
				prev->next = current->next;
			}
			Node *tmp = current;
			current = current->next;			
			free(tmp);
		}else{
			prev = current;
			current = current->next;			
		}
	}
	return list;
}

/* execProgram
 *
 * Exekverar programmet med givna parametrar.
 * Hantering av för- resp. bakgrundsprocesser.
 *
 * Returnerar 0 om OK, annars errorkod. 
 *
 */
int
execProgram(
    char* args[]) /* Lista över argumenten */
{
	char * program = args[0];
	
	/* Kolla sista argumentet för att avgöra om bak- eller förgrundsprocess */
	int lastArgIndex = -1;
	for (;args[lastArgIndex+1];lastArgIndex++);
	char *lastArg = args[lastArgIndex];
	int isBackground = lastArg[strlen(lastArg)-1] == '&';
	if(isBackground) {
 	    lastArg[strlen(lastArg)-1] = 0;
	}
	
	/* Skapa ny process */
	int childPid = fork();
	if (childPid == -1){
	    fprintf(stderr, "Fork error\n"); 
	    return 1;
	}
	
    if (!childPid) {
		if (execvp(program, args) == -1) exit(1);
	} else {
	    if (isBackground) {	
	        printf("Spawned background process pid: %d\n", childPid);
	        bgChildProcesses = addFirst(bgChildProcesses, childPid); 
        } else {
        	/* Tidsvariabler för förgrundsprocesser */
	        struct timeval startTime;
	        struct timeval endTime;
	        int timeError = 0;
	        
	        if (gettimeofday(&startTime, NULL) == -1) {
	            timeError = 1;
	        }
	        
            printf("Spawned foreground process pid: %d\n", childPid);
            
            int ret, childExitStatus;
			ret = wait(&childExitStatus); 	/* Vänta på barnprocessen */
			if (ret == -1) {
			    fprintf(stderr, "Failed to get return value from child\n");
			}
			
			if (gettimeofday(&endTime, NULL) == -1) {
			    timeError = 1;
			}
			
			if (timeError) {
				printf("Error getting time from %d, status: %d\n", childPid, childExitStatus);			
			} else {
				long ms = (endTime.tv_sec - startTime.tv_sec)*1000 + (endTime.tv_usec - startTime.tv_usec)/1000;
				
				printf("Foreground process %d terminated\n", childPid);
				printf("wallclock time: %ld msec\n", ms);			
			}
	    }
    }
	
	return 0;
}

/* terminationCheck
 * 
 * Kontrollerar om en process har blivit terminerad. 
 * 
 * Returnerar 1 om processen är terminerad annars 0.
 *
 */
int 
terminationCheck(
	int pid) /* Process id till den process som ska kontrolleras */
{
	int val;
	int ret;
	
    ret = waitpid(pid, &val, WNOHANG);
    
	int isTerminated = ret == 0 ? 1 : 0;
	if (!isTerminated) {
	    printf("Background process %d terminated\n", pid);
	}
	
	return isTerminated;
}

/* splitList
 * 
 * splitList delar en sträng på mellanslag, \n och \0. 
 * 
 * Returvärdet är en lista av strängar.
 *
 */
char ** 
splitList(
	char * string,	/* Strängen att dela */
	int count)		/* Räknevariabel till rekursionerna, initialiseras till 0 */
{
	char * tok = strtok(string, " \t\n\0");
	char ** result;
	if (tok != NULL) {
		result = splitList(NULL, count+1);
		result[count] = tok;
	} else {
		result = malloc(sizeof(char*)*(count+1));
		result[count] = NULL;
	}
	
	return result;
}

/* signalHandler
 *
 * Funktionen gör ingenting då uppgiftskravet säger
 * att Small-Shell inte ska avslutas vid CTRL-C. 
 * Returnerar ingenting.
 *
 */
void
signalHandler(
    int signum) /* Signalen som ska hanteras */
{
    /* GÖR INGENTING */
}
 
/* smallShell
 *
 * smallShell innehåller logiken för shellets funktionalitet som definierat
 * i laborationsbeskrivningen.
 *
 */
void
smallShell()
{
    char arg[70] = ""; /* Hanterare för argument */
    signal(SIGINT, signalHandler); /* Hantera CTRL-C */

    int isBackground;
    while(1) /* Evighetsloop då programmet endast ska avlutas vid "exit" */    
    {
        printf("==>> "); /* Skriv ut prompttecken */
        
        /* Rensa zombieprocesser */
		bgChildProcesses = zombieFilter(bgChildProcesses, &terminationCheck);
        
        fgets(arg, 70, stdin); /* Hämta argument från stdin till arg */
        char **args = splitList(arg, 0); /* Dela in indatat i lista */

        char *command = args[0]; /* Det första argumentet är kommandot */
        if(command != NULL)
        {
            /* Kolla om inbyggt kommando */
            if(!strcmp(command, "exit")) {
                free(args);
                return;
            } else if(!strcmp(command, "cd")) {
                if(chdir(args[1])) {
                    chdir(getenv("HOME"));
                }
            } else { /* Icke inbyggt kommando */
				execProgram(args);
            }
        }
        
        free(args);
    } 
}
 
/* main
 *
 * Returnerar 0 om OK, annars errorkod.
 *
 */
int
main(
	int argc,   /* Antal argument givna i argv */
	char *argv[])   /* Argument givna via kommandotolken */
{
    smallShell(); /* Starta shellet */

	return 0;
}
