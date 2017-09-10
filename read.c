#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "read.h"
char username[]="";
char password[]="";
char server_ip[]="";
int server_port;
/*   删除左边的空格   */
char * del_left_trim(char *str) {
    assert(str != NULL);
    for (;*str != '\0' && isblank(*str) ; ++str);
    return str;
}
/*   删除两边的空格   */
char * del_both_trim(char * str) {
    char *p;
    char * szOutput;
    szOutput = del_left_trim(str);
    for (p = szOutput + strlen(szOutput) - 1; p >= szOutput && isblank(*p);
            --p);
    *(++p) = '\0';
    return szOutput;
}
/*主函数*/
void readconf() {
    FILE * fp = NULL;
   /*打开配置文件a.txt*/
    fp = fopen("drcom.conf", "r");
   /*緩沖區*/
    char buf[64];
    char s[64];
    /*分割符*/
    char * delim = "=";
    char * p;
    char ch;
    char str[]="";
    char usr[]="";
    char psd[]="";
    char s_ip[]="";
    char s_port[]="";
     while (!feof(fp)) {
        if ((p = fgets(buf, sizeof(buf), fp)) != NULL) {
            strcpy(s, p);
            ch=del_left_trim(s)[0];
           /*判断注释 空行，如果是就直接下次循环*/
            if (ch == '#' || isblank(ch) || ch=='\n')
                continue;
          /*分割字符串*/
            p=strtok(s, delim);
            if(p){
            	strcpy(str,p);
if(strcmp(str,"username")==0)
{
            	printf("usernmae:");
while ((p = strtok(NULL, delim)))
strcpy(usr,del_both_trim(p));
printf("%s", usr);
usr[strlen(usr)-1]='0';
strcpy(username,usr);
}else if(strcmp(str,"password")==0)
{
	           	printf("password:");
while ((p = strtok(NULL, delim)))
strcpy(psd,del_both_trim(p));
printf("%s", psd);
psd[strlen(psd)-1]='\0';
strcpy(password,psd);
}else if(strcmp(str,"server_ip")==0)
{
	           	printf("server_ip:");
while ((p = strtok(NULL, delim)))
strcpy(s_ip,del_both_trim(p));
printf("%s", s_ip);
s_ip[strlen(s_ip)-1]='\0';
strcpy(server_ip,s_ip);
}else if(strcmp(str,"server_port")==0)
{
	           	printf("server_port:");
while ((p = strtok(NULL, delim)))
strcpy(s_port,del_both_trim(p));
printf("%s", s_port);
s_port[strlen(s_port)-1]='\0';
server_port = *(int*)s_port;
}

}
}
}
}
