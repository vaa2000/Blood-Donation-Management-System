/*Blood donation*/
#define     TRUE    1

#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

struct node
{
  char name[10],address[20];
  unsigned long int contact_no;
  int no_of_days;
  char blood_group;
  struct node *next;
};

struct node *startAn=NULL;
struct node *startAp=NULL;
struct node *startBn=NULL;
struct node *startBp=NULL;
struct node *startOn=NULL;
struct node *startOp=NULL;
struct node *startABn=NULL;
struct node *startABp=NULL;

/* function prototypes */
struct node *insert_end(struct node *, char, int);
struct node *delete_beg(struct node *);
void display_function(struct node *start);
//void insert_end_from_file(void);
void display_file(void);


int main()
{
    int option,no_of_days;
    char blg,c1;
    char choice= 0;

    printf("\n Following is notation A->A+ve,a->A-ve,B->B+ve,b->B-ve,O->O+ve,o->O-ve,T->AB+ve,t->AB-ve\n");

    while(TRUE)
    {
        printf("\n===============\n");
        printf("Menu \n");
        printf("===============\n");
        printf("1. Add a blood donor\n");
        printf("2. Send a request\n");
        printf("3. Display selected list\n");
        printf("4. Read from file.\n");
        printf("5. Exit\n");
        printf("===============\n");
        printf("Your option: ");
        scanf("%d",&option);

        switch(option)
        {
            case 1:
                printf("\nEnter no of days since last donation: ");
                scanf("%d",&no_of_days);

                if(no_of_days>=30)
                {
                    printf("\nEnter the blood group: ");
                    choice=getche();

                    switch(choice)
                    {
                        case 'A':
                            startAp=insert_end(startAp, choice, no_of_days);
                            break;
                        case 'a':
                            startAn= insert_end(startAn, choice, no_of_days);
                            break;
                        case 'B':
                            startBp=insert_end(startBp, choice, no_of_days);
                            break;
                        case 'b':
                            startBn=insert_end(startBn, choice, no_of_days);
                            break;
                        case 'O':
                            startOp=insert_end(startOp, choice, no_of_days);
                            break;
                        case 'o':
                            startOn=insert_end(startOn, choice, no_of_days);
                            break;
                        case 'T':
                            startABp=insert_end(startAp, choice, no_of_days);
                            break;
                        case 't':
                            startABn=insert_end(startABn, choice, no_of_days);
                            break;
                        default:
                            break;
                    }
                    printf("\nTHANK YOU FOR REGISTERING TO DONATE BLOOD\n");
                }
                else
                {
                    printf("\nKINDLY DONATE BLOOD AFTER A DURATION OF 30 DAYS FROM PREVIOUS DONATION\n");
                }
                break;

            case 2:
                printf("To send a request regarding requirement of blood of particular group\n enter the blood group");
                choice=getche();

                switch(choice)
                {
                    case 'a':
                        startAn=delete_beg(startAn);
                        break;
                    case 'A':
                        startAp=delete_beg(startAp);
                        break;
                    case 'b':
                        startBn=delete_beg(startBn);
                        break;
                    case 'B':
                        startBp=delete_beg(startBp);
                        break;
                    case 'o':
                        startOn=delete_beg(startOn);
                        break;
                    case 'O':
                        startOp=delete_beg(startOp);
                        break;
                    case 't':
                        startABn=delete_beg(startABn);
                        break;
                    case 'T':
                        startABp=delete_beg(startABp);
                        break;
                    default:
                        break;
                }
            break;

            case 3:
                printf("To see the list enter valid blood group :");
                c1= getche();
                switch (c1)
                {
                    case 'a':
                        display_function(startAn);
                        break;
                    case 'A':
                        display_function(startAp);
                        break;
                    case 'b':
                        display_function(startBn);
                        break;
                    case 'B':
                        display_function(startBp);
                        break;
                    case 'o':
                        display_function(startOn);
                        break;
                    case 'O':
                        display_function(startOp);
                        break;
                    case 't':
                        display_function(startABn);
                        break;
                    case 'T':
                        display_function(startABp);
                        break;
                    default:
                        break;
                }
                break;

            case 4:
                printf("Displaying data from file....\n");
                display_file();
                break;
            case 5:
                printf("Exiting the program....");
                getch();
                exit(0);
                break;
            default:
                break;
        }
    }
    return 0;
}

