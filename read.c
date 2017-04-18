#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
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
int main(int argc, char **argv) {
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
     while (!feof(fp)) {
        if ((p = fgets(buf, sizeof(buf), fp)) != NULL) {
            strcpy(s, p);
            ch=del_left_trim(s)[0];
           /*判断注释 空行，如果是就直接下次循环*/
            if (ch == '#' || isblank(ch) || ch=='\n')
                continue;
          /*分割字符串*/
            p=strtok(s, delim);
            if(p)
            printf("%s", del_both_trim(p));

            while ((p = strtok(NULL, delim)))
            printf("%s ", del_both_trim(p));
            printf("\n");
        }
    }
    return 0;
}
