#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include "md5.h"
#include "read.h"
void readconf();
void init1();
int main();
extern char username1[30];
extern char password1[30];
extern char server_ip[30];
extern int server_port;
// 必须修改，帐号密码和 mac 地址是绑定的
char user[30];
char pass[30];
unsigned char hostip[4]= {10,207,56,21};
unsigned char dhcpip[4]= {10,207,56,254};
uint64_t mac = 0x207693300da5; // echo 0x`ifconfig eth | egrep -io "([0-9a-f]{2}:){5}[0-9a-f]{2}" | tr -d ":"`
char keep_alive_version[2] = {0xdc,0x02};
char auth_version[2] = {0x0a,0x00};
char ipdog[1]= {0x01};
char ccs[1]= {0x20};
char adpnum[1]= {0x04};
int reconnect=1;//重连
int restart=1800;//1800 restart
// 不一定要修改
char host[] = "123";
char os[] = "Windows 10";
int user_len;
int pass_len;
int host_len;
int os_len;
// TODO 增加从文件读取参数

char SERVER_ADDR[30];
int SERVER_PORT;

#define RECV_DATA_SIZE 1000
#define SEND_DATA_SIZE 1000
#define CHALLENGE_TRY 10
#define LOGIN_TRY 5
#define ALIVE_TRY 5
#define INTERFACE 1

/* infomation */
struct user_info_pkt
{
    char *username;
    char *password;
    char *hostname;
    char *os_name;
    uint64_t mac_addr;                 //unsigned long int
    int username_len;
    int password_len;
    int hostname_len;
    int os_name_len;
};

/* signal process flag */
int logout_flag = 0;
void init1()//初始化
{
    strcpy(pass,password1);
	strcpy(user,username1);
    pass_len=strlen(pass);
    user_len=strlen(user);
    host_len=strlen(host);
    os_len=strlen(os);
	strncpy(SERVER_ADDR,server_ip,strlen(server_ip));
	SERVER_PORT=server_port;
}
void de(unsigned char *data,int offset,int len)//
{

    int i=0;
    printf("\n");
    {
        for (i=0; i<len; i++)

            printf("%02x ",data[offset+i]);
        if(i/16==0)
            printf("\n");
    }
}

void get_user_info(struct user_info_pkt *user_info)
{
    user_info->username = user;
    user_info->username_len = user_len;
    user_info->password = pass;
    user_info->password_len = pass_len;
    user_info->hostname = host;
    user_info->hostname_len = host_len;
    user_info->os_name = os;
    user_info->os_name_len = os_len;
    user_info->mac_addr = mac;
}

void set_challenge_data(unsigned char *clg_data, int clg_data_len, int clg_try_count)
{
    /* set challenge */
    int random = rand() % 0xF0 + 0xF;  //这里有点小问题
    int data_index = 0;
    memset(clg_data, 0x00, clg_data_len);
    /* 0x01 challenge request */
    clg_data[data_index++] = 0x01;
    /* clg_try_count first 0x02, then increment */
    clg_data[data_index++] = 0x02 + (unsigned char)clg_try_count;                                                          //python里面这一段是只用02
    /* two byte of challenge_data */
    clg_data[data_index++] = (unsigned char)(random % 0xFFFF);
    clg_data[data_index++] = (unsigned char)((random % 0xFFFF) >> 8);
    /* end with 0x09 */
    clg_data[data_index++] = 0x09;                                                                                          //后面自动空15位，共20位
    if(INTERFACE == 1)
    {
        printf("[DEBUG]challenge_packet_len=%d",clg_data_len);
        printf("\n[DEBUG]challenge packet is:");
        de(clg_data,0,clg_data_len);
        printf("\n");
    }
}