struct node *insert_end(struct node *start, char choice, int no_of_days)
{
    struct node *new_node, *ptr;
    int i=0, j=0;
    char n[10], a[10];
    unsigned long int cn;
    char bg;
    int blg;

    printf("\nEnter name= ");
    scanf("%s",n);
    printf("Address= ");
    scanf("%s",a);
    printf("Contact no= ");
    scanf("%lu",&cn);
    printf("enter Blood group");
    bg=getche();


     // getche();
//printf("\nName= %s\n", n);
//printf("Address= %s\n", a);
//printf("Contact no= %lu\n", cn);
//printf("Blood group= %c\n", bg);
//getch();
    new_node=(struct node *)malloc(sizeof(struct node));
    for(i= 0; i<10; ++i)
        new_node->name[i]= '\0';
    for(i=0; i<20; ++i)
        new_node->address[i]= '\0';

    strcpy(new_node->name, n);
    strcpy(new_node->address, a);

    new_node->contact_no=cn;
    new_node->blood_group=bg;
    new_node->no_of_days= no_of_days;

    if(start==NULL)
    {
        start=new_node;
        new_node->next=NULL;
    }
    else
    {
        ptr=start;
        while(ptr->next!=NULL)
            ptr=ptr->next;
        ptr->next=new_node;
        new_node->next=NULL;
    }

   /* if (fp == NULL)
    {
        printf("\nPlease check if the file exists\n");
        getch();
        exit(0);
    }*/
    FILE *fp1,*fp2,*fp3,*fp4,*fp5,*fp6,*fp7,*fp8;

