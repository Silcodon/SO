#include "header.h"
#include "drone_movement.h"


Config cf;
Coordenadas max;
Armazem armazem[MAX_ARMAZENS];
mem_partilhada *mp;
int shmid;
int fd;
pid_t armazens[MAX_ARMAZENS];
int n_produtos=0;
Encomenda pedidos[MAX];


pid_t id_centr,getid;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t *sem_esta;
sem_t *sem_drone;
sigset_t set_bloqueado,intmask;
int mq_id;



//ler config
void ler_config(Armazem* armazem){

	FILE *f;
	char linha[MAX];
	char *prod;
	int i = 0;

	f = fopen("config.txt","r"); //Abrir ficheiro
	
	/* Se ficheiro nao existir */
	if (f == NULL){
		printf("Error: File not Found!\n");
	}

	fgets(linha, MAX, f);
	cf.max.x = atoi(strtok(linha,",")); //xmax
	cf.max.y = atoi(strtok(NULL, ",")); //ymax
	if(cf.max.y <=0 || cf.max.x <= 0){
		printf("%f %f", cf.max.x, cf.max.y);
		printf("Dimensões não podem ser negativas!");
		exit(0);
	}
	printf("\n------------------------------------\n");

	printf("XMáx: %f YMáx: %f\n", cf.max.x, cf.max.y);

	fgets(linha, MAX, f);
	char *token = strtok(linha, ", ");
	printf("Produtos: ");
	while(token != NULL){
		strcpy(cf.tipoProduto[n_produtos],token);
		token = strtok(NULL, ", ");
		printf("%s ", cf.tipoProduto[n_produtos]);
		n_produtos++;
		if(n_produtos == MAX_PRODUTOS){
			printf("Numero limite de produtos atingido!\n");
			exit(0);
		}
		
	}
	
	/*Numero de drones*/
	fgets(linha, MAX, f);
	cf.n_drones = atoi(linha);
	if(cf.n_drones <= 0 ){
		printf("Numero de drones incorretos!\n");
		exit(0);
	}
	printf("Nº Drones: %d\n",cf.n_drones);

	/*Freq. Abastecimento, Quantidade e Unidade de tempo*/
	fgets(linha, MAX, f);

	cf.freq_abastecimento = atoi(strtok(linha,","));
	if(cf.freq_abastecimento<0){
		printf("Frequência de Abastecimento incorreta!\n");
		exit(0);
	}
	printf("Freq. Abastecimento: %d\n",cf.freq_abastecimento);

	cf.quant_prod = atoi(strtok(NULL, ","));
	if(cf.quant_prod<0){
		printf("Quantidade de produtos inválida!\n");
		exit(0);
	}
	printf("Quantidade: %d\n",cf.quant_prod);

	cf.unidade_tempo = atoi(strtok(NULL, ","));
	if(cf.unidade_tempo<0){
		printf("Unidade de tempo inválida!\n");
		exit(0);
	}
	printf("Unidade de tempo: %d\n",cf.unidade_tempo);

	/*Nº Armazens*/

	fgets(linha, MAX, f);
	cf.n_armazens = atoi(linha);
	if(cf.n_armazens<1 || cf.n_armazens>MAX_ARMAZENS){
		printf("Numero de armazéns inválidos!\n");
	}
	printf("Nº Armazéns: %d\n",cf.n_armazens);
 	for(i=0;i<cf.n_armazens;i++){
        	fscanf(f,"%s %*s %lf %*s %lf %*s %[^\n]",armazem[i].nome,&armazem[i].local.x,&armazem[i].local.y,linha);
        	if(armazem[i].local.x<0 || armazem[i].local.y<0 ){
            		printf("Coordenadas do armazem %s fora do limite da simulação!!\n",armazem[i].nome);
           		 exit(0);
        	}
        printf("\n%s xy: %f, %f prod: ",armazem[i].nome,armazem[i].local.x,armazem[i].local.y);
        prod = strtok (linha,", ");
        int j =0;
        while (prod != NULL){
            strncpy(armazem[i].produto[j].nome, prod,SIZENAME);
            j++;
            if(j==MAX_PRODUTOS){
                printf("\nNumero de produtos inicial do armazem %s superior ao permitido!!\n",armazem[i].nome);
                exit(0);
            }
            prod = strtok (NULL, ", ");
            armazem[i].produto[j-1].quantidade=atoi(prod);
            prod = strtok (NULL, ", ");
            printf("%s, %d, ",armazem[i].produto[j-1].nome,armazem[i].produto[j-1].quantidade);
            
        }
        
    }
    
    printf("\n------------------------------------\n");
    fclose(f);

}

