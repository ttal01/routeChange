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

	static char* route[][4] = {
			{"gw","192.168.1.1","dev","eth0"}, 
			{"gw","0.0.0.0","dev","ppp0"},
			{"gw","192.168.1.1","dev","wlan0"}
						};

//确认默认路由是否是以太网
int checkRouteIsEthernet(char* route[][4])
{
	char cmd[128] = {0};
	sprintf(cmd ,"test \"$(route -n| awk 'NR==3{print $2}')\" = \"%s\"", route[0][1]);
	int ret = system(cmd);
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
int checkRouteType(char* type[][4], int n)
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
	int size = sizeof(route)/sizeof(*route);

	char cmd[128] = {0};
	
	//bool_e=1 直接切换到以太网路由  0 按优先级级顺序更换路由 
	if(bool_e)
	{
		//删除全部默认路由 删除完毕退出循环
		sprintf(cmd, "route del default");
		
		while(0 == system(cmd)) ;
		
		//启动以太网路由
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

			num = checkRouteType(route, size);
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
		
		//添加默认路由为下一优先级路由
		//sprintf(cmd, "wr route add default gw %s", route[num%size]);
		
		//切换成功退出 最多切换3次
		int count = 3;
		do{
			++num;		//确定下一优先级路由
			sprintf(cmd, "route add default %s %s %s %s", route[num%size][0], route[num%size][1],
									route[num%size][2], route[num%size][3]);
			
			ret = system(cmd);
			if(ret != 0)
			{
				fprintf(stderr, "%s :error\n", cmd);
				//return -1;
			}
			else
			{
				fprintf(stdout, "%s :success\n", cmd);
			}
			usleep(1000);
		}while(ret && --count>=0);
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
	struct itimerval timer = {{20, 0},{20, 0}};
	
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
			ret = changeRoute(0);
			if(ret != 0)
			{
				fprintf(stderr, "changeRoute error：all route i\n");
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

int main(int argc, char** argv)
{
	//设置定时器 定时尝试切换到以太网路由
	
	//网线插上事件触发 尝试切换到以太网路由
	
	//启动设置路由 优先级：以太网>4G>wifi
	//1.检查是否是默认路由 不是则切换
	
	//测试联网情况 无法连接时，自动切换下一优先级网络
	
	test();
	
	return 0;
}