                    switch(choice)
                    {
                        case 'A':fp1=fopen("Apositive.txt","a");
                                {
                                   fprintf(fp1,"\t%s",n);  // name
                                   fprintf(fp1,"\t%s",a);  // address
                                   fprintf(fp1,"\t%lu",cn);   // contact
                                   fprintf(fp1,"\t%d",no_of_days);    // no of days since last blood donation.
                                   putc(bg,fp1);
                                }
                                fclose(fp1);
                                   break;
                        case 'a':fp2=fopen("Anegative.txt","a");
                                {
                                   fprintf(fp2,"\t%s",n);  // name
                                   fprintf(fp2,"\t%s",a);  // address
                                   fprintf(fp2,"\t%lu",cn);   // contact
                                   fprintf(fp2,"\t%d",no_of_days);    // no of days since last blood donation.
                                   putc(bg,fp2);
                                }
                                fclose(fp2);
                                   break;
                        case 'B':fp3=fopen("Bpositive.txt","a");
                                {
                                   fprintf(fp3,"\t%s",n);  // name
                                   fprintf(fp3,"\t%s",a);  // address
                                   fprintf(fp3,"\t%lu",cn);   // contact
                                   fprintf(fp3,"\t%d",no_of_days);    // no of days since last blood donation.
                                   putc(bg,fp3);
                                }
                                fclose(fp3);
                                   break;
                        case 'b':fp4=fopen("Bnegative.txt","a");
                                {
                                   fprintf(fp4,"\t%s",n);  // name
                                   fprintf(fp4,"\t%s",a);  // address
                                   fprintf(fp4,"\t%lu",cn);   // contact
                                   fprintf(fp4,"\t%d",no_of_days);    // no of days since last blood donation.
                                   putc(bg,fp4);
                                }
                                fclose(fp4);
                                   break;
                        case 'O':fp5=fopen("Opositive.txt","a");
                                {
                                   fprintf(fp5,"\t%s",n);  // name
                                   fprintf(fp5,"\t%s",a);  // address
                                   fprintf(fp5,"\t%lu",cn);   // contact
                                   fprintf(fp5,"\t%d",no_of_days);    // no of days since last blood donation.
                                   putc(bg,fp5);
                                }
                                fclose(fp5);
                                   break;
                        case 'o':fp6=fopen("Onegative.txt","a");
                                {
                                   fprintf(fp6,"\t%s",n);  // name
                                   fprintf(fp6,"\t%s",a);  // address
                                   fprintf(fp6,"\t%lu",cn);   // contact
                                   fprintf(fp6,"\t%d",no_of_days);    // no of days since last blood donation.
                                   putc(bg,fp6);
                                }
                                fclose(fp6);
                                   break;
                        case 'T':fp7=fopen("ABpositive.txt","a");
                                {
                                   fprintf(fp7,"\t%s",n);  // name
                                   fprintf(fp7,"\t%s",a);  // address
                                   fprintf(fp7,"\t%lu",cn);   // contact
                                   fprintf(fp7,"\t%d",no_of_days);    // no of days since last blood donation.
                                   putc(bg,fp7);
                                }
                                fclose(fp7);
                                   break;
                        case 't':fp8=fopen("ABnegative.txt","a");
                                {
                                   fprintf(fp8,"\t%s",n);  // name
                                   fprintf(fp8,"\t%s",a);  // address
                                   fprintf(fp8,"\t%lu",cn);   // contact
                                   fprintf(fp8,"\t%d",no_of_days);    // no of days since last blood donation.
                                   putc(bg,fp8);
                                }
                                fclose(fp8);
                                   break;
                        default:
                            break;
                    }
  return start;
}

struct node *delete_beg(struct node *start)
{
   struct node *ptr;

   if(start==NULL)
   {
       printf("no node available/UNDERFLOW\n");
   }
   else
   {
     ptr=start;
     start=start->next;
     free(ptr);

   }

   return start;
}

void display_function(struct node *start)
{
    struct node *ptr;
    ptr= start;
    while(ptr!=NULL)
    {
        printf("\n%s\t%s\t%lu\t%c\t%d\n",ptr->name,ptr->address,ptr->contact_no,ptr->blood_group,ptr->no_of_days);
        ptr=ptr->next;
    }
  return;
}

