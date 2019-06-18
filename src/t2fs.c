
/**
*/

#include "../include/t2fs.h"
#include "../include/apidisk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#define MAX_OPEN_FILES 200
#define MAX_OPEN_DIRS 200
#define TAMANHO_SETOR 256

#define FALSE 0
#define TRUE 1



//DIRENT2 : Descritor

//typedef struct {
//    char    name[MAX_FILE_NAME_SIZE+1]; /* Nome do arquivo cuja entrada foi lida do disco      */
//    BYTE    fileType;                   /* Tipo do arquivo: regular (0x01) ou diretório (0x02) */
//    DWORD   fileSize;                  /* Numero de bytes do arquivo */
//} DIRENT2;



typedef struct {
    char*    name; /* Nome do arquivo cuja entrada foi lida do disco      */
    BYTE    fileType;                   /* Tipo do arquivo: regular (0x01) ou diretório (0x02) */
	DWORD   fileSize; 
    DWORD	blocoInicial;
    DWORD	numeroDeBlocos;
} Registro;


Registro lista_registros[200];
int index_registros = 0;
//Arquivos / Diretorios Abertos

typedef struct t2fs_openfile{
	Registro registro;
	DWORD currentPointer; // Em bytes a partir do inicio do arquivo!
} OpenFile;

//Super Bloco
/** Superbloco */
struct informacoes_disco{       		
	DWORD    NUMERO_DE_BLOCOS; 		
	DWORD    SETORES_POR_BLOCO;	
	DWORD    NUMERO_DE_SETORES;		
	};
typedef struct informacoes_disco INFO_DISCO;

INFO_DISCO CONTROLE;


int tamanho_bloco = 0;
int inicio_escrita_blocos = 0;
int ultimo_bloco_escrito = 0;

unsigned char buffer_setor[TAMANHO_SETOR];

int inicializado = FALSE;

char currentPath[200];

OpenFile arquivos_abertos[MAX_OPEN_FILES];
OpenFile diretorios_abertos[MAX_OPEN_DIRS];


/* AUXILIARES */

void inicializaT2FS();
void inicializaArquivosAbertos();
void inicializaDiretoriosAbertos();
FILE2 getFreeFileHandle();
FILE2 getFreeDirHandle();
int isFileHandleValid(FILE2);
int isDirHandleValid(DIR2);
int VerificaSeNomeJaExiste(char*);
int VerificaSeRegistroExiste(char*,BYTE);
void TrataNomesDuplicados(char*);
void writeBlock(int, char*);
int PegaInformacoesDoDisco();
void limpa_buffer( unsigned char buffer[]);
int ApagaRegistroPeloNome( char *, int);
/*-----------------------------------------------------------------------------
Função:	Informa a identificação dos desenvolvedores do T2FS.
-----------------------------------------------------------------------------*/
int identify2 (char *name, int size) {

	char *grupo = "Diennifer P Vargas 00261612\nHenrique C Jordão 00230188\nNicolas C Fernandes 00230281\0";
	if(size < strlen(grupo)){
		printf("faltou espaço\n");
		return -1;
	}
	strncpy(name, grupo, strlen(grupo)+1);

	return 0;
}

/*-----------------------------------------------------------------------------
Função:	Formata logicamente o disco virtual t2fs_disk.dat para o sistema de
		arquivos T2FS definido usando blocos de dados de tamanho 
		corresponde a um múltiplo de setores dados por sectors_per_block.
-----------------------------------------------------------------------------*/
int format2 (int sectors_per_block) {
	tamanho_bloco = sectors_per_block;
	unsigned char buffer[SECTOR_SIZE] = {0};

	//Coloca no Setor 0 --> MAXIMO DE BLOCOS
	
	int MAX_BLOCOS = 500; // Serão Sempre 500 blocos
	
	sprintf ((char *)buffer, "%i",MAX_BLOCOS);
	if(write_sector(0, buffer) != 0)
	{
		return -1;
	}

	// Coloca no Setor 1 --> Setores Por Bloco (MAX 8)
	limpa_buffer(buffer);
	
	sprintf ((char *)buffer, "%i",sectors_per_block);
	if(write_sector(1, buffer) != 0)
	{
		return -1;
	}

	// Coloca no Setor 2 --> Total de Setores (Max 4000)
	limpa_buffer(buffer);
	sprintf ((char *)buffer, "%i",sectors_per_block* MAX_BLOCOS);
	
	if(write_sector(2, buffer) != 0)
	{
		return -1;
	}

	// Falta ainda colocar a lista de Registros;

	// Falta ainda colocar o vetor de bits;


	// Esvazia o resto do disco; Se adicionar mais coisas atualize o inicio do I
	
	int i = 3;
	for (i = 3; i < sectors_per_block* MAX_BLOCOS; i++)
	{
		limpa_buffer(buffer);
		if(write_sector(i, buffer) != 0)
		{
			return -1;
		}
		
	}  
	

	inicializado = FALSE;
	return 0;
}