void challenge(int sock, struct sockaddr_in serv_addr, unsigned char *clg_data, int clg_data_len, char *recv_data, int recv_len)
{
    long ret;                                                                                                                //定义整形变量ret
    int challenge_try = 0;                                                                                                  //定义challenge尝试次数为0
    do
    {
        if (challenge_try > CHALLENGE_TRY)
        {
            close(sock);
            fprintf(stderr, "[drcom-challenge]: try challenge, but failed, please check your network connection.\n");
            exit(EXIT_FAILURE);
        }                                                                                                                   //当challenge尝试超过10次预设值时，释放套接口描述字s，报错
        set_challenge_data(clg_data, clg_data_len, challenge_try);                                                          //生成challenge_dat数据
        if(INTERFACE ==1)
            printf("[debug]challenge packet make !\n");
        fprintf(stderr,"%s",clg_data);
        printf("%s",clg_data);
        challenge_try++;                                                                                                    //尝试次数自加
        ret = sendto(sock, clg_data, clg_data_len, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));                    //将字符串发送给server,,,ret=sendto(已建立的连接，clg包，clg包长度，flags设0不变，sockaddr结构体，前者长度)
        if (ret != clg_data_len)                                                                                            //ret不等于clg_data长度报错
        {
            fprintf(stderr, "[drcom-challenge]: send challenge data failed.\n");
            continue;
        }
        ret = recvfrom(sock, recv_data, recv_len, 0, NULL, NULL);                                                           //ret=server返回的字符串
        if (ret < 0)                                                                                                        //返回值小于0报错
        {
            fprintf(stderr, "[drcom-challenge]: recieve data from server failed.\n");
            continue;
        }
        if (*recv_data != 0x02)                                                                                             //receive_data第一个字符不为02，但为07报错退出，否则继续循环
        {
            if (*recv_data == 0x07)
            {
                close(sock);
                fprintf(stderr, "[drcom-challenge]: wrong challenge data.\n");
                exit(EXIT_FAILURE);
            }
            fprintf(stderr, "[drcom-challenge]: challenge failed!, try again.\n");
        }
    }
    while ((*recv_data != 0x02));                                                                                            //receive_data第一个字符为02时，退出循环。
    fprintf(stdout, "[drcom-challenge]: challenge success!\n");
}

