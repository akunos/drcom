#include <unistd.h>
#include <stdio.h>

#include <stdlib.h>
int main()
{
 char buf[80];
 getcwd(buf, sizeof(buf));
 FILE* rf = fopen("drcom.conf","r");
 if(rf==NULL)
 { 
	 printf("drcom.conf cannot open\n");
	 exit(1);
 }

    char a[1024][128];
    int i=0;
    while(fgets(a[i],128,rf))i++;
    int n=i; 
    printf("行数：%d\n",n);
    fclose(rf);
    i=0;
    while(i<n)printf("%s",a[i++]);
    return 0;
}