/*-----------------------------------------------------------------------------
Função:	Função usada para criar um novo arquivo no disco e abrí-lo,
		sendo, nesse último aspecto, equivalente a função open2.
		No entanto, diferentemente da open2, se filename referenciar um 
		arquivo já existente, o mesmo terá seu conteúdo removido e 
		assumirá um tamanho de zero bytes.
-----------------------------------------------------------------------------*/
FILE2 create2 (char *filename) {
    inicializaT2FS();

    if (VerificaSeNomeJaExiste(filename) != 0)
    {
    	printf("%s\n","Esse nome já existe");
    	return -1;  	
    }
   
    strncpy(lista_registros[index_registros].name,filename,MAX_FILE_NAME_SIZE - 1);
    printf("O nome é: %s", lista_registros[index_registros].name);
    lista_registros[index_registros].fileType = ARQUIVO_REGULAR;
    lista_registros[index_registros].blocoInicial = index_registros;
    lista_registros[index_registros].numeroDeBlocos = 1;
    index_registros++;

  
    //Tem que colocar o arquivo num bloco de disco
    //Tem que Atualizar o Vetor de Bits

    //writeBlock()


	return 0;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para remover (apagar) um arquivo do disco. 
-----------------------------------------------------------------------------*/
int delete2 (char *filename) {

	Registro regDeletado;
	regDeletado.name = malloc(sizeof(char) * MAX_FILE_NAME_SIZE);

	strncpy(regDeletado.name,filename,MAX_FILE_NAME_SIZE - 1);
 
	int encontrou =ApagaRegistroPeloNome(filename,ARQUIVO_REGULAR);
	
	//Apaga o arquivo	
	//Atualiza Vetor de bits

	return encontrou;
}

/*-----------------------------------------------------------------------------
Função:	Função que abre um arquivo existente no disco.
-----------------------------------------------------------------------------*/
FILE2 open2 (char *filename) {
	inicializaT2FS();
	FILE2 freeHandle = getFreeFileHandle();
	if(freeHandle == -1)
		return -1;

	Registro registro;

	if(registro.fileType == ARQUIVO_REGULAR)
	{
		arquivos_abertos[freeHandle].registro = registro;
		arquivos_abertos[freeHandle].currentPointer = 0;
		return freeHandle;
	}

	return -3;

}

/*-----------------------------------------------------------------------------
Função:	Função usada para fechar um arquivo.
-----------------------------------------------------------------------------*/
int close2 (FILE2 handle) {
	inicializaT2FS();
	if(isFileHandleValid(handle))
	{
		arquivos_abertos[handle].registro.fileType = INVALID_PTR;
		return 0;
	}

	return -1; //FileHandleInvalido
}

/*-----------------------------------------------------------------------------
Função:	Função usada para realizar a leitura de uma certa quantidade
		de bytes (size) de um arquivo.
-----------------------------------------------------------------------------*/
int read2 (FILE2 handle, char *buffer, int size) {
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para realizar a escrita de uma certa quantidade
		de bytes (size) de  um arquivo.
-----------------------------------------------------------------------------*/
int write2 (FILE2 handle, char *buffer, int size) {
	inicializaT2FS();

		
	return -2;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para truncar um arquivo. Remove do arquivo 
		todos os bytes a partir da posição atual do contador de posição
		(current pointer), inclusive, até o seu final.
-----------------------------------------------------------------------------*/
int truncate2 (FILE2 handle) {
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Altera o contador de posição (current pointer) do arquivo.
-----------------------------------------------------------------------------*/
int seek2 (FILE2 handle, DWORD offset) {
	inicializaT2FS();

	OpenFile file;
	
	file = arquivos_abertos[handle];

	if(file.registro.fileType == ARQUIVO_REGULAR)
	{
		if(offset != -1)
			file.currentPointer = offset;
		else 
		{
			file.currentPointer = -1; //DUVIDA
		}
		arquivos_abertos[handle] = file;
		return 0;
	}

	return -2;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um novo diretório.
-----------------------------------------------------------------------------*/
int mkdir2 (char *pathname) {
	inicializaT2FS();
    

    if (VerificaSeNomeJaExiste(pathname) != 0)
    {
    	printf("%s\n","Esse nome já existe");
    	return -1;  	
    }
	printf("%s",pathname);
    
   
    strncpy(lista_registros[index_registros].name,pathname,50);
    lista_registros[index_registros].fileType = ARQUIVO_DIRETORIO;
    lista_registros[index_registros].blocoInicial = index_registros;
    lista_registros[index_registros].numeroDeBlocos = 1;
    index_registros++;

  
    //Tem que colocar o diretorio num bloco de disco
    //Tem que Atualizar o Vetor de Bits

    //writeBlock()


	return 0;






	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para remover (apagar) um diretório do disco.
-----------------------------------------------------------------------------*/
int rmdir2 (char *pathname) {
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para alterar o CP (current path)
-----------------------------------------------------------------------------*/
int chdir2 (char *pathname) {
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para obter o caminho do diretório corrente.
-----------------------------------------------------------------------------*/
int getcwd2 (char *pathname, int size) {
	inicializaT2FS();

	strncpy(pathname, currentPath, size);

	if(strcmp(pathname, "/") != 0)
	{
		int len = strlen(pathname);
		pathname[len - 1] = '\0';
	}
	return 0;
}

/*-----------------------------------------------------------------------------
Função:	Função que abre um diretório existente no disco.
-----------------------------------------------------------------------------*/
DIR2 opendir2 (char *pathname) {
	
	inicializaT2FS();

	DIR2 freeHandle = getFreeDirHandle();
	if(freeHandle == -1){
		return -1; // Não tem handles disponiveis
	}

	Registro registro;
	
	diretorios_abertos[freeHandle].registro = registro;
	diretorios_abertos[freeHandle].currentPointer = 0;

	return freeHandle;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para ler as entradas de um diretório.
-----------------------------------------------------------------------------*/
int readdir2 (DIR2 handle, DIRENT2 *dentry) {

// Conctinua
	return 0;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para fechar um diretório.
-----------------------------------------------------------------------------*/
int closedir2 (DIR2 handle) {
	inicializaT2FS();

	if(!isDirHandleValid(handle)){
		return -1;
	}

	diretorios_abertos[handle].registro.fileType = INVALID_PTR;

	return 0;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um caminho alternativo (softlink) com
		o nome dado por linkname (relativo ou absoluto) para um 
		arquivo ou diretório fornecido por filename.
-----------------------------------------------------------------------------*/
int ln2 (char *linkname, char *filename) {
	return -1;
}


/* AUXILIARES */

void inicializaT2FS()
{
	if(inicializado)
		return;

	PegaInformacoesDoDisco();

	int i;
	for (i = 0; i < 200; i++){
		lista_registros[i].name = malloc(sizeof(char) * MAX_FILE_NAME_SIZE);
		lista_registros[i].fileType = INVALID_PTR;
	}


	inicializaArquivosAbertos();
	inicializaDiretoriosAbertos();

	ultimo_bloco_escrito = 2000; //--> Buscar do Bitmap
	
	strcpy(currentPath, "/\0");

	inicializado = TRUE;
}


int PegaInformacoesDoDisco(){


	unsigned char buffer[SECTOR_SIZE] = {0};

	//Carrega a INFO do NUMERO_TOTAL_DE_BLOCOS	

	limpa_buffer(buffer);

	if(read_sector(0, buffer) != 0){
		printf("Error: Failed reading sector 0!\n");
		return -1;
	}

	sscanf((char *)buffer, "%u", &CONTROLE.NUMERO_DE_BLOCOS);	

	
	//Carrega a INFO do NUMERO de SETORES_POR_BLOCO

	limpa_buffer(buffer);

	if(read_sector(1, buffer) != 0){
		printf("Error: Failed reading sector 1!\n");
		return -1;
	}
	
	sscanf((char *)buffer, "%u", &CONTROLE.SETORES_POR_BLOCO);

	//Carrega a INFO do NUMERO de SETORES


	limpa_buffer(buffer);

	if(read_sector(2, buffer) != 0){
		printf("Error: Failed reading sector 2!\n");
		return -1;
	}

	sscanf((char *)buffer, "%u", &CONTROLE.NUMERO_DE_SETORES);
	
	printf("\n%u",CONTROLE.NUMERO_DE_BLOCOS);
	printf("\n%u",CONTROLE.SETORES_POR_BLOCO);
	printf("\n%u\n",CONTROLE.NUMERO_DE_SETORES);

	return 0;

}


void inicializaArquivosAbertos(){

	int i;
	for (i = 0; i < MAX_OPEN_FILES; i++)
	{
		arquivos_abertos[i].registro.fileType = INVALID_PTR;
	}
}

void inicializaDiretoriosAbertos(){

	int i;
	for (i = 0; i < MAX_OPEN_DIRS; i++)
	{
		diretorios_abertos[i].registro.fileType = INVALID_PTR;
	}
}

FILE2 getFreeFileHandle(){
	FILE2 freeHandle;
	for(freeHandle = 0; freeHandle < MAX_OPEN_FILES; freeHandle++)
	{
		if(arquivos_abertos[freeHandle].registro.fileType == INVALID_PTR)
			return freeHandle;
	}
	return -2;
}

DIR2 getFreeDirHandle(){
	DIR2 freeHandle;
	for(freeHandle = 0; freeHandle < MAX_OPEN_DIRS; freeHandle++)
	{
		if(diretorios_abertos[freeHandle].registro.fileType == INVALID_PTR)
			return freeHandle;
	}
	//return -2;
	return 0;
}

int isFileHandleValid(FILE2 handle){
	if(handle < 0 || handle >= MAX_OPEN_FILES || arquivos_abertos[handle].registro.fileType != ARQUIVO_REGULAR)
		return FALSE;
	else
		return TRUE;
}

int isDirHandleValid(DIR2 handle){
	if(handle < 0 || handle >= MAX_OPEN_DIRS || diretorios_abertos[handle].registro.fileType != ARQUIVO_DIRETORIO)
		return FALSE;
	else
		return TRUE;
}

int VerificaSeNomeJaExiste(char* nome)

{	int cont = 0;
	for(cont = 0; cont < index_registros; cont++)
	{
		if(lista_registros[cont].name != NULL &&  strcmp(lista_registros[cont].name, nome) == 0)
		{
		   return -1;
		}
	}

	return 0;
}


int VerificaSeRegistroExiste(char* nome, BYTE tipo)
{
	int cont = 0;
	printf("index: %d",index_registros);
	for(cont = 0; cont < index_registros; cont++)
	{
		printf("%s",lista_registros[cont].name);		
		if(lista_registros[cont].name != NULL && strcmp(lista_registros[cont].name, nome) == 0 && lista_registros[cont].fileType == tipo)
		{
		   return cont;
		}
	}
	printf("Não Foi Encontrado Registro com esse nome");
	return -1;
}

int ApagaRegistroPeloNome( char * pathname, int tipo)
{
	int indice = VerificaSeRegistroExiste(pathname,tipo);
	if(indice < 0)
	{
		return -1;
	}
	lista_registros[indice].fileType = INVALID_PTR; 
	return 0;
}


void writeBlock(int blockNumber, char *buffer){
	unsigned char auxBuffer[SECTOR_SIZE];
	int sector = blockNumber*tamanho_bloco;
	int sectors = 0;
	int i, currentByte=0;
	
	while(sectors < tamanho_bloco ){
			for(i = 0; i< SECTOR_SIZE; i++){
				auxBuffer[i] = buffer[currentByte];
				currentByte++;
			}
			if(write_sector(sector, auxBuffer) == 0){
				sector++;
				sectors++;
			}
		}

}


void limpa_buffer( unsigned char buffer[])
{
	int i = 0;
	for (i = 0; i < TAMANHO_SETOR; i++)
	{
		buffer[i] = 0;
	}
}
