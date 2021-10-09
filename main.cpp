#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <cutils/properties.h>
#include "long2time.h"

#define JAN_1970 0x83aa7e80
#define NTPFRAC(x) (4294 * (x) + ((1981 * (x)) >> 11))
#define USEC(x) (((x) >> 12) - 759 * ((((x) >> 10) + 32768) >> 16))
#define Data(i) ntohl(((unsigned int *)data)[i])
#define LI 0
#define VN 3
#define MODE 3
#define STRATUM 0
#define POLL 4
#define PREC -6

struct NtpTime
{
	unsigned int coarse;
	unsigned int fine;
};

void sendPacket(int fd)
{
	unsigned int data[12];
	struct timeval now;

	if (sizeof(data) != 48)
	{
		fprintf(stderr, "size error\n");
		return;
	}

	memset((char *)data, 0, sizeof(data));
	data[0] = htonl((LI << 30) | (VN << 27) | (MODE << 24) | (STRATUM << 16) | (POLL << 8) | (PREC & 0xff)); //构造协议头部信息
	data[1] = htonl(1 << 16);
	data[2] = htonl(1 << 16);
	gettimeofday(&now, NULL);
	data[10] = htonl(now.tv_sec + JAN_1970); //构造传输时间戳
	data[11] = htonl(NTPFRAC(now.tv_usec));
	send(fd, data, 48, 0);
}
//获取NTP服务器返回的时间
void getNewTime(unsigned int *data, struct timeval *ptimeval)
{
	struct NtpTime trantime;
	trantime.coarse = Data(10);
	trantime.fine = Data(11);

	ptimeval->tv_sec = trantime.coarse - JAN_1970;
	ptimeval->tv_usec = USEC(trantime.fine);
}

int getNtpTime(struct hostent *phost, struct timeval *ptimeval)
{
	if (phost == NULL)
	{
		debugpri("err:host is null!\n");
		return -1;
	}
	int sockfd;
	struct sockaddr_in addr_src, addr_dst;
	fd_set fds;
	int ret;
	int recv_len;
	unsigned int buf[12];
	memset(buf, 0, sizeof(buf));
	int addr_len;
	int count = 0;

	struct timeval timeout;

	addr_len = sizeof(struct sockaddr_in);

	memset(&addr_src, 0, addr_len);
	addr_src.sin_family = AF_INET;
	addr_src.sin_addr.s_addr = htonl(INADDR_ANY);
	addr_src.sin_port = htons(0);

	memset(&addr_dst, 0, addr_len);
	addr_dst.sin_family = AF_INET;
	memcpy(&(addr_dst.sin_addr.s_addr), phost->h_addr_list[0], 4);
	addr_dst.sin_port = htons(123); //ntp默认端口123

	if (-1 == (sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))) //创建UDP socket
	{
		debugpri("create socket error!\n");
		return -1;
	}

	ret = bind(sockfd, (struct sockaddr *)&addr_src, addr_len); //bind
	if (-1 == ret)
	{
		debugpri("bind error!\n");
		close(sockfd);
		return -1;
	}

	ret = connect(sockfd, (struct sockaddr *)&addr_dst, addr_len); //连接NTP服务器
	if (-1 == ret)
	{
		debugpri("connect error!\n");
		close(sockfd);
		return -1;
	}
	sendPacket(sockfd); //发送请求包
	while (count < 50)	//轮询请求
	{
		FD_ZERO(&fds);
		FD_SET(sockfd, &fds);

		timeout.tv_sec = 0;
		timeout.tv_usec = 100000;
		ret = select(sockfd + 1, &fds, NULL, NULL, &timeout);
		if (0 == ret)
		{
			count++;
			debugpri("ret == 0\n");
			sendPacket(sockfd);
			usleep(100 * 1000);
			continue;
		}
		if (FD_ISSET(sockfd, &fds))
		{
			recv_len = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&addr_dst, (socklen_t *)&addr_len);
			if (-1 == recv_len)
			{
				debugpri("recvfrom error\n");
				close(sockfd);
				return -1;
			}
			else if (recv_len > 0)
			{
				debugpri("receiv data\n");
				getNewTime(buf, ptimeval);
				debugpri("sec = %ld usec = %ld", ptimeval->tv_sec, ptimeval->tv_usec); //打印输出NTP服务器返回的时间
				change(ptimeval->tv_sec + 8 * 3600);
				setRtcTime(ptimeval->tv_sec * 1000);
				break;
			}
		}
		else
		{
			debugpri("count %d \n", count);
			usleep(50 * 1000);
			count++;
		}
	}
	if (count >= 50)
	{
		debugpri("getNewTime   timeout fail \n");
		close(sockfd);
		return -1;
	}
	close(sockfd);
	return 0;
}

int main(int argc, char **argv)
{
	char noethernet[PROPERTY_VALUE_MAX];
	property_get("persist.ruichi.native_agent", noethernet, NULL);
	if (strcmp("1", noethernet) != 0)
	{
		INFO("persist.ruichi.native_agent = 0");
		return 0;
	}
	struct timeval TimeSet;
	static struct hostent *host = NULL;
	char ntpServer[PROPERTY_VALUE_MAX];

	char CrtcLastTime[PROPERTY_VALUE_MAX];
	char CsettingTime[PROPERTY_VALUE_MAX];
	long rtcNowTime;
	long currentTime;
	long settingTime;
	long rtcLastTime;

	INFO("vclusters argc = %d argv[1] = %s\n", argc, argv[1]);

	if (argc == 2)
	{
		if (strcmp("start", argv[1]) == 0)
		{

			property_get("persist.sys.settingsrtc", CrtcLastTime, NULL);
			property_get("persist.sys.time", CsettingTime, NULL);
			settingTime = atol(CsettingTime);
			rtcLastTime = atol(CrtcLastTime);
			rtcNowTime = getRtcTime();

			currentTime = settingTime + (rtcNowTime - rtcLastTime) * 1000;
			INFO("settingTime = %ld  rtcNowTime = %ld rtcLastTime = %ld currentTime = %ld", settingTime, rtcNowTime, rtcLastTime, currentTime);
			change(currentTime / 1000 + 8 * 3600);
			sleep(10);
			property_get("persist.ruichi.ntpServer", ntpServer, DEFAULT_NTP_SERVER);
			host = gethostbyname(ntpServer);
			//INFO("persist.ruichi.ntpServer = %s",ntpServer);
		}
		else
		{
			host = gethostbyname(argv[1]);
			property_set("persist.ruichi.ntpServer", argv[1]);
		}
		memset(&TimeSet, 0, sizeof(TimeSet));
		getNtpTime(host, &TimeSet);
	}
	return 0;
}
