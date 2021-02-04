/*
 * udpdisplay.c - send pixels to an android app
 * Copyright (C) 2021 Sanjay Rao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <time.h>
#include <netinet/in.h>

typedef void ignore;
#define ERRORMSG(a) fprintf a 
#define GOTOERROR do { ERRORMSG((stderr,"%s:%d: Error\n",__FILE__,__LINE__)); goto error; } while (0)
#define SCLEARFUNC(a) static void clear_##a(struct a *dest) { static struct a blank; memcpy(dest,&blank,sizeof(struct a)); }
SCLEARFUNC(sockaddr_in);

static int initwfd(int *wfd_out, uint32_t dipv4, unsigned short port) {
int fd=-1;
struct sockaddr_in sa;

clear_sockaddr_in(&sa);
sa.sin_family=AF_INET;
sa.sin_addr.s_addr=dipv4;
#if 1
{
	unsigned char b4[4];
	memcpy(b4,&dipv4,4);
	fprintf(stderr,"Connecting to %u.%u.%u.%u:%d\n",b4[0],b4[1],b4[2],b4[3],port);
}
#endif
sa.sin_port=htons(port);

fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
if (fd<0) GOTOERROR;
if (connect(fd,(struct sockaddr*)&sa,sizeof(sa))) GOTOERROR;

*wfd_out=fd;
return 0;
error:
	if (fd!=-1) close(fd);
	return -1;
}

static int initrfd(int *rfd_out, unsigned short *port_out, unsigned short port) {
struct sockaddr_in sa;
int fd=-1;
unsigned short offset=0;

fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
if (fd<0) GOTOERROR;
while (1) {
	clear_sockaddr_in(&sa);
	sa.sin_family=AF_INET;
	sa.sin_port=htons(port+offset);
	if (!bind(fd,(struct sockaddr*)&sa,sizeof(sa))) break;
	if (errno==EADDRINUSE) {
		offset+=1;
		if (offset==100) GOTOERROR;
	}
}

#if 0
{
	int k;
	char buff[512];
	fprintf(stderr,"Listening on port %d\n",port+offset);
	k=read(fd,buff,512);
	fprintf(stderr,"Read returned %d\n",k);
}
#endif

*port_out=port+offset;
*rfd_out=fd;
return 0;
error:
	if (fd!=-1) close(fd);
	return -1;
}

static int query_udp(unsigned int *width_out, unsigned int *height_out, int rfd, unsigned short port,
		int wfd, uint32_t dipv4) {
unsigned char request[3];
unsigned char buff508[508];
unsigned short n_short;
struct sockaddr_in from;
socklen_t fromlen;
int k;
unsigned int w=0,h=0;

request[0]=1;
n_short=htons(port); memcpy(request+1,&n_short,2);
if (3!=write(wfd,request,3)) GOTOERROR;
fprintf(stderr,"Sent query request\n");
fromlen=sizeof(from);
k=recvfrom(rfd,buff508,508,0,(struct sockaddr*)&from,&fromlen);
if (k<0) GOTOERROR;
fprintf(stderr,"Received reply with %d bytes\n",k);
if (from.sin_addr.s_addr!=dipv4) {
	fprintf(stderr,"Received packet from invalid ip\n");
} else {
	if ((k==5)&&(buff508[0]==1)) {
		memcpy(&n_short,buff508+1,2);
		w=ntohs(n_short);
		memcpy(&n_short,buff508+3,2);
		h=ntohs(n_short);
	}
}
*width_out=w;
*height_out=h;
return 0;
error:
	return -1;
}


static int drawonebox(uint32_t uid, unsigned int x, unsigned int y, unsigned int w, unsigned int h,
		unsigned char *rgb,
		int rfd, unsigned short port, int wfd, uint32_t dipv4) {
unsigned char buff508[508];
unsigned char *temp;
struct sockaddr_in from;
socklen_t fromlen;
unsigned short us;
int k,n;

fprintf(stderr,"Drawing x:%u, y:%u, w:%u, h:%u to port %u\n",x,y,w,h,port);

temp=buff508;
*temp=2; temp+=1;
#if 0
	if (!(uid&15)) port=0; // only wait for an ACK some of the time
#endif
us=htons(port); memcpy(temp,&us,2); temp+=2;
memcpy(temp,&uid,4); temp+=4;
us=htons(x); memcpy(temp,&us,2); temp+=2;
us=htons(y); memcpy(temp,&us,2); temp+=2;
us=htons(w); memcpy(temp,&us,2); temp+=2;
us=htons(h); memcpy(temp,&us,2); temp+=2;
n=w*h*3;
memcpy(temp,rgb,n);
n+=15;
while (1) {
	fd_set rset;
	struct timeval tv;
firstcontinue:
	if (n!=write(wfd,buff508,n)) GOTOERROR;
//	fprintf(stderr,"Sent draw request uid=%u\n",uid);

	if (!port) break; // no ACK possible

	fromlen=sizeof(from);
	while (1) {
		tv.tv_sec=1; tv.tv_usec=0; FD_ZERO(&rset); FD_SET(rfd,&rset);
		if (1!=select(rfd+1,&rset,NULL,NULL,&tv)) {
			fprintf(stderr,"Lost packet (1), retrying\n");
			goto firstcontinue;
		}
		k=recvfrom(rfd,buff508,508,0,(struct sockaddr*)&from,&fromlen);
		if (k<0) GOTOERROR;
		//fprintf(stderr,"Received reply with %d bytes\n",k);
		if (from.sin_addr.s_addr!=dipv4) {
			fprintf(stderr,"Received packet from invalid ip\n");
			continue;
		} else {
			if ((k==5)&&(buff508[0]==2)) {
				uint32_t ui;
				memcpy(&ui,buff508+1,4);
				if (ui!=uid) continue;
//					fprintf(stderr,"Received invalid ACK, saw:%u,uid:%u\n",ui,uid);
				goto doublebreak;
			}
		}
	}
}
doublebreak:
return 0;
error:
	return -1;
}

// works, draws a single pixel
int drawpixel(uint32_t uid, unsigned int x, unsigned int y,
		unsigned char red, unsigned char green, unsigned char blue,
		int rfd, unsigned short port, int wfd, uint32_t dipv4) {
unsigned char buff[3];
buff[0]=red;
buff[1]=green;
buff[2]=blue;
return drawonebox(uid,x,y,1,1,buff,rfd,port,wfd,dipv4);
}

// works, sends a pixel without waiting for an ack
int noack_drawpixel(unsigned int x, unsigned int y,
		unsigned char red, unsigned char green, unsigned char blue,
		int rfd, unsigned short port, int wfd, uint32_t dipv4) {
unsigned char buff508[508];
unsigned char *temp;
unsigned short us;

temp=buff508;
*temp=3; temp+=1;
us=htons(x); memcpy(temp,&us,2); temp+=2;
us=htons(y); memcpy(temp,&us,2); temp+=2;
*temp=red; temp+=1;
*temp=green; temp+=1;
*temp=blue; temp+=1;

if (8!=write(wfd,buff508,8)) GOTOERROR;
// usleep(1000);
return 0;
error:
	return -1;
}

// poor man's _MIN
#define _MIN(a,b) ((a<=b)?a:b)

static int drawbox(uint32_t *uid_inout,
		unsigned int left, unsigned int top, unsigned int width, unsigned int height,
		unsigned char *rgbs,
		int rfd, unsigned short port, int wfd, uint32_t dipv4) {
unsigned int i,width3,rl;
uint32_t uid;

uid=*uid_inout;
rl=left;
width3=width*3;
for (i=0;i<width;i+=12) {
	unsigned int bw3,j,rt,bw;
	bw=_MIN(12,width-i);
	bw3=bw*3;
	rt=top;
	for (j=0;j<height;j+=13) {
		unsigned int k,bh;
		unsigned char rgb468[468],*tempout,*tempin; // 12*13*3
		bh=_MIN(13,height-j);
		tempout=rgb468;
		tempin=rgbs+j*width3+i*3;
		for (k=0;k<bh;k++) {
			memcpy(tempout,tempin,bw3);
			tempout+=bw3;
			tempin+=width3;
		}
		if (drawonebox(uid,rl,rt,bw,bh,rgb468,rfd,port,wfd,dipv4)) GOTOERROR; uid+=1;
		rt+=13;
	}
	rl+=12;
}

*uid_inout=uid;
return 0;
error:
	return -1;
}

// drawx is unused but works
int drawx(uint32_t *uid_inout, int rfd, unsigned short port, int wfd, uint32_t dipv4,
		unsigned int width, unsigned int height) {
unsigned int i,n,n3;
unsigned char *rgbs=NULL;
unsigned char red[3]={255,0,0};
unsigned char green[3]={0,255,0};
unsigned char blue[3]={0,0,255};
n=_MIN(width,height);
n3=n*3;
if (!(rgbs=malloc(n*n*3))) GOTOERROR;
for (i=0;i<n;i++) {
	memcpy(rgbs+i*(n3+3),red,3); // 3*i+n3*i
	memcpy(rgbs+3*i+n3*(width-i),green,3);
	memcpy(rgbs+n3*i,blue,3);
	memcpy(rgbs+3*i,blue,3);
	memcpy(rgbs+3*(n-1)+n3*i,blue,3);
	memcpy(rgbs+3*i+n3*(n-1),blue,3);
}
if (drawbox(uid_inout,0,0,n,n,rgbs,rfd,port,wfd,dipv4)) GOTOERROR;

free(rgbs);
return 0;
error:
	if (rgbs) free(rgbs);
	return -1;
}

// fillbox is unused but works
int fillbox(uint32_t *uid_inout,
		unsigned int left, unsigned int top, unsigned int width, unsigned int height,
		unsigned char red, unsigned char green, unsigned char blue,
		int rfd, unsigned short port, int wfd, uint32_t dipv4) {
unsigned char *rgbs=NULL,*temp;
unsigned int wxh;
int i;
wxh=width*height;
if (!(rgbs=malloc(wxh*3))) GOTOERROR;
temp=rgbs;
for (i=0;i<wxh;i+=3) { *temp=red; temp++; *temp=green; temp++; *temp=blue; temp++; }
if (drawbox(uid_inout,left,top,width,height,rgbs,rfd,port,wfd,dipv4)) GOTOERROR;

free(rgbs);
return 0;
error:
	if (rgbs) free(rgbs);
	return -1;
}

static int drawfile(uint32_t *uid_inout, char *filename, int rfd, unsigned short port,
		int wfd, uint32_t dipv4) {
FILE *fin=NULL;
char oneline[16];
unsigned char *rgb=NULL;
unsigned int wxh3,width,height;
if (!memcmp(filename,"-",2)) fin=stdin;
else fin=fopen(filename,"rb");
if (!fin) GOTOERROR;
if (!fgets(oneline,16,fin)) GOTOERROR;
if (memcmp(oneline,"P6\n",3)) GOTOERROR;
if (!fgets(oneline,16,fin)) GOTOERROR;
{
	char *temp;
	width=atoi(oneline);
	temp=strchr(oneline,' ');
	if (!temp) GOTOERROR;
	temp++;
	height=atoi(temp);
}
if (!fgets(oneline,16,fin)) GOTOERROR;
if (memcmp(oneline,"255\n",4)) GOTOERROR;
wxh3=width*height*3;
if (!(rgb=malloc(wxh3))) GOTOERROR;
if (1!=fread(rgb,wxh3,1,fin)) GOTOERROR;

#if 0
{
	time_t t;
	t=time(NULL);
	switch (t&3) {
		case 0: if (drawbox(uid_inout,0,0,width,height,rgb,rfd,port,wfd,dipv4)) GOTOERROR; break;
		case 1: if (drawbox(uid_inout,1920-width,0,width,height,rgb,rfd,port,wfd,dipv4)) GOTOERROR; break;
		case 2: if (drawbox(uid_inout,0,1080-height,width,height,rgb,rfd,port,wfd,dipv4)) GOTOERROR; break;
		case 3: if (drawbox(uid_inout,1920-width,1080-height,width,height,rgb,rfd,port,wfd,dipv4)) GOTOERROR; break;
	}
}
#else
	if (drawbox(uid_inout,0,0,width,height,rgb,rfd,port,wfd,dipv4)) GOTOERROR;
#endif

fclose(fin);
free(rgb);
return 0;
error:
	if (fin) fclose(fin);
	if (rgb) free(rgb);
	return -1;
}

static int parseipport(unsigned char *ipv4_out, unsigned short *port_out, char *str) {
unsigned char ipv4[4];
unsigned short port;

ipv4[0]=atoi(str);
if (!(str=strchr(str,'.'))) GOTOERROR;
str++;
ipv4[1]=atoi(str);
if (!(str=strchr(str,'.'))) GOTOERROR;
str++;
ipv4[2]=atoi(str);
if (!(str=strchr(str,'.'))) GOTOERROR;
str++;
ipv4[3]=atoi(str);
if (!(str=strchr(str,':'))) GOTOERROR;
str++;
port=atoi(str);

memcpy(ipv4_out,ipv4,4);
*port_out=port;
return 0;
error:
	return -1;
}

static int runudp(uint32_t dipv4, unsigned short dport, char *cmd, char *filename) {
int rfd=-1;
int wfd=-1;
uint32_t uid=1;
unsigned short rfdport=8081;

if (initrfd(&rfd,&rfdport,rfdport)) GOTOERROR;
fprintf(stderr,"Listening on port %d\n",rfdport);
if (initwfd(&wfd,dipv4,dport)) GOTOERROR;
if (!strcmp(cmd,"query")) {
	unsigned int width,height;
	if (query_udp(&width,&height,rfd,rfdport,wfd,dipv4)) GOTOERROR;
	fprintf(stderr,"Query dimensions w:%u,h:%u\n",width,height);
} else if (!strcmp(cmd,"draw")) {
	if (drawfile(&uid,filename,rfd,rfdport,wfd,dipv4)) GOTOERROR;
} else {
	fprintf(stderr,"Unknown command: %s\n",cmd);
}

if (wfd!=-1) close(wfd);
if (rfd!=-1) close(rfd);
return 0;
error:
	if (wfd!=-1) close(wfd);
	if (rfd!=-1) close(rfd);
	return -1;
}

static int writen(int fd, unsigned char *b, unsigned int n) {
if (n) while (1) {
	int i;
	i=write(fd,b,n);
	if (i<=0) GOTOERROR;
	n-=i;
	if (!n) break;
	b+=i;
}
return 0;
error:
	return -1;
}
static int readn(int fd, unsigned char *b, unsigned int n) {
if (n) while (1) {
	int i;
	i=read(fd,b,n);
	if (i<=0) GOTOERROR;
	n-=i;
	if (!n) break;
	b+=i;
}
return 0;
error:
	return -1;
}

static int query_tcp(unsigned int *width_out, unsigned int *height_out,
		uint32_t ipv4, unsigned short port) {
int fd=-1;
struct sockaddr_in sa;
unsigned char buff[8];
unsigned int w=0,h=0;

clear_sockaddr_in(&sa);
sa.sin_family=AF_INET;
sa.sin_addr.s_addr=ipv4;
sa.sin_port=htons(port);
fd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
if (fd<0) GOTOERROR;
if (0>connect(fd,(struct sockaddr*)&sa,sizeof(sa))) GOTOERROR;
memset(buff,0,8);
if (writen(fd,buff,8)) GOTOERROR;
if (readn(fd,buff,5)) GOTOERROR;
if (buff[0]==1) {
	unsigned short n_short;
	memcpy(&n_short,buff+1,2);
	w=ntohs(n_short);
	memcpy(&n_short,buff+3,2);
	h=ntohs(n_short);
}

*width_out=w;
*height_out=h;

close(fd);
return 0;
error:
	if (fd!=-1) close(fd);
	return -1;
}

int bufferfile(unsigned char **data_out, unsigned int *len_out, char *filename) {
FILE *ff=NULL;
unsigned int len;
unsigned char *data=NULL;

ff=fopen(filename,"rb");
if (!ff) goto error;
if (fseek(ff,0,SEEK_END)) goto error;
len=ftell(ff);
(void)rewind(ff);
data=malloc(len+1);
if (!data) goto error;
data[len]='\0';
if (fread(data,len,1,ff)!=1) goto error;
fclose(ff);
*data_out=data;
*len_out=len;
return 0;
error:
	if (data) free(data);
	if (ff) fclose(ff);
	return -1;
}

static int bufferstream(unsigned char **data_out, unsigned int *datalen_out, FILE *fin) {
unsigned char *data=NULL;
unsigned int datalen=0,datamax=0;
while (1) {
	int k;
	if (datalen==datamax) {
		unsigned int m;
		m=datamax*2+4096;
		data=realloc(data,m);
		if (!data) GOTOERROR;
		datamax=m;
	}
	k=fread(data+datalen,1,datamax-datalen,fin);
	if (k<=0) {
		if (k<0) GOTOERROR;
		break;
	}
	datalen+=k;
}
*data_out=data;
*datalen_out=datalen;
return 0;
error:
	if (data) free(data);
	return -1;
}

static int draw_tcp(uint32_t ipv4, unsigned short port, char *filename,
		unsigned short left, unsigned short top) {
unsigned char *data=NULL;
unsigned int datalen;
int fd=-1;
struct sockaddr_in sa;
unsigned char buff[8];
unsigned short us;
uint32_t ui;
if (!memcmp(filename,"-",2)) {
	if (bufferstream(&data,&datalen,stdin)) GOTOERROR;
} else if (bufferfile(&data,&datalen,filename)) GOTOERROR;
us=htons(left);memcpy(buff,&us,2);
us=htons(top);memcpy(buff+2,&us,2);
ui=htonl(datalen);memcpy(buff+4,&ui,4);

clear_sockaddr_in(&sa);
sa.sin_family=AF_INET;
sa.sin_addr.s_addr=ipv4;
sa.sin_port=htons(port);
fd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
if (fd<0) GOTOERROR;
if (0>connect(fd,(struct sockaddr*)&sa,sizeof(sa))) GOTOERROR;
if (writen(fd,buff,8)) GOTOERROR;
fprintf(stderr,"Sent header\n");
if (writen(fd,data,datalen)) GOTOERROR;
fprintf(stderr,"Sent data, waiting for reply\n");
if (readn(fd,buff,2)) GOTOERROR;
fprintf(stderr,"Got reply\n");
if (!memcmp(buff,"OK",2)) {
	fprintf(stderr,"Received OK\n");
}

free(data);
close(fd);
return 0;
error:
	if (data) free(data);
	if (fd!=-1) close(fd);
	return -1;
}

static int runtcp(uint32_t ipv4, unsigned short port, char *cmd, char *filename) {
if (!strcmp(cmd,"query")) {
	unsigned int width,height;
	if (query_tcp(&width,&height,ipv4,port)) GOTOERROR;
	fprintf(stderr,"Query dimensions w:%u,h:%u\n",width,height);
} else if (!strcmp(cmd,"draw")) {
	if (draw_tcp(ipv4,port,filename,0,0)) GOTOERROR;
} else if (!memcmp(cmd,"draw+",5)) {
	unsigned short left,top;
	char *t;
	left=atoi(cmd+5);
	t=strchr(cmd+5,'+');
	if (!t) top=0;
	else top=atoi(t+1);
	if (draw_tcp(ipv4,port,filename,left,top)) GOTOERROR;
} else if (!strcmp(cmd,"draw%")) {
	unsigned short left=0,top=0;
	time_t t=time(NULL);
	switch (t&3) {
		case 0: left=65535; break;
		case 1: top=65535; break;
		case 2: left=top=65535; break;
	}
	if (draw_tcp(ipv4,port,filename,left,top)) GOTOERROR;
} else {
	fprintf(stderr,"Unknown command: %s\n",cmd);
}
return 0;
error:
	return -1;
}

int main(int argc, char **argv) {
unsigned char c_dipv4[4];
uint32_t dipv4;
unsigned short dport;
char *filename="";
char *cmd;
int isudp=0;

if ((argc!=5)&&(argc!=4)) {
	fprintf(stdout,"Usage: %s (udp|tcp) (ip):(port) draw[+top+left] (filename)\n",(argc)?argv[0]:"udpdisplay");
	fprintf(stdout,"Usage: %s (udp|tcp) (ip):(port) query\n",(argc)?argv[0]:"udpdisplay");
	return 0;
}

if (!strcmp(argv[1],"udp")) isudp=1;
if (parseipport(c_dipv4,&dport,argv[2])) GOTOERROR;
memcpy(&dipv4,c_dipv4,4);
cmd=argv[3];
if (argc==5) filename=argv[4];

if (isudp) {
	if (runudp(dipv4,dport,cmd,filename)) GOTOERROR;
} else {
	if (runtcp(dipv4,dport,cmd,filename)) GOTOERROR;
}
return 0;
error:
	return -1;
}