void esta_handler(int signum){
	sem_wait(sem_esta);
	printf("Número total de encomendas atribuidas aos drones: %d\n",mp->enc_atribuidas);
	printf("Número total de produtos carregados de armazens: %d\n",mp->prod_carregadas);
	printf("Número total de encomendas entregues: %d\n",mp->total_enc_entregue);
	printf("Número total de produtos entregues: %d\n",mp->total_prod_entregue);
	printf("Tempo médio para conclusão de uma encomenda:%d\n",mp->temp_medio);
	sem_post(sem_esta);
}


void cria_armazem(int id,Armazem *armazem){
	Mq request;
	Mq abastece;
	int i,k;
	while(1){
		sem_wait(sem_esta);
		for(i = 0; i < MAX_PRODUTOS; i++){
			strcpy(mp->arm[id]->produto[i].nome,armazem[id].produto[i].nome);
			mp->arm[id]->produto[i].quantidade = armazem[id].produto[i].quantidade;

		}
		sem_post(sem_esta);
		msgrcv(mq_id, &request, sizeof(Mq)-sizeof(long), id, 0);
		printf("Armazem [%d] com PID[%d] recebeu pedido do drone %ld com o Id da encomenda = %d produtos\n", id, getpid(),request.mtype, request.id_encomenda);
		sleep(request.encomenda.produto->quantidade);
		msgsnd(mq_id, &request, sizeof(Mq)-sizeof(long),0);


		msgrcv(mq_id, &abastece, sizeof(Mq)-sizeof(long),cf.n_armazens+2, 0);
		for (i = 0; i < MAX_ARMAZENS; i++){
			for(k = 0; k < MAX_PRODUTOS; k++){
				if ( strcmp(armazem[i].produto[k].nome, abastece.encomenda.produto->nome)==0){
					armazem[i].produto[k].quantidade = abastece.encomenda.produto->quantidade;
				}
			}
		}

	}
	exit(0);
}

void *drone(void* idp){
	int num;
	int my_id=*((int*)idp);
	//Drone drone_ativo=&drone_ativo[my_id];
	sem_wait(sem_drone);
	/*escrever na memoria partilhada drone
	num=rand()%4;
	if (num==0){
		*drone_ativo.posicao->x=0;
		*drone_ativo.posicao->y=0;
	}
	if (num==1){
		*drone_ativo.posicao->x=cf.max.x;
		*drone_ativo.posicao->y=0;
	}
	if (num==2){
		*drone_ativo.posicao->x=0;
		*drone_ativo.posicao->y=cf.max.y;
	}
	if (num==3){
		*drone_ativo.posicao->x=cf.max.x;
		*drone_ativo.posicao->y=cf.max.y;
	}
	*drone_ativo.estado=false;
	*drone_ativo.id=my_id;
	sem_post(sem_drone);
	movimentar drone
	drone->destino.x=armazem[id].local.x;
	drone->destino.y=armazem[id].local.y;
	printf("Drone[%d] criado com sucesso.\n",my_id);
	if(move_towards((double*)&drone->posicao.x,(double*)&drone->posicao.y,drone->destino.x,drone->destino.y)==-2){
		perror("Erro no movimento do drone.");
		exit(0);
	}
	*/
	sleep(2);
	pthread_exit(NULL);
}