void set_login_data(struct user_info_pkt *user_info, unsigned char *login_data, int login_data_len, unsigned char *salt, int salt_len)      //这句里定义了salt是challenge返回值4-8位
{
    /* login data */
    int i, j;
    static unsigned char md5_str[16];                                 //定义无符号字符串 md5_str[16]
    unsigned char md5_str_tmp[1000];                           //定义无符号字符串 md5_str_tmp[1000]
    int md5_str_len;

    int data_index = 0;                                       //data_index初始化

    memset(login_data, 0x00, login_data_len);

    /* magic 3 byte, username_len 1 byte 4位 */
    login_data[data_index++] = 0x03;
    login_data[data_index++] = 0x01;
    login_data[data_index++] = 0x00;
    login_data[data_index++] = (unsigned char)(user_info->username_len + 20);    //ASCII值转字符串（字符串长度（账号）+20）

    /* md5 0x03 0x01 salt password 16位*/
    md5_str_len = 2 + salt_len + user_info->password_len;                        //MD5_1   md5_str长度为2+4+密码长度
    memset(md5_str_tmp, 0x00, md5_str_len);                                      //将md5_str_tmp的前md5_str_len位全部置0x00
    md5_str_tmp[0] = 0x03;
    md5_str_tmp[1] = 0x01;
    memcpy(md5_str_tmp +2, salt, salt_len);                                      //++salt
    memcpy(md5_str_tmp + 2 + salt_len, user_info->password, user_info->password_len);//++password
    MD5(md5_str_tmp, md5_str_len, md5_str);
    memcpy(login_data + data_index, md5_str, 16);                                    //向data_index中添加16位的md5_str
    data_index += 16;


    /* user name 36位 */
    memcpy(login_data + data_index, user_info->username, user_info->username_len);
    data_index += 36;       //用户名ASCII码先填入logindata+data_index偏移量位置，然后强艹data_index为36

    /* ccs ada 2位*/
    memcpy(login_data+data_index,ccs,1);
    data_index += 1;                                                //CONTROLCHECKSTATUS
    memcpy(login_data+data_index,adpnum,1);
    data_index += 1;                                                   //ADAPTERNUM

    /* (data[4:10].encode('hex'),16)^mac */
    uint64_t sum = 0;
    for (i = 0; i < 6; i++)
    {
        sum = (int)md5_str[i] + sum * 256;                       //把前面的md5-1值的前6位十六进制表示
    }
    sum ^= user_info->mac_addr;                                                      //把md5-1的前6位和mac按位异或再赋值给sum
    for (i = 6; i > 0; i--)
    {
        login_data[data_index + i - 1] = (unsigned char)(sum % 256);                 //把sum逐位加回login_data中，共6位
        sum /= 256;
    }
    data_index += 6;

    /* md5 0x01 pwd salt 0x00 0x00 0x00 0x00 */
    md5_str_len = 1 + user_info->password_len + salt_len + 4;                        //MD5_2    长度为1+密码长度+salt长度+4个零位
    memset(md5_str_tmp, 0x00, md5_str_len);                                          //MD5temp区前md5_str_len清零
    md5_str_tmp[0] = 0x01;                                                           //第一位01
    memcpy(md5_str_tmp + 1, user_info->password, user_info->password_len);           //拷贝密码
    memcpy(md5_str_tmp + 1 + user_info->password_len, salt, salt_len);               //拷贝salt
    MD5(md5_str_tmp, md5_str_len, md5_str);                                          //计算MD5_2,赋值到md5_str
    memcpy(login_data + data_index, md5_str, 16);                                    //拷贝16位的MD5_2到logindata+偏移量
    data_index += 16;                                                                //偏移量+16

    /* 0x01 hexip 0x00*12 */
    login_data[data_index++] = 0x01;
    memcpy(login_data+data_index,hostip,4);
    data_index += 16;
    //printf("len:%d",data_index);
    //de(login_data,0,data_index);

    /* md5 login_data[0-data_index] 0x14 0x00 0x07 0x0b 8 bytes */
    md5_str_len = data_index + 4; //MD5_3    长度为偏移量+4
    //printf("\nmd5_str_len:%d",md5_str_len);
    memset(md5_str_tmp, 0x00, md5_str_len); //md5_str_tmp清空
    memcpy(md5_str_tmp, login_data, data_index);//把前面logindata全部内容拷贝到md5_str_tmp
    //de(md5_str_tmp,0,md5_str_len);
    md5_str_tmp[data_index+0] = 0x14;
    md5_str_tmp[data_index+1] = 0x00;
    md5_str_tmp[data_index+2] = 0x07;
    md5_str_tmp[data_index+3] = 0x0b;
    //de(md5_str_tmp,0,101);
    MD5(md5_str_tmp, md5_str_len, md5_str);
    memcpy(login_data + data_index, md5_str, 8);                                     //MD5_3只拷贝前8位
    data_index += 8;//偏移量+8
    /* ipdog 0x00*4 */
    memcpy(login_data+data_index,ipdog,1);
    data_index += 1;

    data_index += 4;

    /* hostname */
    i = user_info->hostname_len > 32 ? 32 : user_info->hostname_len;                  //这一段拷贝Hostname,用i表示长度，偏移量+32
    memcpy(login_data + data_index, user_info->hostname, i);
    data_index += 32;

    /* primary dns: 114.114.114.114 */
    login_data[data_index++] = 0x72;
    login_data[data_index++] = 0x72;
    login_data[data_index++] = 0x72;
    login_data[data_index++] = 0x72;

    /* dhcp */
    memcpy(login_data+data_index,dhcpip,4);
    data_index += 4;

    /* second dns */
    login_data[data_index++] = 0x08;
    login_data[data_index++] = 0x08;
    login_data[data_index++] = 0x08;
    login_data[data_index++] = 0x08;

    /*乱七八糟并不懂 */
    data_index += 8;
    login_data[data_index++] = 0x94;
    data_index += 3;
    login_data[data_index++] = 0x05;
    data_index += 3;
    login_data[data_index++] = 0x01;
    data_index += 3;
    login_data[data_index++] = 0x28;
    login_data[data_index++] = 0x0a;
    data_index += 2;
    login_data[data_index++] = 0x02;
    data_index += 3;

    /* host_os.ljust(32,'\x00') 128 */
    i = user_info->os_name_len > 32 ? 32 : user_info->os_name_len;
    memcpy(login_data + data_index, user_info->os_name, i);
    data_index += 32;
    data_index += 96;

    /* auth version */

    memcpy(login_data+data_index,auth_version,2);
    data_index += 2;

    /* 0x02 0x0c */
    login_data[data_index++] = 0x02;
    login_data[data_index++] = 0x0c;

    /* checksum point 4*/
    int check_point = data_index;                           //定义check_point为偏移量，方便一会儿覆盖数据，然后给logindata加'\x01\x26\x07\x11\x00\x00'+dump(mac)
    login_data[data_index++] = 0x01;
    login_data[data_index++] = 0x26;
    login_data[data_index++] = 0x07;
    login_data[data_index++] = 0x11;
    login_data[data_index++] = 0x00;
    login_data[data_index++] = 0x00;
    uint64_t mac = user_info->mac_addr;
    for (i = 0; i < 6; i++)
    {
        login_data[data_index + 5 - i] = (unsigned char)(mac % 256);
        mac /= 256;
    }
    data_index += 6;

    sum = 1234;
    uint64_t check = 0;                                                        //定义check为0
    for (i = 0; i < data_index; i += 4)
    {
        check = 0;
        for (j = 0; j < 4; j++)
        {
            check = check * 256 + (int)login_data[i + j];

        }
        sum ^= check;

    }
    sum = (1968 * sum) & 0xFFFFFFFF;
    for (j = 0; j < 4; j++)
    {
        login_data[check_point + j] = (unsigned char)(sum >> (j * 8) & 0x000000FF); //把4位checksum结果写入check_point开始的logindata处覆盖数据
    }
    /* 0x00 0x00 0x00 0x00 */
    login_data[data_index++] = 0x00;
    login_data[data_index++] = 0x00;
    login_data[data_index++] = 0xc2;                   //不造
    login_data[data_index++] = 0x66;                   //不造，反正这两个数窝们学校能登陆
    if(INTERFACE == 1)
    {
        printf("[DEBUG]login data len=%d\n",data_index);
        printf("[DEBUG]login packet is:");
        de(login_data,0,data_index);
    }

}


