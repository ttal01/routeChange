#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<string.h>
#include<errno.h>
#include<stdlib.h>
#include<stdbool.h>
#include<sys/time.h>
#include<signal.h>

	// static char* route[][4] = {
			// {"gw","192.168.1.1","dev","eth0"}, 
			// {"gw","0.0.0.0","dev","ppp0"},
			// {"gw","192.168.1.1","dev","wlan0"}
						// };

#define COLUMN 4
int CHANGETIME = 3600;	//定期切换到以太的周期，可能导致断网 
typedef char* ROUTE[COLUMN];
static ROUTE* route = NULL;
static int row_index = 0;
						
//加载配置文件
void LoadRouteConfigFile(void)
{
	char name[32];
    memset(name,0x00,32);
    strcpy(name,"./route.dll");
    FILE *f=NULL;

    f = fopen(name,"a+");
    if(f!=NULL)
    {
        char* wz;
        int js=0;
        char nr[32];
        int temp1=0;
        int j=0;
		
		//计算文件行数
		char buf[1024] = {0};
		if(NULL == fgets(buf,1024,f) )
		{
			fprintf(stderr,"第一行不能是空行\n");
		}
		CHANGETIME = atoi(buf);
		printf("CHANGETIME : %d s\n", CHANGETIME);
		
		
		while(1)
		{
			if(NULL == fgets(buf,1024,f) )
			{
				break;
			}
			if(0 == strcmp(buf,"\n") || 0 == strcmp(buf,"\r\n"))
			{
				break;
			}
			
			++row_index;
			memset(buf, 0, sizeof(buf));
		}
		
		//分配空间
		route = (ROUTE*)malloc(row_index*sizeof(ROUTE));
		if(NULL == route)
		{
			fprintf(stderr,"malloc ROUTE error\n");
			return;
		}
		memset(route, 0, row_index*sizeof(ROUTE));
		
		//重置文件读取位置
		rewind(f);
		
		//读取配置文件
		int i = 0;
        while(i != row_index)
        {
			fgets(buf,sizeof(buf),f);
            if(NULL == strstr(buf,",") || 0 == strncmp(buf,"//", 2))
                continue;
            js=0;
            j=0;
            if(strlen(buf)<=1)
                break;
					
            wz=strstr(buf+js,",");
            while(wz!=NULL)
            {
                temp1=wz-buf;
                temp1=temp1-js;
								
				char* item = (char*)malloc(temp1+1);
				if(NULL == item)
				{
					fprintf(stderr,"malloc item error\n");
					return;
				}
				memset(item,0x00,temp1+1);
                memcpy(item,buf+js,temp1);
				

				route[i][j++] = item;

				if(COLUMN == j)
				{
					break;
				}
                js=wz-buf;
                js++;
                if(strlen(buf)<=js)
                    break;
                wz=strstr(buf+js,",");
            }
			
			++i;
        }
        fclose(f);
    }
}						
						
						
//确认默认路由是否是以太网  返回0为以太
int checkRouteIsEthernet(ROUTE* route)
{
	char cmd[128] = {0};
	int ret = 0;
	
	//判断是否只有一个默认路由 如果存在多个默认路由，无法准确知道数据到底走第一个还是第二个路由
	sprintf(cmd ,"test `route -n|awk '{print $1}'|grep '0.0.0.0'|wc -l` -eq 1");
	ret = system(cmd);
	if(ret != 0)
	{
		return ret;
	}
	
	//只有一个默认路由的情况下，检查是否是以太网路由
	sprintf(cmd ,"test \"$(route -n| awk 'NR==3{print $2}')\" = \"%s\"", route[0][1]);
	ret = system(cmd);
	if(ret == 0)
	{
		sprintf(cmd ,"test \"$(route -n| awk 'NR==3{print $8}')\" = \"%s\"", route[0][3]);
		ret = system(cmd);
		if(0 == ret)
		{
			printf("route is ethernet, ip: %s  dev: %s\n", route[0][1], route[0][3]);
		}
	}
	else
	{
		printf("route is not ethernet, ret=%d\n", ret);
	}
	
	return ret;
}

/*						
//确认默认路由是否是以太网
int checkRouteIsEthernet(char* ip)
{
	char cmd[128] = {0};
	sprintf(cmd ,"test \"$(route -n| awk 'NR==3{print $2}')\" = \"%s\"", ip);
	int ret = system(cmd);
	if(ret == 0)
	{
		printf("route is ethernet, ip: %s\n", ip);
	}
	else
	{
		printf("route is not ethernet, ret=%d\n", ret);
	}
	
	return ret;
}
*/