/* function to read from file and enter the data in linked lists of donors of each blood group*/
/*
void insert_end_from_file(void)
{
    struct node *new_node, *ptr;
    int i=0, j=0;
    char n[10], a[10];
    unsigned long int cn;
    int no_of_days;
    int bg;
    FILE *fp;

    fp= fopen("Blood_donor_data.txt","r");
    if (fp == NULL)
    {
        printf("\nPlease check if the file exists\n");
        getch();
        exit(0);
    }

    while (!feof(fp))
    {


        fscanf(fp,"%s",n);  // name
        //printf("\nEnter name= %s\n",n);

        fscanf(fp,"%s",a);  // address
        //printf("Address= %s\n",a);


        fscanf(fp,"%lu",&cn);   // contact no
        //printf("Contact no= %lu", cn);
        fscanf(fp,"%d",&no_of_days);    // no of days since last blood donation.
        //printf("%d\n", no_of_days);

        fscanf(fp,"%d",&bg);            // blood group
        //printf("%d\n", bg);
//getch();
//exit(0);

        //bg= choice; // getche();
        //printf("Blood group= %c", choice);

        //printf("\nName= %s\n", n);
        //printf("Address= %s\n", a);
        //printf("Contact no= %lu\n", cn);
        //printf("Blood group= %c\n",
        //getch();
        new_node=(struct node *)malloc(sizeof(struct node));
        for(i= 0; i<10; ++i)
            new_node->name[i]= '\0';
        for(i=0; i<20; ++i)
            new_node->address[i]= '\0';

        strcpy(new_node->name, n);
        strcpy(new_node->address, a);

        new_node->contact_no= cn;
        new_node->no_of_days= no_of_days;

        switch(bg)
        {
            case 10:
                new_node->blood_group= 'A';
                ptr= startAp;
                break;
            case 11:
                new_node->blood_group= 'a';
                ptr= startAn;
                break;
            case 12:
                new_node->blood_group= 'B';
                ptr= startBp;
                break;
            case 13:
                new_node->blood_group= 'b';
                ptr= startBn;
                break;
            case 14:
                new_node->blood_group= 'O';
                ptr= startOp;
                break;
            case 15:
                new_node->blood_group= 'o';
                ptr= startOn;
                break;
            case 16:
                new_node->blood_group= 'T';
                ptr= startABp;
                break;
            case 17:
                new_node->blood_group= 't';
                ptr= startABn;
                break;
            default:
                new_node->blood_group= 'A';
                ptr= startAp; // default
                break;
        }


        if(ptr == NULL)
        {
            ptr= new_node;
            new_node->next=NULL;
            switch(bg)
            {
                case 10:
                    startAp= ptr;
                    break;
                case 11:
                    startAn= ptr;
                    break;
                case 12:
                    startBp= ptr;
                    break;
                case 13:
                    startBn= ptr;
                    break;
                case 14:
                    startOp= ptr;
                    break;
                case 15:
                    startOn= ptr;
                    break;
                case 16:
                    startABp= ptr;
                    break;
                case 17:
                    startABn= ptr;
                    break;
                default:
                    startAp= ptr; // default
                    break;
            }
        }
        else
        {
            while(ptr->next!=NULL)
                ptr= ptr->next;
            ptr->next= new_node;
            new_node->next= NULL;
        }
    }
    fclose(fp);

    return;
}*/