void login1(int sock, struct sockaddr_in serv_addr, unsigned char *login_data, int login_data_len, char *recv_data, int recv_len)
{
    /* login */
    long ret = 0;
    int login_try = 0;
    do
    {
        if (login_try > LOGIN_TRY)
        {
            close(sock);
            fprintf(stderr, "[drcom-login]: try login, but failed, something wrong.\n");                               //尝试次数过多报错
            exit(EXIT_FAILURE);
        }
        login_try++;
        ret = sendto(sock, login_data, login_data_len, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));          //ret=发送到服务器的数据长度
        if (ret != login_data_len)
        {
            fprintf(stderr, "[drcom-login]: send login data failed.\n");                                              //ret长度和刚才生成长度不符合报错 ，继续循环，try自加
            continue;
        }
        ret = recvfrom(sock, recv_data, recv_len, 0, NULL, NULL);                                                     //ret=从服务器接收的数据
        if (ret < 0)
        {
            fprintf(stderr, "[drcom-login]: recieve data from server failed.\n");                                     //ret小于0则接收数据失败报错，继续循环，try自加
            continue;
        }
        if (*recv_data != 0x04)                                                                                       //如果接收数据首字符不为04，且为05，报错提示账号或密码错误
        {
            if (*recv_data == 0x05)
            {
                close(sock);
                fprintf(stderr, "[drcom-login]: wrong password or username!\n\n");
                exit(EXIT_FAILURE);
            }
            fprintf(stderr, "[drcom-login]: login failed!, try again\n");                                             //都触发登陆失败提示。。
        }
    }
    while ((*recv_data != 0x04));
    fprintf(stdout, "[drcom-login]: login success!\n");                                                               //只有当首字符为04提示登陆成功
}

void set_alive1_data(unsigned char *alive_data1, int alive_data1_len, unsigned char *tail, unsigned char *salt,int salt_len,struct user_info_pkt *user_info,
                     unsigned char *md5_str)