//确认默认路由是哪种
int checkRouteType(ROUTE* type, int n)
{
	char cmd[128] = {0};
	int i = 0;
	int ret = 0;
	
	//检查是否默认是以太网路由
	checkRouteIsEthernet(route);
		
	for(i=0; i!=n; ++i)
	{
		sprintf(cmd ,"test \"$(route -n| awk 'NR==3{print $8}')\" = \"%s\"", type[i][3]);
		ret = system(cmd);
		if(ret == 0)
		{
			fprintf(stdout, "default route type is %s\n", type[i][3]);
			return i;
		}
	}
	
	fprintf(stderr, "error: can not gei route type\n");	
	return -1;
}

//按优先级顺序更换路由  bool_e=1 直接切换到以太网路由  0 按优先级级顺序更换路由 
int changeRoute(bool bool_e)
{
	static int num = 0;		//记录当前默认路由
	static int hasCheckOut = 0;	//记录是否已获取默认路由
	int ret = 0;
	// int num = 0;
	
	// char* route[][2] = {
			// {"gw","192.168.1.1"}, 
			// {"dev","ppp0"}
						// };
	// int size = sizeof(route)/sizeof(*route);

	char cmd[128] = {0};
	
	//bool_e=1 直接切换到以太网路由  0 按优先级级顺序更换路由 
	if(bool_e)
	{
		// printf("---------------0--------------------\n");
		//默认是以太网路由 直接退出
		ret = checkRouteIsEthernet(route);
		if(0 == ret)
		{
			return ret;
		}
		//删除全部默认路由 删除完毕退出循环
		sprintf(cmd, "route del default");
		while(0 == system(cmd)) ;
		
		//为防止以太网路由出现在路由表低序列中 删除操作 该步骤不会影响现有网络
		// sprintf(cmd, "route del default %s %s %s %s", route[0][0], route[0][1], route[0][2], route[0][3]);	
		// while(0 == system(cmd)) ;
		// system(cmd);
		
		//添加默认以太网路由
		sprintf(cmd, "route add default %s %s %s %s", route[0][0], route[0][1], route[0][2], route[0][3]);			
		ret = system(cmd);
		
		if(ret != 0)
		{
			fprintf(stderr, "%s :error\n", cmd);
			//return -1;
		}
		else
		{
			fprintf(stdout, "%s :success\n", cmd);
			num = 0;
			hasCheckOut = 1;		//直接设定路由则相当于也进行了检查
		}
		// printf("---------------000--------------------\n");
	}
	else
	{
		//确认当前路由
		if(!hasCheckOut)
		{
			// ret = checkRouteIsEthernet(route);
			// if(0 == ret)
			// {
				// num = 0;
			// }

			num = checkRouteType(route, row_index);
			// if(-1 == num)
			// {
				// fprintf(stderr, "type error\n");
				// 非法路由
				// num = 0-1;
			// }

			hasCheckOut = 1;
		}
		
		
		
		//删除默认路由 返回错误 判断是否删除完毕 会否删除非网关内容11111111111111111111111111111
		
		// sprintf(cmd, "route | grep 'default' | wc -l");
		// num = systme(cmd);
		//删除全部默认路由 删除完毕退出循环
		sprintf(cmd, "route del default");
		while(0 == system(cmd)) ;
		
		
		// printf("------------------------1------------null-------\n");
		
		// sleep(3); //测试
		
		//添加默认路由为下一优先级路由
		//sprintf(cmd, "wr route add default gw %s", route[num%size]);
		
		//切换成功退出 最多切换3次
		int count = 3;
		do{
			++num;		//确定下一优先级路由
			sprintf(cmd, "route add default %s %s %s %s", route[num%row_index][0], route[num%row_index][1],
									route[num%row_index][2], route[num%row_index][3]);
			
			ret = system(cmd);
			if(ret != 0)
			{
				fprintf(stderr, "%s :error\n", cmd);
				//return -1;
			}
			else
			{
				fprintf(stdout, "%s :success\n", cmd);
				break;
			}
			usleep(1000);
		}while(ret && --count>=0);
		// printf("------------------------2-------------------\n");
	}


	
	return ret;
}

//信号处理函数
void sig_handler(int sig)
{
	//检查默认路由是否是以太网
	if(checkRouteIsEthernet(route))
	{
		//若不是 则尝试切换到以太网路由
		changeRoute(1);
	}
}