void envia_abastecimento(int id, Armazem *armazem){
	Mq abastece;
	
	while(1){
		abastece.mtype = cf.n_armazens+2;
		int i = rand() % 3;
		switch(i){
		case 1: strcpy(abastece.encomenda.produto->nome,armazem[id].produto[0].nome);
				abastece.encomenda.produto->quantidade = armazem[id].produto[0].quantidade;
		break;
		case 2: strcpy(abastece.encomenda.produto->nome,armazem[id].produto[1].nome);
				abastece.encomenda.produto->quantidade = armazem[id].produto[1].quantidade;
		break;
		case 3: strcpy(abastece.encomenda.produto->nome,armazem[id].produto[2].nome);
				abastece.encomenda.produto->quantidade = armazem[id].produto[2].quantidade;
		break;

	}

		msgsnd(mq_id, &abastece, sizeof(Mq)-sizeof(long), 0);
		sleep(cf.freq_abastecimento);
	}

}


Encomenda faz_encomenda(char comando[BUF_SIZE]){
	char* token;
	Encomenda pedido;
	int j;
	int quant;
	int x,y;
	int i=0;
    token = strtok(comando, " ");
    	while (token != NULL) {
       		token = strtok(NULL, " ");
			i++;
			if(i==3){
				for(j=0;j<n_produtos;j++){
					if(strcmp(token,cf.tipoProduto[j])==0){
						strcpy(pedido.produto->nome,cf.tipoProduto[j]);
						}
					}
				}
			if(i==4){
				quant=atoi(token);
				pedido.produto->quantidade=quant;

			}
			if(i==6){
				x=atoi(token);
				pedido.destino.x=x;
				}
			if(i==7){
				y=atoi(token);
				pedido.destino.y=y;
				}
    		}
		return pedido;



}

int ler_comando(char comando[BUF_SIZE]){
	char* token;
	int i=0;
	int j;
	int x,y;
	int count=0;
    token = strtok(comando, " ");
	if(strcmp(token,"ORDER")!=0){
		return -1;
	}
    	while (token != NULL) {
       		token = strtok(NULL, " ");
			i++;
			if(i==2 && strcmp(token,"prod:")!=0){
				return -1;
				}
			if(i==3){
				for(j=0;j<n_produtos;j++){
					if(strcmp(token,cf.tipoProduto[j])==0){
						count++;
						}
					}
				if(count==0){
						return -1;
						}
				}
			if(i==5 && strcmp(token,"to:")!=0){
				return -1;	
				}
			if(i==6){
				x=atoi(token);
				if(x>cf.max.x || x<0){
					return -1;
					}
				}
			if(i==7){
				y=atoi(token);
				if(y>cf.max.y || y<0){
					return -1;
					}
				}
    		}
		return 0;


}

void Central(){
	pthread_t drones[cf.n_drones];	
	char command[BUF_SIZE],command_log[BUF_SIZE];
	int ORDER_NO=0;
	//Drone drone_ativo[cf.n_drones]=malloc(cf.n_drones*sizeof(Drone));
	int id[cf.n_drones];
	int i;//menor,j,id_drone,dist;
	//menor=MAX;
	sem_wait(sem_esta);
	//*escrever na memoria partilhada
	sem_post(sem_esta);
	//Criar named pipe
	unlink(PIPE_NAME);
	if (mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0600) == -1  && (errno!= EEXIST)){
		perror("Failed to create pipe");
		exit(0);
	}

	//Cria drones
	for(i=0;i<cf.n_drones;i++){
		id[i]=i;
		if(pthread_create(&drones[i],NULL,drone,&id[i]) != 0){
			perror("Erro na criação das threads");
			exit(1);
		}
	}
	while(1){
		if ((fd = open(PIPE_NAME, O_RDONLY)) < 0) {
		perror("Cannot open pipe for reading.");
		exit(0);
	}
		read(fd,command,BUF_SIZE);
		close(fd);

		strcpy(command_log,command);
		//Verifica se comando esta correto
		if(ler_comando(command)==-1){
			if ((fd = open(PIPE_NAME, O_WRONLY)) < 0) {
			perror("Cannot open pipe for writing.");
			exit(0);
			}
			FILE *log = fopen("log.txt", "a+");
			if (log == NULL){
	       	 		printf("Não foi possível abrir ficheiro de log");
	       	 		exit(0);
			}
			fprintf(log,"Comando descartado:");
			fprintf(log,"%s\n",command_log);
			write(fd,"Erro no comando!",BUF_SIZE);
			close(fd);
			fclose(log);
		}
		else if(ler_comando(command)==0){
			if ((fd = open(PIPE_NAME, O_WRONLY)) < 0) {
				perror("Cannot open pipe for writing.");
				exit(0);
			}
			write(fd,"Comando entregue!",BUF_SIZE);
			close(fd);
			printf("Ordem recebida!");
			ORDER_NO++;
			pedidos[ORDER_NO]=faz_encomenda(command_log);
			//Mandar comando
		}
	}

	//Drone mais proximo
	//for(i=0;i<cf.n_drones;i++){
	//	for(j=0;i<-cf.n_armazens;i++){
	//		dist=distance(*drone_ativo[i]->posicao.x,*drone_ativo[i]->posicao.y,armazem[j].local.x,armazem[j].local.y);
	//		if(dist<menor){
	//			menor=dist;
	//			id_drone=i;
	//		}
	//	}
	//}

	//espera ate os drones terminarem
	for(i=0;i<cf.n_drones;i++){
		pthread_join(drones[i],NULL);
	}
}