//set_alive_data1(send_data, alive1_data_len, package_tail, globle_salt,4,&user_info);
{
    memset(alive_data1, 0x00, alive_data1_len);
    int i = 0;

    time_t *timep=malloc(sizeof(uint16_t));
    *timep=time(NULL);
    if(INTERFACE == 1)
        printf("\n[DEBUG]timep=%ld\n",*timep);
    memset(alive_data1, 0x00, alive_data1_len);
    alive_data1[i++] = 0xff;

    /*

    int md5_str_len,j=0;
    memset(md5_str,0x00,16);
    unsigned char md5_str_tmp[1000];
    md5_str_len = 2 + salt_len + user_info->password_len;
    memset(md5_str_tmp, 0x00, md5_str_len);
    md5_str_tmp[j++] = 0x03;
    md5_str_tmp[j++] = 0x01;
    memcpy(md5_str_tmp + j, salt,salt_len);
    memcpy(md5_str_tmp + j + salt_len, user_info->password, user_info->password_len);
    MD5(md5_str_tmp, md5_str_len, md5_str);
     */
    memcpy(alive_data1 + i, md5_str, 16);                       //应该是十六位，暂无测试环境
    i += 19;
    memcpy(alive_data1 + i,tail,16);
    i += 16;

    memcpy(alive_data1 + i,timep,2);
    i+=6;


    if(INTERFACE ==1)
    {
        printf("[DEBUG]alive_data1(len=%d):",i);
        de(alive_data1,0,i);
        printf("\n");
    }
}

void set_alive2_data(unsigned char *alive_data2, int alive_data2_len, unsigned char *tail, int tail_len, int alive_count,int type,unsigned char *kl1_recv_data,int first)
//(send_data, alive_data_len, tail, tail_len, alive_count, random,type)
{
    // 0: 84 | 1: 82 | 2: 82
    int i = 0;
    memset(alive_data2, 0x00, alive_data2_len);
    alive_data2[i++] = 0x07;

    alive_data2[i++] = (unsigned char)alive_count;

    alive_data2[i++] = 0x28;
    alive_data2[i++] = 0x00;
    alive_data2[i++] = 0x0b;
    alive_data2[i++] = (unsigned char)type;

    if(first == 1)
    {
        alive_data2[i++] = 0x0f;
        alive_data2[i++] = 0x27;
    }
    else
    {
        memcpy(alive_data2+i,keep_alive_version,2);  //keep_alive_version
        i += 2;
    }
    alive_data2[i++] = 0x2f;
    alive_data2[i++] = 0x12;
    i += 6;
    memcpy(alive_data2+i,tail,tail_len);
    i += 12;
    if(type == 3)
    {
        memcpy(alive_data2+i,hostip,4);
        i += 12;
    }
    else
        i += 12;   //!!!!!!!!!type=1


    if(INTERFACE == 1)
    {
        printf("\n[DEBUG]kl2_len=%d\n",i);
        printf("[DEBUG]kl2 packet is:");
        de(alive_data2,0,i);
    }

}



void set_logout_data(unsigned char *logout_data, int logout_data_len)
{
    memset(logout_data, 0x00, logout_data_len);
    // TODO
}

void logout1(int sock, struct sockaddr_in serv_addr, unsigned char *logout_data, int logout_data_len, char *recv_data, int recv_len)
{
    set_logout_data(logout_data, logout_data_len);
    // TODO
    close(sock);
    if(reconnect==1)
    {
        main();
    }
    else
    {
        exit(EXIT_SUCCESS);
    }
}

void logout_signal(int signum)
{
    logout_flag = 1;
}