int test()
{
	int fd = 0;
	int ret = 0;
	
	struct timeval ctimeo = {3, 0};		//超时时间
	socklen_t len = sizeof(ctimeo);
	
	struct sockaddr_in addr;		//服务器地址
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(80);
	if(-1 == inet_pton(AF_INET, "111.13.101.208", &addr.sin_addr))	//暂定百度地址
	{
		perror("inet_pton error");
		return -1;
	}
	
	
	//设置定时器 定时尝试切换到以太网路由
	struct itimerval timer = {{CHANGETIME, 0},{CHANGETIME, 0}};
	
	ret = setitimer(ITIMER_REAL, &timer, NULL);
	if(-1 == ret)
	{
		perror("setitimer error");
		return -1;
	}
	
	//注册信号处理函数 SIGALRM
	struct sigaction act;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = sig_handler;
	
	ret = sigaction(SIGALRM, &act, NULL);
	if(-1 == ret)
	{
		perror("sigaction error");
		return -1;
	}
	
	//网线插上事件触发 尝试切换到以太网路由
	
	//初始时设置以太网连接 路由 优先级：以太网>4G>wifi
	//检查是否是默认路由 不是则切换
	ret = checkRouteIsEthernet(route);
	// printf("==========00000000======ret:%d\n",ret);
	if(ret != 0)
	{
		changeRoute(1);		//强制直接更换为以太网路由
	}
	
	//主循环 测试联网情况 无法连接时，自动切换下一优先级网络
	while(1)
	{
		// int flag = 0;
		fd = socket(AF_INET, SOCK_STREAM, 0);
		if(-1 == fd)
		{
			perror("socket error");
			return -1;
		}
		
		//设定连接超时
		if(-1 == setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &ctimeo, len))
		{
			perror("setsockopt error");
			return -1;
		}
	

		//建立连接  成功则保持 失败则切换路由
		ret = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
		if(-1 == ret)
		{
			perror("connetc error");

			// if(errno == EINPROGRESS && flag <= 3)
			if(errno == EINPROGRESS)
			{
				fprintf(stderr, "errno:%d  time out\n", errno);
				//++flag;			
				//continue;
			}
			else
			{
				fprintf(stderr, "errno:%d  connect error\n", errno);
			}

			//按优先级级顺序更换路由
			//阻塞定时器信号
			sigset_t blockSet, prevMask;
			sigemptyset(&blockSet);
			sigaddset(&blockSet, SIGALRM);
			
			if(sigprocmask(SIG_BLOCK, &blockSet, &prevMask) == -1)
			{
				perror("sigprocmask error to block SIGALRM signal");
			}
			// printf("================ret:%d\n",ret);
			ret = changeRoute(0);
			
			if(ret != 0)
			{
				fprintf(stderr, "changeRoute error：all route i\n");
			}
			
			//解除阻塞
			if(sigprocmask(SIG_SETMASK, &prevMask, NULL) == -1)
			{
				perror("sigprocmask error to reset SIGALRM signal");
			}
		}
		else
		{
			printf("connect success\n");
			// close(fd);
			// break;
			sleep(2);
		}
		
		close(fd);
		sleep(2);
		
		//flag = 0;
			
	}
	
	printf("process : route manager exit\n");
	return 0;
}

//判断是否已启动该程序
void checkRouteProcess(char* arg)
{
	int ret = 0;
	char cmd[128] = {0};
	sprintf(cmd, "test `ps ajx|grep -v 'grep'|grep '%s'|wc -l ` -eq 1 ",arg);
	
	ret = system(cmd);	//判断是否只有当前的进程存在 0为真 非0为多于1个进程存在
	
	if(ret != 0)
	{
		fprintf(stderr, "Failure: switchRoute process has existed!\n");
		exit(ret);
	}
	else
	{
		fprintf(stdout, "OK: switchRoute process started!\n");
	}
}

int main(int argc, char** argv)
{
	//判断是否已启动该程序
	checkRouteProcess(argv[0]);
	//设置定时器 定时尝试切换到以太网路由
	
	//网线插上事件触发 尝试切换到以太网路由
	
	//启动设置路由 优先级：以太网>4G>wifi
	//1.检查是否是默认路由 不是则切换
	
	//测试联网情况 无法连接时，自动切换下一优先级网络
	
	//读取配置文件
	LoadRouteConfigFile();
	test();
	
	// for(int i=0; i!=row_index; ++i)
	// {
		// for(int j=0; j!=COLUMN; ++j)
		// {
			// printf("%s,",route[i][j]);
		// }
		// printf("\n");
	// }
	
	return 0;
}