void display_file()
{
    char choice;
    char n[10],a[20];
    char bg;
    unsigned long cn;
    int no_of_days;
    FILE *fp1,*fp2,*fp3,*fp4,*fp5,*fp6,*fp7,*fp8;

    printf("enter blood group whose details are required");
    choice=getche();

                    switch(choice)
                    {
                        case 'A':fp1=fopen("Apositive.txt","r");
                                while(!feof(fp1))
                                {
                                   fscanf(fp1,"\t%s",n);  // name
                                   printf("name %s",n);

                                   fscanf(fp1,"\t%s",a);
                                   printf("address %s",a);

                                   fscanf(fp1,"\t%lu",cn);
                                   printf("contact no :%lu",cn);
                                      // contact
                                   fscanf(fp1,"\t%d",no_of_days);
                                   printf("no of days %d",no_of_days);

                                   bg=fgetc(fp1);
                                   printf("blood group %c",bg);  // no of days since last blood donation.

                                }
                                fclose(fp1);
                                   break;
                        case 'a':fp2=fopen("Anegative.txt","w");
                                while(!feof(fp2))
                                {
                                   fscanf(fp2,"\t%s",n);  // name
                                   printf("name %s",n);

                                   fscanf(fp2,"\t%s",a);
                                   printf("address %s",a);

                                   fscanf(fp2,"\t%lu",cn);
                                   printf("contact no :%lu",cn);
                                      // contact
                                   fscanf(fp2,"\t%d",no_of_days);
                                   printf("no of days %d",no_of_days);

                                   bg=fgetc(fp2);
                                   printf("blood group %c",bg);    // no of days since last blood donation.

                                }
                                fclose(fp2);
                                   break;
                        case 'B':fp3=fopen("Bpositive.txt","w");
                                while(!feof(fp3))
                                {
                                   fscanf(fp3,"\t%s",n);  // name
                                   printf("name %s",n);

                                   fscanf(fp3,"\t%s",a);
                                   printf("address %s",a);

                                   fscanf(fp3,"\t%lu",cn);
                                   printf("contact no :%lu",cn);
                                      // contact
                                   fscanf(fp3,"\t%d",no_of_days);
                                   printf("no of days %d",no_of_days);

                                   bg=fgetc(fp3);
                                   printf("blood group %c",bg);   // no of days since last blood donation.

                                }
                                fclose(fp3);
                                   break;
                        case 'b':fp4=fopen("Bnegative.txt","w");
                                while(!feof(fp4))
                                {
                                   fscanf(fp4,"\t%s",n);  // name
                                   printf("name %s",n);

                                   fscanf(fp4,"\t%s",a);
                                   printf("address %s",a);

                                   fscanf(fp4,"\t%lu",cn);
                                   printf("contact no :%lu",cn);
                                      // contact
                                   fscanf(fp4,"\t%d",no_of_days);
                                   printf("no of days %d",no_of_days);

                                   bg=fgetc(fp4);
                                   printf("blood group %c",bg);    // no of days since last blood donation.

                                }
                                fclose(fp4);
                                   break;
                        case 'O':fp5=fopen("Opositive.txt","w");
                                while(!feof(fp5))
                                {
                                   fscanf(fp5,"\t%s",n);  // name
                                   printf("name %s",n);

                                   fscanf(fp5,"\t%s",a);
                                   printf("address %s",a);

                                   fscanf(fp5,"\t%lu",cn);
                                   printf("contact no :%lu",cn);
                                      // contact
                                   fscanf(fp5,"\t%d",no_of_days);
                                   printf("no of days %d",no_of_days);

                                   bg=fgetc(fp5);
                                   printf("blood group %c",bg);    // no of days since last blood donation.

                                }
                                fclose(fp5);
                                   break;
                        case 'o':fp6=fopen("Onegative.txt","w");
                                while(!feof(fp6))
                                {
                                   fscanf(fp6,"\t%s",n);  // name
                                   printf("name %s",n);

                                   fscanf(fp6,"\t%s",a);
                                   printf("address %s",a);

                                   fscanf(fp6,"\t%lu",cn);
                                   printf("contact no :%lu",cn);
                                      // contact
                                   fscanf(fp6,"\t%d",no_of_days);
                                   printf("no of days %d",no_of_days);

                                   bg=fgetc(fp6);    // no of days since last blood donation.
                                   printf("blood group %c",bg);
                                }
                                fclose(fp6);
                                   break;
                        case 'T':fp7=fopen("ABpositive.txt","w");
                                while(!feof(fp7))
                                {
                                   fscanf(fp7,"\t%s",n);  // name
                                   printf("name %s",n);

                                   fscanf(fp7,"\t%s",a);
                                   printf("address %s",a);

                                   fscanf(fp7,"\t%lu",cn);
                                   printf("contact no :%lu",cn);
                                      // contact
                                   fscanf(fp7,"\t%d",no_of_days);
                                   printf("no of days %d",no_of_days);

                                   bg=fgetc(fp7);    // no of days since last blood donation.
                                   printf("blood group %c",bg);
                                }
                                fclose(fp7);

                                 break;
                        case 't':fp8=fopen("ABnegative.txt","r");
                                while(!feof(fp8))
                                {
                                   fscanf(fp8,"\t%s",n);  // name
                                   printf("name %s",n);

                                   fscanf(fp8,"\t%s",a);
                                   printf("address %s",a);

                                   fscanf(fp8,"\t%lu",cn);
                                   printf("contact no :%lu",cn);
                                      // contact
                                   fscanf(fp8,"\t%d",no_of_days);
                                   printf("no of days %d",no_of_days);

                                   bg=fgetc(fp8);
                                   printf("blood group %c",bg);    // no of days since last blood donation.

                                }
                                fclose(fp8);
                                   break;
                        default:
                            break;
                    }
}
