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

#define		MAX_BUFF		1024		//用户输入的最大长度
#define		MAX_COMM		128			//单个命令及其参数最大字符长度

#define		INCREASE		0		//链表插入
#define		DELETE			1		//链表节点删除
#define		PRINT			2		//打印链表

char cwd[MAX_BUFF];			//当前路径
char *commands;
char *all_command[MAX_COMM];	//折分后的单条命令及其参数
int		current_out;
int		current_in;

//后台进程结构体
struct proc {
	pid_t	pid;
	int		status;
	char	*argv[MAX_COMM];
	struct proc		*next;
};

typedef struct proc		proc;
proc *head = NULL;		//头结点
/**
 * 获取当前的路径和用户信息
 */
void print_prompt(void)
{
	struct utsname		hostinfo;
	uname(&hostinfo);
	getcwd(cwd, sizeof(cwd));

	setbuf(stdout, NULL);
	printf("[%s@%s:%s] $ ", hostinfo.nodename, hostinfo.sysname, cwd);
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
* 释放内存
*/
void clean(void)
{
	proc *iter = head;
	proc *next;
	int count;
	for (count = 0; all_command[count] != NULL; count++)
		free(all_command[count]);
	while (iter != NULL) {
		next = head->next;
		for (count = 0; iter->argv[count] != NULL; count++)
			free(iter->argv[count]);
		free(iter);
		iter = next;
	}
}

/**
* 任务列表，数据结构为链表，功能为记录当前后台运行任务数量
*/
void bg_struct_handler(pid_t pid, char **argv, int type)
{
	int count;
	proc *new, *iter;
	// 新增一个后台任务
	if (type == INCREASE) {
		// 初始化
		new = (proc *)malloc(sizeof(proc));
		new->pid = pid;
		new->status = 1;
		new->next = NULL;
		for (count = 0; argv[count] != NULL; count++) {
			new->argv[count] = (char *)malloc(MAX_COMM);
			strcpy(new->argv[count], argv[count]);
		}
		new->argv[count] = NULL;
		// 第一个加进后台任务列表的线程
		if (head == NULL) {
			head = new;
		}
		else {
			iter = head;
			while (iter->next != NULL)
				iter = iter->next;
			iter->next = new;
		}
	}
	// 一个后台进程终止
	else if (type == DELETE) {
		proc *preiter = NULL;
		iter = head;
		while (iter != NULL && iter->pid != pid) {
			preiter = iter;
			iter = iter->next;
		}
		if (iter == NULL) {
			printf("匹配失败，该进程已不存在\n");
			return;
		}
		// 匹配成功
		else if (iter->pid == pid) {
			// 匹配到的是第一个任务
			if (preiter == NULL)
				head = iter->next;
			// 非第一个任务
			else
				preiter->next = iter->next;

			// 释放该任务进程的内存空间
			for (count = 0; iter->argv[count] != NULL; count++) {
				free(iter->argv[count]);
			}
			free(iter);
		}
	}
	// 打印该进程的状态信息
	else if (type == PRINT) {
		count = 0;
		iter = head;
		// 后台进程链表为空
		if (iter == NULL) {
			printf("没有正在运行的后台进程！\n");
			return;
		}
	
		while (iter != NULL) {
			//输出流恢复原来的状态，puts输出到控制台
			setbuf(stdout, NULL);
			printf("[%d]", count + 1);
			while (iter->argv[count] != NULL) {
				// 打印指令
				printf("%s ", iter->argv[count]);
				count++;
			}
			// 打印进程号
			printf("[%d]\n", iter->pid);
			iter = iter->next;
		}
	}
	return;
}

/**
*
*/
void bg_signal_handler(int sig)
{
	int status;
	pid_t pid;
	proc *iter = head;
	// 有效避免后台进程成为僵尸进程
	// 参考博客：https://www.cnblogs.com/Harley-Quinn/p/7157579.html
	pid = waitpid(-1, &status, WNOHANG);
	while (iter != NULL) {
		if (iter->pid == getpid())
			bg_struct_handler(pid, NULL, 1);
	}
}

/**
* 执行重定向下的写/读操作
*/
void bf_exec(char **argv, int type)
{
	pid_t pid;
	int status;
	// type为0时表示非后台进程
	if (type == 0) {
		// 子进程
		if ((pid = fork()) < 0) {
			printf("*** ERROR:froking child process faild\n");
			return;
		}
		else if (pid == 0) {
			signal(SIGTSTP, SIG_DFL);
			execvp(argv[0], argv);
		}
		// 父进程阻塞等待子线程执行完毕
		else {
			pid_t c;
			signal(SIGTSTP, SIG_DFL);
			c = wait(&status);
			// 子进程终止，父进程把标准输入输出的指针指回原stdin_info
			dup2(current_out, 1);
			dup2(current_in, 0);
			return;
		}
	}
	//后台进程
	else {						
		signal(SIGCHLD, bg_signal_handler);
		// 子进程
		if ((pid = fork()) < 0) {
			printf("*** ERROR:forking child process faild\n");
			return;
		}
		else if (pid == 0) {
			execvp(argv[0], argv);
		}
		// 父进程不等待
		else {
			bg_struct_handler(pid, argv, 0);
			// 父进程把标准输入输出的指针指回原stdin_info
			dup2(current_out, 1);
			dup2(current_in, 0);
			return;
		}
	}
}

/**
*
*/
void file_out(char **argv, char *out_file, int type)
{
	int		fd_out;
	// 只写模式，如果文件不存在就创建
	// type==0是">"
	if (type == 0)
		fd_out = open(out_file, O_WRONLY | O_CREAT, 0777);
	// type==1是">>"
	else	
		fd_out = open(out_file, O_WRONLY | O_CREAT | O_APPEND, 0777);
	// 绑定标准输出的指针为fd_out
	dup2(fd_out, 1);
	close(fd_out);
	// 执行重定向操作
	bf_exec(argv, 0);
}

/**
*
*/
//处理重定向的输入问题,其中包裹了重定向输出
void file_in(char **argv, char *in_file, char *out_file, int type)
{
	int fd_in;
	fd_in = open(in_file, O_RDONLY);
	// 绑定标准输入的指针为fd_in
	dup2(fd_in, 0);
	close(fd_in);
	if (type == 0) {			//没有重定向输出
		printf("Going to execute bf_exec\n");
		bf_exec(argv, 0);
	}
	else if (type == 1)		//重定向输出 >
		file_out(argv, out_file, 0);
	else					//重定向输出 >>
		file_out(argv, out_file, 1);
}

/**
* 执行当前指令
*/
void execute(char *single_command)
{
	// 分割当前指令的参数
	// 命令和参数的指针
	char *argv[MAX_COMM];
	char *single;
	int argv_count = 1;
	char *out_file_flag;
	char *out_file;
	// 根据空格分割参数和指令
	argv[0] = strtok(single_command, " ");
	argv[1] = NULL;
	// 匹配分支指令
	if (strcmp(argv[0], "cd") == 0) {
		// 指向下一个指令，也就是路径
		single = strtok(NULL, " ");
		chdir(single);
	}
	if (strcmp(argv[0], "exit") == 0) {
		clean();
		printf("退出程序中...");
		exit(0);
	}

	// 匹配非cd非exit指令
	while (1) {
		// 处理完指令，退出
		single = strtok(NULL, " ");
		if (single == NULL)
			break;
		// 匹配重定向——覆盖写出
		else if (strcmp(single, ">") == 0) {
			// 读取输出的文件名
			single = strtok(NULL, " ");
			// 写入文件
			file_out(argv, single, 0);
			// 完成一次指令执行
			return;
		}
		// 匹配重定向——添加写出
		else if (strcmp(single, ">>") == 0) {
			single = strtok(NULL, " ");
			file_out(argv, single, 1);
			return;
		}
		// 匹配重定向——写入
		else if (strcmp(single, "<") == 0) { 
			single = strtok(NULL, " ");	//输入重定向文件名
			out_file_flag = strtok(NULL, " ");
			if (strcmp(out_file_flag, ">") == 0) {
				out_file = strtok(NULL, " ");
				if (out_file == NULL) {
					printf("Syntax error\n");
					return;
				}
				else
					file_in(argv, single, out_file, 1);
			}
			else if (strcmp(out_file_flag, ">>") == 0) {
				out_file = strtok(NULL, " ");
				if (out_file == NULL) {
					printf("Syntax error\n");
					return;
				}
				else
					file_in(argv, single, out_file, 2);
			}
			else
				file_in(argv, single, NULL, 0);
		}
		else if (strcmp(single, "&") == 0) {
			bf_exec(argv, 1);
			return;
		}
		else {
			argv[argv_count] = single;
			argv_count++;
			argv[argv_count] = NULL;
		}
	}

	bf_exec(argv, 0);
	return;
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
*
*/
int main(int argc, char **argv)
{
	int count;
	int commands_num;
	// 当使用重定向符时，把标准输入输出从stdin改为相对应的文件
	current_out = dup(1);
	current_in = dup(0);
	// 分配内存和初始化
	commands = (char *)malloc(sizeof(MAX_BUFF));

	for (; count < MAX_COMM; count++) {
		all_command[count] = (char *)malloc(MAX_COMM);
	}
	while (1){
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
			execute(all_command[commands_num]);
			commands_num++;
		}
	}
}
