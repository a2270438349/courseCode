/*************************************************************************
> File Name: myshell.c
> Author: Bin Xu
> Mail: x_shares@outlook.com
> Created Time: 2016年12月08日 星期四 18时46分01秒
************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include<pwd.h>

#define		MAX_BUFF		1024		//用户输入的最大长度
#define		MAX_COMM		128			//单个命令及其参数最大字符长度

char cwd[MAX_BUFF];			//当前路径
char *commands;
char *all_command[MAX_COMM];	//折分后的单条命令及其参数
int toExit;

/**
* 获取当前的路径和用户信息
*/
void print_prompt(void)
{

	struct passwd* pass;
	pass = getpwuid(getuid());
	getcwd(cwd, sizeof(cwd));
	setbuf(stdout, NULL);
	printf("[%s@%s ~  %s] $ ", pass->pw_name, pass->pw_shell, cwd);
}


/**
* 处理键盘组合键产生的终止信号
*/
void sig_handler(int sig)
{
	// 捕获信号，但是不处理
	if (sig == SIGINT || sig == SIGQUIT) {
		printf("你输入了键盘组合键，但我不确定你是不是要退出，退出请键入exit!\n");
		print_prompt();
	}
}

/**
* 读取用户的输入指令
*/
void scan_user_input(void)
{
	ssize_t nbyte;
	size_t len = MAX_BUFF;
	// 根据字节数来赋终止符
	// 原型：ssize_t getline(char **lineptr, size_t *n, FILE *stream);
	nbyte = getline(&commands, &len, stdin);
	commands[nbyte - 1] = '\0';
}


/**
* 提取分号隔开的每条指令
*/
void parse_by_semicolon(char *command)
{
	int count = 0;
	// 提取分号隔开的每条指令
	count = 0;
	all_command[count] = strtok(command, ";");

	while (1) {
		count++;
		all_command[count] = strtok(NULL, ";");
		if (all_command[count] == NULL)
			break;
	}
}

/**
* 释放分割指令占用的数组
*/
void clean(void)
{
	int count;
	for (count = 0; all_command[count] != NULL; count++)
		free(all_command[count]);
}


/**
* 每次解析命令都要新建一个子进程来运行execvp指令，不能用while循环来写死execvp指令
*/
void bf_exec(char **argv, int type)
{
	pid_t pid;
	int status;
	if ((pid = fork()) < 0) {
		printf("创建子进程失败\n");
		return;
	}
	else if (pid == 0) {
		//printf("\n子进程等待退出\n");
		execvp(argv[0], argv);
		//printf("\n子进程退出\n");
	}
	// 父进程阻塞等待子线程执行完毕
	else {
		//printf("\n父进程等待退出\n");
		// 防止子进程僵死
		waitpid(pid, NULL, 0);
		//printf("\n父进程退出\n");
		return;
	}
}

/*
打印help指令
*/
void func()

{

	FILE * fp = NULL;

	char chBuffer[1024] = { 0 };

	char chCmd[100] = "help";

	fp = popen(chCmd, "r");

	if (fp)

	{

		memset(chBuffer, 0, sizeof(chBuffer));

		while (NULL != fgets(chBuffer, sizeof(chBuffer), fp))

		{

			printf("%s", chBuffer);

		}

		pclose(fp);
	}

}
/**
* 执行当前指令
*/
void execute(char *single_command)
{
	// 分割当前指令的参数
	// 命令和参数的指针
	char *argv[MAX_COMM];
	char *command;
	int argv_count = 1;
	// 根据空格分割参数和指令
	argv[0] = strtok(single_command, " ");
	argv[1] = NULL;
	// 匹配cd指令
	if (strcmp(argv[0], "cd") == 0) {
		// 指向下一个指令，也就是路径
		command = strtok(NULL, " ");
		chdir(command);
		return;
	}
	// 匹配help指令
	if (strcmp(argv[0], "help") == 0) {
		// 执行help指令
		func();
		return;
	}

	// 匹配exit指令
	if (strcmp(argv[0], "exit") == 0) {
		clean();
		printf("shell程序退出中...\n");
		toExit = 1;
		exit(EXIT_SUCCESS);
	}

	// 匹配非cd非exit指令
	while (1) {
		// 处理完指令，退出
		command = strtok(NULL, " ");
		if (command == NULL)
			break;
		else {
			argv[argv_count] = command;
			argv_count++;
			argv[argv_count] = NULL;
		}
	}
	bf_exec(argv, 0);
	return;
}

/**
*
*/
int main(int argc, char **argv)
{
	int numCount = 0;
	int count;
	int commands_num;
	toExit = 0;
	// 当使用重定向符时，把标准输入输出从stdin改为相对应的文件
	// 分配内存和初始化
	commands = (char *)malloc(sizeof(MAX_BUFF));

	for (; count < MAX_COMM; count++) {
		all_command[count] = (char *)malloc(MAX_COMM);
	}
	while (toExit == 0) {
		commands_num = 0;
		commands = NULL;
		for (count = 0; count < MAX_COMM; count++)
			all_command[count] = NULL;
		// 解决键盘输入组合键导致程序意外退出
		signal(SIGINT, sig_handler);
		signal(SIGQUIT, sig_handler);
		signal(SIGCHLD, sig_handler);
		signal(SIGTSTP, SIG_IGN);
		// 模拟shell中用户信息和当前目录
		print_prompt();
		// 等待用户输入
		scan_user_input();
		// 根据分号拆分单行输入的多条独立指令
		parse_by_semicolon(commands);
		// 执行故意的每条指令
		while (all_command[commands_num] != NULL) {
			//printf("执行次数=%d\n",++numCount);
			execute(all_command[commands_num]);
			commands_num++;
		}
	}
}