int main()
{
	readconf();
	init1();
    printf("DR.COM TEST!\n");
    int timeout1=0;
    int i=0;
    int t=0;
    int rec=-1;
int rec2=-1;
int rec3=-1;
    int r=0;
    //FILE *stderr = fopen("test.txt", "wb");
    int sock,alive_count=0;//定义整形变量sock和ret
    //long ret;
    unsigned char send_data[SEND_DATA_SIZE];                   //定义无符号字符串send_data[1000]  长度为1000
    char recv_data[RECV_DATA_SIZE];                            //定义无符号字符串recv_data[1000]  长度为1000
    unsigned char globle_salt[4];
    unsigned char package_tail[16];
    unsigned char package_tail_empty[4];
    unsigned char package_tail2[4];
    unsigned char globle_md5a[16];
    struct sockaddr_in serv_addr,local_addr;                              //定义结构体sockaddr_in        serv_addr
    struct user_info_pkt user_info;                            //定义结构体user_info_pkt      user_info

    sock = socket(AF_INET, SOCK_DGRAM, 0);                     //AF_INET决定了要用ipv4地址（32位的）与端口号（16位的）的组合，。数据报式Socket（SOCK_DGRAM）是一种无连接的Socket，对应于无连接的UDP服务应用
    if (sock < 0)                                              //sock<0即错误
    {
        fprintf(stderr, "[drcom]: create sock failed.\n");
        exit(EXIT_FAILURE);
    }
    serv_addr.sin_family = AF_INET;                           //这三句填写sockaddr_in结构
    serv_addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);       //将服务器IP转换成一个长整数型数
    serv_addr.sin_port = htons(SERVER_PORT);                  //将端口高低位互换
    local_addr.sin_family = AF_INET;                           //这三句填写sockaddr_in结构
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);       //将服务器IP转换成一个长整数型数
    local_addr.sin_port = htons(SERVER_PORT);                  //将端口高低位互换
    bind(sock,(struct sockaddr *)&(local_addr),sizeof(struct sockaddr_in));
    get_user_info(&user_info);                               //为结构体user_info_pkt赋值，包括用户名密码系统版本等及其长度

    // challenge data length 20

    challenge(sock, serv_addr, send_data, 20, recv_data, RECV_DATA_SIZE);                  //challenge,然后返回值4-8位给salt赋值
    memcpy(globle_salt, recv_data + 4, 4);                                                //备份一个坑B SALT,一会要用
    // login data length 319, salt length 4
    if(INTERFACE == 1)
    {
        printf("\n[DEBUG]globle_salt=");
        de(globle_salt,0,4);
    }

    printf("------------------------------\n");
    set_login_data(&user_info, send_data, 330, (unsigned char *)(recv_data + 4), 4);       //貌似是319位数据。。。
    memcpy(globle_md5a,send_data+4,16);
    if(INTERFACE == 1)
    {
        printf("\n[DEBUG]globle_md5a=");
        de(globle_md5a,0,16);
    }
    memset(recv_data, 0x00, RECV_DATA_SIZE);                                               //把challenge的recv_data清零
    login1(sock, serv_addr, send_data, 330, recv_data, RECV_DATA_SIZE);                     //登陆
    memcpy(package_tail, recv_data + 23, 16);
    //memset(package_tail,0xff,16);
    if(INTERFACE == 1)
    {
        printf("\n[DEBUG]package_tail=");
        de(package_tail,0,16);
    }
    printf("------------------------------\n");
    //keep alive 1 data length  36
    int alive1_data_len=38;//长度不太确定
    //int mtype=0;                                              //定义整形变量mtype=0
    //keep_alive_1((unsigned char *)&send_data,(unsigned char *)&recv_data,alive1_data_len,(unsigned char *)&package_tail,(unsigned char *)&globle_salt,&user_info,&serv_addr,globle_md5a,sock,mtype);
    do
    {
        set_alive1_data(send_data, alive1_data_len, package_tail, globle_salt,4,&user_info,globle_md5a);
        memset(recv_data, 0x00, RECV_DATA_SIZE);
        sendto(sock, send_data, 38, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

keepaliverecv:
        recvfrom(sock, recv_data, RECV_DATA_SIZE, 0, NULL, NULL);
        if(INTERFACE == 1)
        {
            printf("[DEBUG]keep_alive_1_recv=");
            de((unsigned char *)recv_data,0,64);
            printf("\n");
        }


        int mtype=0;                                              //定义整形变量mtype=0
        if(*recv_data==0x4d)                                      //如果ka1_recv第一个字符为4d，接收到需要跳过的信息
        {
            printf("Server sent a message,ignore it\n");
            goto keepaliverecv;                                   //继续跳转到上方接受一个数据包
        }

        else if(*recv_data==0x07)                                 //如果接受数据首字符为07，显示ka1成功
        {
            printf("[keep－alive] keep alive 1 success\n");

            while(recv_data[5]<4)
            {

                if(recv_data[2]==0x10&&recv_data[3]==0x00)//rok:1000
                {
                    if(INTERFACE)
                        printf("[keep-alive] rok received\n");
                    mtype=1;
                }
                if(recv_data[2]==0x10&&recv_data[3]==0x01)//file:1001
                {
                    if(INTERFACE)
                        printf("[keep-alive] flie received(droped)\n");
                    mtype=1;
                }
                if(recv_data[2]==0x28&&recv_data[3]==0x00)//common:2800
                {
                    if(INTERFACE)
                        printf("[keep-alive] common received\n");
                    mtype=recv_data[5]+1;
                }
                int alive2_data_len =40;

                memset(package_tail_empty,0x00,4);
                //01
                memset(send_data,0x00,40);
                set_alive2_data(send_data, alive2_data_len,package_tail_empty, 4, alive_count,1,(unsigned char *)recv_data,1);
                //(unsigned char *alive_data2, int alive_data2_len, unsigned char *tail, int tail_len, int alive_count,int type,unsigned char *kl1_recv_data,int first)
                memset(recv_data, 0x00, RECV_DATA_SIZE);
                sendto(sock, (char*)&send_data,alive2_data_len, 0, (struct sockaddr *)&serv_addr,sizeof(struct sockaddr));
                printf("[keep-alive2]send1");
                recvfrom(sock, recv_data, RECV_DATA_SIZE, 0, NULL, NULL);
                printf("[keep-alive2]recv1");
                do
                {
                    if(recv_data[0] ==0x07 && recv_data[2] == 0x28)
                        break;
                    else if(recv_data[2] == 0x10 && recv_data[0] == 0x07)
                    {
                        printf("[keep-alive2] recv file, resending..");
                        alive_count += 1;
                        //set_alive2_data(send_data, alive2_data_len, package_tail_empty, 4, alive_count,1,(unsigned char *)recv_data,1);
                        //sendto(sock, (char*)&send_data,alive2_data_len, 0, (struct sockaddr *)&serv_addr,sizeof(struct sockaddr));
                        break;
                    }
                    else
                        printf("[keep-alive2] recv1/unexpected");
                    set_alive2_data(send_data, alive2_data_len, package_tail_empty, 4, alive_count,1,(unsigned char *)recv_data,1);
                    sendto(sock, (char*)&send_data,alive2_data_len, 0, (struct sockaddr *)&serv_addr,sizeof(struct sockaddr));
                }
                while(1);
                if(INTERFACE ==1)
                {
                    printf("\n[DEBUG]kl2_recv1=");
                    de((unsigned char *)recv_data,0,40);
                }
                //02
                set_alive2_data(send_data, alive2_data_len, package_tail_empty, 4, alive_count,1,(unsigned char *)recv_data,0);
                memset(recv_data, 0x00, RECV_DATA_SIZE);
                sendto(sock, (char*)&send_data,alive2_data_len, 0, (struct sockaddr *)&serv_addr,sizeof(struct sockaddr));
                printf("[keep-alive2]send2");

                do
                {
                    recvfrom(sock, recv_data, RECV_DATA_SIZE, 0, NULL, NULL);

                    if(*recv_data == 0x07)
                    {
                        alive_count +=1;
                        break;
                    }
                    else
                        printf("[keep-alive2]send2/unexpected");
                }
                while(1);
                if(INTERFACE ==1)
                {
                    printf("\n[DEBUG]kl2_recv2=");
                    de((unsigned char *)recv_data,0,40);
                }
                printf("\n[keep-alive2]recv2");
                memcpy(package_tail2,recv_data+16,4);
                printf("\n[DEBUG]package_tail2=");
                de(package_tail2,0,4);
                printf("\n");
                //03
                set_alive2_data(send_data, alive2_data_len, package_tail2, 4, alive_count,3,(unsigned char *)recv_data,0);
                sendto(sock, (char*)&send_data,alive2_data_len, 0, (struct sockaddr *)&serv_addr,sizeof(struct sockaddr));
                printf("\n[keep-alive2]send3");
                memset(recv_data, 0x00, RECV_DATA_SIZE);
                do
                {
                    recvfrom(sock, recv_data, RECV_DATA_SIZE, 0, NULL, NULL);
                    if(*recv_data == 0x07)
                    {
                        alive_count +=1;
                        break;
                    }
                    else
                        printf("[keep-alive2]send2/unexpected");
                }
                while(1);
                if(INTERFACE ==1)
                {
                    printf("\n[DEBUG]kl2_recv3=");
                    de((unsigned char *)recv_data,0,40);
                }
                printf("\n[keep-alive2]recv3");
                memcpy(package_tail2,recv_data+16,4);
                printf("\n[DEBUG]package_tail2=");
                de(package_tail2,0,4);
                printf("\n");
                printf("\n[keep-alive2] keep-alive2 loop was in daemon.");
                uint8_t alive_count2 = alive_count;
                //socket timeout
                struct timeval timeout= {3,0}; //3s
                setsockopt(sock,SOL_SOCKET,SO_SNDTIMEO,(const char*)&timeout,sizeof(timeout));
                setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,(const char*)&timeout,sizeof(timeout));
                while(1)
                {
                    time_t t;
                    char *s;
                    t=time(&t);
                    s=ctime(&t);
                    t++;

                    sleep(18);
                    printf("\n[DEBUG]LOOPING:%s",s);
                    set_alive1_data(send_data, alive1_data_len, package_tail, globle_salt,4,&user_info,globle_md5a);
                    memset(recv_data, 0x00, RECV_DATA_SIZE);
                    sendto(sock, send_data, 38, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
                    printf("\n[keep-alive1]send");
                    rec=recvfrom(sock, recv_data, RECV_DATA_SIZE, 0, NULL, NULL);
                    printf("\n[keep-alive1]receive\n ");
                    de((unsigned char *)recv_data,0,40);
                    set_alive2_data(send_data, alive2_data_len, package_tail2, 4, alive_count2,1,(unsigned char *)recv_data,0);
                    sendto(sock, (char*)&send_data,alive2_data_len, 0, (struct sockaddr *)&serv_addr,sizeof(struct sockaddr));
                    printf("\n[keep-alive2]send1");
                    rec2=recvfrom(sock, recv_data, RECV_DATA_SIZE, 0, NULL, NULL);
                    printf("\n[keep-alive2]receive1 ");
                    de((unsigned char *)recv_data,0,40);
                    memcpy(package_tail2,recv_data+16,4);
                    set_alive2_data(send_data, alive2_data_len, package_tail2, 4, alive_count2+1,3,(unsigned char *)recv_data,0);
                    sendto(sock, (char*)&send_data,alive2_data_len, 0, (struct sockaddr *)&serv_addr,sizeof(struct sockaddr));
                    printf("\n[keep-alive2]send2");
                    rec3=recvfrom(sock, recv_data, RECV_DATA_SIZE, 0, NULL, NULL);
                    if (rec<=0||rec2<=0||rec3<=0)//掉线检测
                    {
                        timeout1++;
                        printf("\ntimeout:%d\n",timeout1);

                        if(timeout1==5)
                        {
                            printf("\ntimeout!!!!\n");
                            system("$(cd `dirname $0`; pwd)/opdrc.sh &");
                            exit(1);
                            //logout_flag = 1;
                            //break;
                        }
                    }
                    else
                    {
                        timeout1=0;
                    }


                    printf("\n[keep-alive2]receive2 ");
                    de((unsigned char *)recv_data,0,40);
                    memcpy(package_tail2,recv_data+16,4);
                    alive_count2 +=2;
                    if(restart>0)
                    {
                        r++;
                        if(r==restart)
                        {
                            system("$(cd `dirname $0`; pwd)/opdrc.sh &");
                            exit(1);
                        }
                    }
                }
            }







        }

        sleep(5);
    }
    while (logout_flag != 1);
// logout, data_length 80 or ?
    memset(recv_data, 0x00, RECV_DATA_SIZE);
    printf("logout");
    logout1(sock, serv_addr, send_data, 80, recv_data, RECV_DATA_SIZE);
    return 0;
}



