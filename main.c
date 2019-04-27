#include <stdio.h>
#include <time.h>
#include <string.h>
#include "mpi.h"
#include <omp.h>

int Max(int array[], int size)
{
    int maximum = array[0];
    int location;
    int c;
  for (c = 1; c < size; c++)
  {
    if (array[c] > maximum)
    {
       maximum  = array[c];
       location = c;
    }
  }
  array[location] = 0;
  return location;
}



int main(int argc, char * argv[])
{
    int my_rank;		/* rank of process		*/
    int p;				/* number of process	*/
    int source;			/* rank of sender		*/
    int dest;			/* rank of reciever		*/
    int tag = 0;		/* tag for messages		*/
    char message[100];	/* storage for message	*/
    MPI_Status status;	/* return status for */ /* recieve */

    int portionSize,rem;
    int *localArr;      /* small array of voters for every process	*/
    int *localArr2;
    int *round1Score;
    int top2[2];
    int round2Score[2];
    int round2 = 1;
    //int *round1_remScore;
    FILE *fp;
	char ch;
	char filename[10];
	int choice;
	int winner_cand;
	int candidates,voters;
	int k;
	srand(time(0));

    MPI_Init( &argc, &argv );
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &p);

	if( my_rank == 0 ) {
		while(1)
        {
            printf("Enter 1 for generate new file\n");
            printf("Enter 2 for calculations\n");
            printf("Choice: ");
            fflush(stdout);
            scanf("%d",&choice);
            if(choice==1)
            {
                printf("Enter file name that will be created ");
                fflush(stdout);
                scanf("%s",&filename);
				printf("Enter the number of candidates: ");
				fflush(stdout);
                scanf("%d",&candidates);
				printf("Enter the number of voters: ");
				fflush(stdout);
                scanf("%d",&voters);
                int i,j;
                //Generate Preferences
                fp = fopen(filename,"wb");
                int* all_voters = (int*)malloc(voters*candidates*sizeof(int));
                int ind =0;
                for (i = 0; i < voters; i++)
                {
                    int visited[candidates];
                    for(j=1; j <= candidates; j++)
                    {
                        visited[j]=j;
                    }
                    for(j=1; j <= candidates; j++)
                    {
                        int temp = visited[j];
                        int num = rand() % candidates +1 ;
                        visited[j]=visited[num];
                        visited[num]=temp;
                    }
                    for(j=1; j <= candidates; j++)
                    {
                        all_voters[ind++] = visited[j];

                    }
                }
                fwrite(&candidates, sizeof(int), 1, fp);
                fwrite(&voters, sizeof(int), 1, fp);
                fwrite(all_voters, sizeof(int), voters*candidates, fp);
                fclose(fp);
            }
                else if( choice == 2) {
                    printf("Enter the file name you want to make calculations on: ");
                    fflush(stdout);
                    scanf("%s",&filename);
                    break;
                }
                else
                {
                    fclose(fp);
                    break;
            }
        }
	}

	MPI_Bcast (filename, 10 , MPI_CHAR , 0, MPI_COMM_WORLD);

    fp = fopen(filename,"rb");

    if (my_rank == 0)
    {
        fread(&candidates, sizeof(int), 1, fp);
        fread(&voters, sizeof(int), 1, fp);
        portionSize = voters/p;
        rem = voters%p;
    }
    MPI_Bcast (&candidates, 1, MPI_INT,0,MPI_COMM_WORLD);
    MPI_Bcast (&voters, 1, MPI_INT,0,MPI_COMM_WORLD);
    MPI_Bcast (&portionSize, 1, MPI_INT,0,MPI_COMM_WORLD);
    if(my_rank == 0)
        localArr = (int*)malloc((portionSize+rem)*candidates*sizeof(int));
    else
        localArr = (int*)malloc(portionSize*candidates*sizeof(int));
    int tell;

    //Parallel read from the file
    if(my_rank != 0)
    {
      int t;
      MPI_Recv(&tell,1,MPI_INT,(my_rank-1),0,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
      fseek(fp, tell, SEEK_SET);
      fread(localArr, sizeof(int), portionSize*candidates, fp);
      tell = ftell(fp);
    }
    else
    {
        int t;
        fread(localArr, sizeof(int), (portionSize+rem)*candidates , fp);
        tell = ftell(fp);
    }
    MPI_Send(&tell,1,MPI_INT,(my_rank+1)%p,0,MPI_COMM_WORLD);
    fclose(fp);

    //Initialize Scores
    round1Score = (int*)malloc(candidates*(sizeof(int)));
    if(my_rank == 0)
    {
        for(k =0; k<candidates+1; k++)
            round1Score[k] = 0;
    }
    MPI_Bcast(round1Score, candidates+1, MPI_INT, 0, MPI_COMM_WORLD);

    //Calculate Scores
    int len;
    if(my_rank == 0)
        len = (portionSize+rem)*candidates;
    else
        len = portionSize*candidates;
    for(k=0; k<len; k+=candidates)
        round1Score[localArr[k]]++;

    //Send the results to master process
    int* r1 = (int*)malloc(candidates+1 * sizeof(int));
    MPI_Reduce(round1Score, r1, candidates+1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    //Print Output
    if(my_rank == 0)
    {
        int total = 0;
        printf("\nRound-1 Scores:\n");
        printf("================\n");
        for(k=1; k<candidates+1; k++)
        {
            total += r1[k];
            float ratio = (float)r1[k]/voters;
            if(ratio > 0.5)
            {
                round2 = 0;
                winner_cand = k;
            }
            printf("Candidate[%d] got  %d/%d which is %.0f%%\n", k, r1[k], voters, ratio*100);
        }
        printf("\n");

        if(round2 == 0)
        {
            printf("Candidate[%d] wins in round 1!\n", winner_cand);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        top2[0] = Max(r1, candidates+1);
        top2[1] = Max(r1, candidates+1);
    }
    MPI_Bcast(top2, 2, MPI_INT, 0, MPI_COMM_WORLD);

    //Initialize Round2
    if(my_rank == 0)
    {
        localArr2 = (int*)malloc((portionSize+rem)*2*sizeof(int));
        len = (portionSize+rem)*candidates;
    }
    else
    {
        localArr2 = (int*)malloc(portionSize*2*sizeof(int));
        len = portionSize*candidates;
    }

    int ind = 0;
    for(k=0; k<len; k++)
    {
        int i;
        if(localArr[k] == top2[0] || localArr[k] == top2[1])
            localArr2[ind++] = localArr[k];
    }

    //Initialize Scores
    if(my_rank == 0)
    {
        for(k =0; k<2; k++)
            round2Score[k] = 0;
    }
    MPI_Bcast(round2Score, 2, MPI_INT, 0, MPI_COMM_WORLD);

    //Calculate Scores
    if(my_rank == 0)
        len = (portionSize+rem)*2;
    else
        len = portionSize*2;
    for(k=0; k<len; k+=2)
    {
        int x;
        if(localArr2[k] == top2[0])
            x = 0;
        else
            x = 1;
        round2Score[x]++;
    }

    //Send to root
    int* r2 = (int*)malloc(2 * sizeof(int));
    MPI_Reduce(round2Score, r2, 2, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    //Print Output
    if(my_rank == 0)
    {
        int total = 0;
        printf("\nRound-2 Scores:\n");
        printf("================\n");
        for(k=0; k<2; k++)
        {
            total += r2[k];
            float ratio = (float)r2[k]/voters;
            printf("Candidate[%d] got  %d/%d which is %.0f%%\n", top2[k], r2[k], voters, ratio*100);
        }
        printf("\n");
        //printf("Total Votes: %d\n", total);
        if(r2[0] > r2[1])
            winner_cand = top2[0];
        else
            winner_cand = top2[1];
        printf("Candidate[%d] wins in round 2!\n", winner_cand);
    }
	
    MPI_Finalize();
    return 0;
}