void terminar(){
	int i;
	FILE *log = fopen("log.txt", "a+");
	if (log == NULL){
	        printf("Não foi possível abrir ficheiro de log");
	        exit(0);
	}

	//Termina processos armazem
	for(i=1;i<cf.n_armazens+1;i++){
		if (kill(armazens[i], SIGKILL) == -1){
			perror("Failed to send the KILL signal");
		}
		else{
			//tempo final armazem
			char fin_arm[20];
			time_t fim_arm=time(0);
			struct tm *timeinfo;
			timeinfo=gmtime(&fim_arm);
			strftime(fin_arm,sizeof(fin_arm),"%H:%M:%S",timeinfo);
			fprintf(log,"Armazem[%ld] terminou as:%s\n",(long)armazens[i],fin_arm);
			printf("Armazem[%ld] terminou as:%s\n",(long)armazens[i],fin_arm);
		}
	}
	

	// Elimina os semáforos
	int error = 0;
	if (sem_close(sem_esta) == -1){
		error = errno;
	}
	if (sem_unlink("SEM_ESTA") == -1){
		error=errno;
	}	
	if (error){ /* set errno to first error that occurred */
		errno = error;
	}

	error=0;
	if (sem_close(sem_drone) == -1){
		error = errno;
	}
	if (sem_unlink("SEM_DRONE") == -1){
		error=errno;
	}	
	if (error){ /* set errno to first error that occurred */
		errno = error;
	}

	//terminar memoria partilhada
	error = 0;
	if (shmdt(&mp) == -1){
		error = errno;
	}
	if ((shmctl(shmid, IPC_RMID, NULL) == -1) && !error){
		error = errno;
	}
	if(error){
		errno = error;
	}

	//Terminar pipe
	if (unlink(PIPE_NAME) == -1){
		perror("Failed to remove pipe");
	}
	close(fd);

	//Terminar message queue
	if(msgctl(mq_id, IPC_RMID, NULL)==-1){
		perror("Failed to eliminate message queue.");
	}

	//tempo final
	char final[20];
	time_t fim=time(0);
	struct tm *timeinfo;
	timeinfo=gmtime(&fim);
	strftime(final,sizeof(final),"%H:%M:%S",timeinfo);
	fprintf(log,"Evento terminou as:%s\n",final);
	printf("Evento terminou as:%s\n",final);
	fclose(log);

	//termina central
	if (kill(id_centr, SIGKILL) == -1){
		perror("Failed to send the KILL signal");
	}
	exit(0);

}

//Cria novo semaforo
void init (){
	sem_unlink("SEM_ESTA");
	if((sem_esta = sem_open("SEM_ESTA",O_CREAT|O_EXCL,0700,1)) == SEM_FAILED){
		perror("Failed to initialize semaphore");
  	}
	sem_unlink("SEM_DRONE");
	if((sem_drone = sem_open("SEM_DRONE",O_CREAT|O_EXCL,0700,1)) == SEM_FAILED){
		perror("Failed to initialize semaphore");
	}
}

