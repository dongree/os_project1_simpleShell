#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>


#define MAX_LINE 80 /* The maximum length command */

int main(void)
{
        int should_run = 1; //프로그램 실행, 종료 flag

        while (should_run) {
                printf("osh>");
                fflush(stdout);

                // 입력값 전체 command에 저장
                char command[MAX_LINE/2 + 1];
                fgets(command, sizeof(command), stdin);
                command[strlen(command)-1] = 0; // fgets은 \n도 입력값으로 받기 때문에 \n를 0으로 변경

                // args에 입력값 전체 token 저장
                char *args[MAX_LINE/2 + 1] = {NULL,}; // 입력값 전체 token 저장
                int s_num = 0; // 입력값 token 개수
                char *ptr = strtok(command, " ");
                while (ptr != NULL){
                        args[s_num] = ptr;
                        s_num++;
                        ptr = strtok(NULL, " ");
                }

                // background(&) 실행 flag 설정         
                int background_flag = 0;
                if (strcmp(args[s_num-1],"&") == 0){
                        background_flag = 1;
                        s_num -= 1;
                }

                // execvp의 인자 배열에 들어갈 인자 수이면서 '<', '>', '|'의 인덱스 번호인 k 구하기
                int i = 0;
                int k = s_num;
                int pr_flag = 0; // pip or redirection flag 
                while (args[i] != NULL){
                        if((strcmp(args[i],">")==0) || (strcmp(args[i],"<")==0) || (strcmp(args[i],"|")==0)){
                                k = i;
                                pr_flag = 1;
                                }
                        i++;
                }

                // '<', '>', '|' 앞에 있는 token들을 param에 저장, execvp에 들어갈 인자 배열 만들기 
                char *param[MAX_LINE/2 + 1] = {NULL,};
                for (int a=0; a<k; a++){
                        param[a] = args[a];
                }

                // '|' 뒤에 나오는 명령어 인자 배열 만들기              
                char *param2[MAX_LINE/2 + 1] = {NULL,};
                int j = 0;
                if (background_flag == 1){
                        while (strcmp(args[k+pr_flag+j], "&")!=0){
                                param2[j] = args[k+1+j];
                                j++;
                        }
                }
                else{
                        while (args[k+1+j] != NULL){
                                param2[j] = args[k+1+j];
                                j++;
                        }
                }

                int fd[2];
                pipe(fd);

                pid_t pid;
                pid = fork();

                if (pid == 0) {
                        //에러 방지: strcmp 인자로 null이 들어가면 오류가 발생하기 때문에
                        if (args[k] == NULL){
                                args[k] = "error_prev";
                        }

                        // output redirection
                        if (strcmp(args[k],">") == 0){
                                int file = open(args[k+1], O_WRONLY | O_CREAT, 0755);
                                dup2(file, STDOUT_FILENO);
                                close(file);
                                execvp(param[0], param);
                        }

                        // input redirecntion
                        else if (strcmp(args[k],"<") == 0){
                                int file = open(args[k+1], O_RDONLY);
                                dup2(file, STDIN_FILENO);
                                close(file);
                                execvp(param[0], param);
                        }

                        // pipe 
                        else if (strcmp(args[k],"|") == 0){
                                int fd2[2];
                                pipe(fd2);

                                pid_t pid2;
                                pid2 = fork();

                                // '|' 앞에 나오는 명령어 출력값 파이프에 write
                                if (pid2 == 0){
                                        close(fd2[0]);
                                        dup2(fd2[1], STDOUT_FILENO);
                                        execvp(param[0], param);
                                }
                                // 파이프에서 read한 출력값을 '|' 뒤에 나오는 명령어의 입력값으로 받기
                                else if (pid2 > 0){
                                        wait(NULL);
                                        close(fd2[1]);
                                        dup2(fd2[0], STDIN_FILENO);
                                        execvp(param2[0], param2);
                                }
                                else{
                                        fprintf(stderr, "Fork failed");
                                        return 1;
                                }
                        }

                        // exit을 입력하면 프로그램 종료
                        else if (strcmp(args[0],"exit") == 0){
                                close(fd[0]);
                                should_run = 0;
                                write(fd[1], &should_run, 4);
                                return 0;
                        }

                        // 첫 번째 역할: '>', '<', '|' 없이 명령어만 입력한 경우 입력한 명령어 실행
                        // 두 번째 역할: 잘못된 명령어를 입력한 경우 프로그램 종료
                        else{
                                execvp(param[0], param);
                                close(fd[0]);
                                should_run = 0;
                                write(fd[1], &should_run, 4);
                                printf("해당 명령어가 없으므로 프로그램을 종료합니다\n");
                                return 0;
                        }
                }
                else if (pid > 0) {
                        // foreground 실행 
                        if (background_flag == 0){
                                wait(NULL);
                                close(fd[1]);
                                read(fd[0], &should_run, 4);
                        }
                        // background 실행 
                        else{
                                waitpid(pid, NULL, WNOHANG);
                                printf("pidNum:%d\n", pid);
                        }
                }
                else {
                        fprintf(stderr, "Fork failed");
                        return 1;
                }
        }
        return 0;
}