int main(){
	int status;
	int i;
	pid_t my_armazem;

	//Ignora os sinais
	if(sigfillset(&set_bloqueado)==-1){
		perror("Failed to create set.");
	}
	if(sigdelset(&set_bloqueado,SIGINT)==-1){
		perror("Failed to delete SIGINT from set.");
	}
	if(sigdelset(&set_bloqueado,SIGUSR1)==-1){
		perror("Failed to delete SIGUSR1 from set.");
	}
	if(sigprocmask(SIG_BLOCK,&set_bloqueado,NULL)==-1){
		perror("Failed to block set.");
	}
	if ((sigemptyset(&intmask) == -1) || (sigaddset(&intmask, SIGINT) == -1)){
   		perror("Failed to initialize the signal mask");
	}
	if (sigprocmask(SIG_BLOCK, &intmask, NULL) == -1){
       		perror("Failed to block SIGINT.");
	}


	//Cria ficheiro log
	FILE *log = fopen("log.txt", "w+");
	if (log == NULL){
	        printf("Não foi possível criar ficheiro de log");
	        exit(0);
	}
	//tempo inicial
	char inicial[20];
	time_t inicio=time(0);
	struct tm *timeinfo;
	timeinfo=gmtime(&inicio);
	strftime(inicial,sizeof(inicial),"%H:%M:%S",timeinfo);
	fprintf(log,"Evento ocorreu as:%s\n", inicial);
	printf ("Evento ocorreu as:%s\n", inicial);

	
	fflush(log);
	init();
	ler_config(armazem);


	//cria memoria partilhada
	if ((shmid = shmget(IPC_PRIVATE, sizeof(mem_partilhada), IPC_CREAT | 0777)) < 0){
		perror("Erro ao criar a memoria partilhada\n");
		exit(1);
 	}
 	mp = (mem_partilhada*)shmat(shmid, NULL, 0);
	for(i=0;i<cf.n_armazens;i++){
		mp->arm[i]=&armazem[i];
	}
	mp->enc_atribuidas=0;
	mp->prod_carregadas=0;
	mp->total_enc_entregue=0;
	mp->total_prod_entregue=0;
	mp->temp_medio=0;
 	
 	//AO RECEBER ESTE SINAL IMPRIME ESTATISTICAS
	signal(SIGUSR1, esta_handler);
	
	//CRIAR MESSAGE QUEUE
	mq_id = msgget(IPC_PRIVATE, IPC_CREAT|0777);
	if (mq_id < 0){
		perror("Message Queue\n");
		exit(0);
	}

	//Cria armazens e central
	getid = fork();
	if(getid != 0){
   		id_centr = getpid();
 		printf("Central criada[%ld]\n",(long)id_centr);
 		Central();
		exit(0);
	}


	//Cria processos armazem
	for(i = 1; i < cf.n_armazens+1; i++){
		if((armazens[i] = fork()) == 0){
			my_armazem=getpid();
			//tempo do armazem
			char tem_armazem[20];
			time_t intervalo=time(0);
			timeinfo=gmtime(&intervalo);
			strftime(tem_armazem,sizeof(tem_armazem),"%H:%M:%S",timeinfo);
			fprintf(log,"Armazem[%ld] criado as:%s\n",(long)my_armazem,tem_armazem);
			printf("Armazem[%ld] criado as:%s\n",(long)my_armazem,tem_armazem);

			cria_armazem(i,&armazem[i]);
			exit(0);
	   		}

		}
	fclose(log);

	//Desbloqueia SIGINT
	if (sigprocmask(SIG_UNBLOCK, &intmask, NULL) == -1){
       		perror("Failed to unblock SIGINT.");
	}
	signal(SIGINT,terminar);


	//Espera que todos os armazens terminem
	do {
  		status = wait(0);
    		if(status == -1 && errno != ECHILD) {
        		perror("Erro na espera.");
        		abort();
    		}
	} while (status > 0);



	



	return 0;


}



