
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
#define MAX_FILHOS 10
#define MAX_BLOCOS 500
#define REGISTROS_POR_SETOR 1
#define SETOR_INICIO_LISTA_REGISTROS 4001
#define FALSE 0
#define TRUE 1
#define SETOR_INICIO_ESCRITA 100

//DIRENT2 : Descritor

//typedef struct {

//    char    name[MAX_FILE_NAME_SIZE+1]; /* Nome do arquivo cuja entrada foi lida do disco      */
//    BYTE    fileType;                   /* Tipo do arquivo: regular (0x01) ou diretório (0x02) */
//    DWORD   fileSize;                  /* Numero de bytes do arquivo */
//} DIRENT2;

typedef struct reg {
    char    name[MAX_FILE_NAME_SIZE]; /* Nome do arquivo      */
    char    pathName[MAX_FILE_NAME_SIZE]; /* Nome do arquivo desde o Root      */
    DWORD    fileType;                   /* Tipo do arquivo: regular (0x01) ou diretório (0x02) */
    DWORD   fileSize;
    DWORD   blocoInicial;
    DWORD   numeroDeBlocos;
    DWORD   filhos[MAX_FILHOS];
} Registro;

//Arquivos / Diretorios Abertos

typedef struct t2fs_openfile{
	Registro registro;
	DWORD currentPointer; // Em bytes a partir do inicio do arquivo!
} OpenFile;

struct informacoes_disco{
	DWORD    NUMERO_DE_BLOCOS;
	DWORD    SETORES_POR_BLOCO;
	DWORD    NUMERO_DE_SETORES;
	};
typedef struct informacoes_disco INFO_DISCO;



/* GLOBAIS */

int opendir_relativo = FALSE;

Registro lista_registros[MAX_BLOCOS]; // Lista de Todos os Registros conhecidos

struct BitMap {
	int bits[MAX_BLOCOS];
	int lastWrittenIndex;
} bitMap;

char currentPath[MAX_FILE_NAME_SIZE+1];

int index_registros = 0;

INFO_DISCO CONTROLE;

int tamanho_bloco = 0;

int inicio_escrita_blocos = 0;

int ultimo_bloco_escrito = 0;

unsigned char buffer_setor[TAMANHO_SETOR];

int inicializado = FALSE;

OpenFile arquivos_abertos[MAX_OPEN_FILES];

OpenFile diretorios_abertos[MAX_OPEN_DIRS];

/********** AUXILIARES ********/

/* Incializacao */
void inicializaT2FS();
void inicializaArquivosAbertos();
void inicializaDiretoriosAbertos();

/* Handle de Arquivos e Diretorios */
FILE2 getFreeFileHandle(int);
FILE2 getFreeDirHandle();
int isFileHandleValid(FILE2);
int isDirHandleValid(DIR2);

/* Manipulação de Registros */
void CriaRegistroDiretorioRoot(); //Vai Sumir
int ApagaRegistroPeloNome( char *, int);
int PegaIndexDoDiretorioAtual();
int ColocaComoFilhoDoPai(int);
int EncontraRegistroPeloPathname(char*,WORD);
int EncontraRegistroFilhoPeloNome(char*,WORD);
int deleteTree(int);
void LimpaListaRegistros();
/* Interface Com o Disco */
void writeBlock(int, char*); //Não esta sendo usada
int writeListaRegistrosNoDisco();
void writeDwordOnBuffer(unsigned char *, int, DWORD);
int writeRegistroNoSetor(int indice, int setor);
int readRegistroNoSetor(int indice, unsigned char* setor);
int readListaRegistrosNoDisco();
int PegaInformacoesDoDisco();

/* Regras de Negocio */
int VerificaSeJaTemIrmaoComMesmoNome(int,int);

/* Utilitarios */
void limpa_buffer( unsigned char buffer[]);
void append(char*, char);
int contaQuantosFilhos(int indice);
void LimpaOpenFiles();
/*-----------------------------------------------------------------------------
Função:	Informa a identificação dos desenvolvedores do T2FS.
-----------------------------------------------------------------------------*/
int identify2 (char *name, int size) {

	char *grupo = "Dieniffer P Vargas 00261612\nHenrique C Jordão 00230188\nNicolas C Fernandes 00230281\0";
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

	// Coloca no Setor 3 --> Tamanho em Setores da Lista de Registros; (cada registro ocupa um setor hoje)
	limpa_buffer(buffer);

	sprintf ((char *)buffer, "%i",1);

	if(write_sector(3, buffer) != 0)
	{
		return -1;
	}


	// Falta ainda colocar o endereço do vetor de bits;

	// Falta ainda criar o vetor de bits;
	int i;
	for (i = 0; i < MAX_BLOCOS; i++) {
		bitMap.bits[i] = 1; //1 = tem vaga, 0 = ocupado de acordo com os slides.
	}
	bitMap.lastWrittenIndex = 0; //Nenhum index escrito ainda começar com 0.


	// Esvazia o resto do disco; Se adicionar mais coisas atualize o inicio do I
	limpa_buffer(buffer);
	for (i = 4; i < 4500; i++)
	{

		if(write_sector(i, buffer) != 0)
		{
			return -1;
		}

	}

	LimpaListaRegistros();
	CriaRegistroDiretorioRoot();
	writeRegistroNoSetor(0,SETOR_INICIO_LISTA_REGISTROS);


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
    printf(" %s",currentPath); 
   
    //PathName precisa ser carregado do pai; Tem que ver as barrinhas

    strncpy(lista_registros[index_registros].name,filename,MAX_FILE_NAME_SIZE - 1);

    strncpy(lista_registros[index_registros].pathName,currentPath,MAX_FILE_NAME_SIZE - 1);
     if(strcmp(currentPath,"/") != 0)
	{
	  strcat(lista_registros[index_registros].pathName,"/");
	}

    strcat(lista_registros[index_registros].pathName,filename);

    lista_registros[index_registros].fileType = ARQUIVO_REGULAR;
    lista_registros[index_registros].blocoInicial = SETOR_INICIO_ESCRITA + index_registros;
    lista_registros[index_registros].numeroDeBlocos = 1;

    //Tem que adicionar o arquivo na lista de filhos do pai (pai é o currentPath)

   if( ColocaComoFilhoDoPai(index_registros) < 0) {
	lista_registros[index_registros].fileType = INVALID_PTR;
        printf("\nNão foi possivel colocar como filho\n");
        return -1;}

	int i;
    for(i = 0; i < MAX_FILHOS; i++)
	lista_registros[index_registros].filhos[i] = 0;

    index_registros++;

    //Tem que colocar o arquivo num bloco de disco
    //Tem que Atualizar o Vetor de Bits

	for(i = 0; i < MAX_BLOCOS; i++){
		if(bitMap.bits[i] == 1) { //Bloco está livre
			bitMap.bits[i] = 0; //Está ocupado.
			bitMap.lastWrittenIndex = i; //Salva ultimo index usado.
			break; //Sai do loop
		}
	}

	if( writeListaRegistrosNoDisco() < 0) return -19;
	return open2(filename); // Tem que retornar Open
}

/*-----------------------------------------------------------------------------
Função:	Função usada para remover (apagar) um arquivo do disco.
-----------------------------------------------------------------------------*/
int delete2 (char *filename) {

	int indice_encontrado = ApagaRegistroPeloNome(filename,ARQUIVO_REGULAR);

	//Remove do pai
	int index_pai = PegaIndexDoDiretorioAtual();

        int i;
	for(i = 0; i < MAX_FILHOS; i ++)
	   {
	    if(lista_registros[index_pai].filhos[i] == indice_encontrado)
		{
		  lista_registros[index_pai].filhos[i] = 0;
		}
    }

	//Atualiza Vetor de bits
	int posicaoDoBloco = lista_registros[index_pai].blocoInicial;
	bitMap.bits[posicaoDoBloco] = 1;

	//Ainda precisa deletar o arquivo do disco

	return indice_encontrado;
}

/*-----------------------------------------------------------------------------
Função:	Função que abre um arquivo existente no disco.
-----------------------------------------------------------------------------*/
FILE2 open2 (char *filename) {
	inicializaT2FS();
	
	int index_arquivo = EncontraRegistroFilhoPeloNome(filename,ARQUIVO_REGULAR);

	FILE2 freeHandle = getFreeFileHandle(index_arquivo);
	if(freeHandle == -1)
		return -1;




	if(index_arquivo >= 0)
	{
		arquivos_abertos[freeHandle].registro = lista_registros[index_arquivo];
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
int write2 (FILE2 handle, char *buffer, int size) 
{
	inicializaT2FS();

	char aux[SECTOR_SIZE];
	strncpy(aux,buffer,size);
	
	if(isFileHandleValid(handle))
	{	
		printf("file type %d",arquivos_abertos[handle].registro.fileType);
		if(arquivos_abertos[handle].registro.fileType == ARQUIVO_REGULAR)
		{	printf("\nbuffer :%s\n",buffer);
			printf("\nsetor escrita :%d\n",arquivos_abertos[handle].registro.blocoInicial);
			write_sector(arquivos_abertos[handle].registro.blocoInicial,(unsigned char *)&aux); //Escreve na pos o que esta no buffer
			
			arquivos_abertos[handle].currentPointer += size + 1; // Atualiza o contador de posição
			arquivos_abertos[handle].registro.fileSize += size; // Atualiza tamanho

			
			return size; // Retorna numero de bytes
		
		}
		return -2;
	}
	return -3;
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

	/*OpenFile file;

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
		*/
	return -2;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um novo diretório.
-----------------------------------------------------------------------------*/
int mkdir2 (char *pathname) {

   //Pelo certo deveria criar recebendo um caminho completo, criando os diretorios recursivamente?

   //PathName precisa ser carregado do pai; Tem que ver as barrinhas;

    inicializaT2FS();

    //printf("%s",pathname);

    strncpy(lista_registros[index_registros].name,pathname,MAX_FILE_NAME_SIZE);

    strncpy(lista_registros[index_registros].pathName,currentPath,MAX_FILE_NAME_SIZE);
    
	printf("current path %s",currentPath);


    strcat(lista_registros[index_registros].pathName,pathname);

    lista_registros[index_registros].fileType = ARQUIVO_DIRETORIO;
    lista_registros[index_registros].blocoInicial = index_registros;
    lista_registros[index_registros].numeroDeBlocos = 1;
    int i;
    for(i = 0; i < MAX_FILHOS; i++)
	lista_registros[index_registros].filhos[i] = 0;

    //printf("\n nome: %s",lista_registros[index_registros].name);
    //printf("\n pathName: %s\n",lista_registros[index_registros].pathName);

    if( ColocaComoFilhoDoPai(index_registros) < 0) {
	lista_registros[index_registros].fileType = INVALID_PTR;
        printf("\nNão foi possivel colocar como filho\n");
        return -1;}

    index_registros++;

    //Tem que Atualizar o Vetor de Bits
	for (i = 0; i<MAX_BLOCOS;i++){
		if(bitMap.bits[i] == 1){
			bitMap.bits[i] = 0;
			bitMap.lastWrittenIndex = i;
			break;
		}
	}


	if( writeListaRegistrosNoDisco() < 0) return -19;

 	opendir_relativo = TRUE;
 	return opendir2(pathname); //Tem que retornar Opendir2
}

/*-----------------------------------------------------------------------------
Função:	Função usada para remover (apagar) um diretório do disco.
-----------------------------------------------------------------------------*/
int rmdir2 (char *pathname) {
	//Tem que apagar todos os filhos recursivamente

	int indice_encontrado =ApagaRegistroPeloNome(pathname,ARQUIVO_DIRETORIO);

	//Remove do pai
	int index_pai = PegaIndexDoDiretorioAtual();

        int i;
	for(i = 0; i < MAX_FILHOS; i ++)
	   {
	    if(lista_registros[index_pai].filhos[i] == indice_encontrado)
		{
		 lista_registros[index_pai].filhos[i] = 0;
		}
           }

	//Atualiza Vetor de bits
	int posicaoBloco = lista_registros[index_pai].blocoInicial;
	bitMap.bits[posicaoBloco] = 1; //Está livre novamente;

	return indice_encontrado;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para alterar o CP (current path)
-----------------------------------------------------------------------------*/
int chdir2 (char *pathname) {
	inicializaT2FS();
	//Verificar se o diretorio desejado existe. Se não existir da erro
	if (EncontraRegistroPeloPathname(pathname,ARQUIVO_DIRETORIO) < 0) {return -1;}

	strncpy(currentPath, pathname, MAX_FILE_NAME_SIZE - 1);
	return 0;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para obter o caminho do diretório corrente.
-----------------------------------------------------------------------------*/
int getcwd2 (char *pathname, int size) {
	inicializaT2FS();
	strncpy(pathname, currentPath, size);
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

	int index_dir = 0;
	if(opendir_relativo == TRUE)
	{
	   index_dir = EncontraRegistroFilhoPeloNome(pathname,ARQUIVO_DIRETORIO);
	}else
	{
	  index_dir = EncontraRegistroPeloPathname(pathname,ARQUIVO_DIRETORIO);
	}

	if(index_dir >= 0)
	{
		diretorios_abertos[freeHandle].registro = lista_registros[index_dir];
		diretorios_abertos[freeHandle].currentPointer = 0;
	}
	opendir_relativo = FALSE;
	return freeHandle;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para ler as entradas de um diretório.
-----------------------------------------------------------------------------*/
int indice_readdir = 0;
int readdir2 (DIR2 handle, DIRENT2 *dentry) {

	if(!isDirHandleValid(handle)){
		return -1;
	}

	int indice_diretorio_lido = EncontraRegistroPeloPathname(diretorios_abertos[handle].registro.pathName,ARQUIVO_DIRETORIO);
	
	while(indice_readdir < MAX_FILHOS)
	{
	  if(lista_registros[indice_diretorio_lido].filhos[indice_readdir] != 0)
	 {

	    int indice_filho = 	lista_registros[indice_diretorio_lido].filhos[indice_readdir];
            if(indice_filho != 0 && 
		(lista_registros[indice_filho].fileType == ARQUIVO_REGULAR || lista_registros[indice_filho].fileType == ARQUIVO_DIRETORIO ))
		{
	         strncpy(dentry->name,lista_registros[indice_filho].name,MAX_FILE_NAME_SIZE);
                 dentry->fileType =  (BYTE)lista_registros[indice_filho].fileType;
                 dentry->fileSize =  (DWORD)lista_registros[indice_filho].fileSize; //Só depois
	         indice_readdir++;
	         return 0;
		}
	 }
         indice_readdir++;
	}
	indice_readdir = 0;
	return -1;
}



/*-----------------------------------------------------------------------------
Função:	Função usada para fechar um diretório.
-----------------------------------------------------------------------------*/
int closedir2 (DIR2 handle) {
	//printf("Chamou CloseDir");
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

	//Cria um novo "Registro" para um arquivo ja existente com um novo path

	return -1;
}


/* AUXILIARES */

void inicializaT2FS()
{
	if(inicializado)
		return;
	strncpy(currentPath, "/\0",2);
	PegaInformacoesDoDisco();
	
	

	readListaRegistrosNoDisco();

	inicializaArquivosAbertos();
	inicializaDiretoriosAbertos();

	ultimo_bloco_escrito = bitMap.lastWrittenIndex; //--> Buscar do VetorDeBits
	inicializado = TRUE;
}


void CriaRegistroDiretorioRoot()
{
    strncpy(lista_registros[index_registros].name,"Root",MAX_FILE_NAME_SIZE - 1);
    strncpy(lista_registros[index_registros].pathName,"/",MAX_FILE_NAME_SIZE - 1);
    lista_registros[index_registros].fileType = ARQUIVO_DIRETORIO;
    lista_registros[index_registros].fileSize = 0;
    lista_registros[index_registros].blocoInicial = -1;
    lista_registros[index_registros].numeroDeBlocos = 1;
    

    int i ;
    for (i = 0; i < MAX_FILHOS; i++)
	lista_registros[index_registros].filhos[i] = 0;

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
	diretorios_abertos[0].registro = lista_registros[0]; //root
}

FILE2 getFreeFileHandle(int index_reg){

	FILE2 freeHandle;
	for(freeHandle = 0; freeHandle < MAX_OPEN_FILES; freeHandle++)
	{
		if(arquivos_abertos[freeHandle].registro.fileType == INVALID_PTR){
			arquivos_abertos[freeHandle].registro = lista_registros[index_reg];
			return freeHandle;
		}
	}

	return -2;
}

DIR2 getFreeDirHandle(){
	DIR2 freeHandle;
	for(freeHandle = 0; freeHandle < MAX_OPEN_DIRS; freeHandle++)
	{
		if(diretorios_abertos[freeHandle].registro.fileType == INVALID_PTR)
			diretorios_abertos[freeHandle].registro = lista_registros[index_registros];
			return freeHandle;
	}
	return -2;

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


int EncontraRegistroPeloPathname(char* pathName, WORD tipo)
{
	int cont = 0;
	for(cont = 0; cont < index_registros; cont++)
	{
		if(lista_registros[cont].pathName != NULL && strcmp(lista_registros[cont].pathName, pathName) == 0 && lista_registros[cont].fileType == tipo)
		{
		   return cont;
		}
	}
	printf("\nNão Foi Encontrado Registro com esse pathName\n");
	return -1;
}

int EncontraRegistroFilhoPeloNome(char* name, WORD tipo)
{
	int cont = 0;
	int index_dir = PegaIndexDoDiretorioAtual();

	int index_filho = 0;
	for(cont = 0; cont < MAX_FILHOS; cont++)
	{

		index_filho = lista_registros[index_dir].filhos[cont];

		Registro filho = lista_registros[index_filho];

		//printf("\n%s\n",lista_registros[cont].pathName);
		if(filho.name != NULL && strcmp(filho.name, name) == 0 && filho.fileType == tipo)
		{
		   return index_filho;
		}
	}
	printf("\nNão Foi Encontrado Registro com esse nome\n");
	return -1;
}

int ApagaRegistroPeloNome( char * pathname, int tipo)
{
	int indice = EncontraRegistroFilhoPeloNome(pathname,tipo);
	if(indice < 0)
	{
		return -1;
	}
	deleteTree(indice);
	return indice;
}

int deleteTree(int indicePai)
{
    lista_registros[indicePai].fileType = INVALID_PTR;
    if (contaQuantosFilhos(indicePai) == 0) return indicePai;

    int i;
    for(i = 0; i < MAX_FILHOS;i++)
	{
	   if(lista_registros[indicePai].filhos[i] != 0)
	   	deleteTree(lista_registros[indicePai].filhos[i]);
	}
    return 0;
}

int contaQuantosFilhos(int indice)
{
   int num_filhos = 0;
   int cont = 0;
   for (cont = 0; cont < MAX_FILHOS; cont++)
	{
		if(lista_registros[indice].filhos != 0)
		{
			num_filhos++;
		}
	}

    return num_filhos;
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


int ColocaComoFilhoDoPai(int index_filho)
{
  int index_pai = PegaIndexDoDiretorioAtual();

  if(index_pai < 0) { printf("\nPai não encontrado\n"); return -1; }

  if( VerificaSeJaTemIrmaoComMesmoNome(index_pai,index_filho) < 0) {printf("\nJa Existe Irmão com o mesmo nome\n");return -2;}

   int i;
   for (i = 0; i < MAX_FILHOS; i++)
    {
    	if(lista_registros[index_pai].filhos[i] == 0)
	   {
		lista_registros[index_pai].filhos[i] = index_filho;
		return 0;
           }
    }

  return -1;
}


int VerificaSeJaTemIrmaoComMesmoNome(int index_pai,int index_filho)

{
    	 int i;
   for (i = 0; i < MAX_FILHOS; i++)
    {
    	if(lista_registros[index_pai].filhos[i] != 0)
	   {
		int index_irmao = lista_registros[index_pai].filhos[i];
		if( strcmp(lista_registros[index_irmao].name,lista_registros[index_filho].name) == 0) {


  		return -1;
		}
           }
    }


    return 0;
}


int PegaIndexDoDiretorioAtual()
{
 int i = 0;
 for(i = 0; i < MAX_BLOCOS; i++)
    {
      if(strcmp(lista_registros[i].pathName,currentPath) == 0 && lista_registros[i].fileType == ARQUIVO_DIRETORIO)
         {

            return i;
         }
    }

 return -1;
}

int writeListaRegistrosNoDisco(){
	unsigned char buffer[SECTOR_SIZE];
	limpa_buffer(buffer);

	int setorInicial = SETOR_INICIO_LISTA_REGISTROS;

        int contadorEscritos = 0;
	int index = 0;
	for(index = 0; index < index_registros ; index++)
	{

	   writeRegistroNoSetor(index, setorInicial + contadorEscritos);
	   contadorEscritos++;
	
	}

	sprintf ((char *)buffer, "%i",contadorEscritos);
 	if (write_sector(3,buffer) != 0) return -16; // Escreve no Setor 3 o tamanho em Setores da Lista de registros;

	limpa_buffer(buffer);

	//if (AtualizaListaRegistros() < 0) return -17;

	return 0;
}


int writeRegistroNoSetor(int indice, int setor)
{	unsigned char buffer[SECTOR_SIZE];
	limpa_buffer(buffer);

	unsigned char aux[SECTOR_SIZE];
	limpa_buffer(aux);

	Registro reg = lista_registros[indice];

	//Name

	strncat((char*)buffer,reg.name,MAX_FILE_NAME_SIZE);
	
	append((char*)buffer,'$');

	//FileName	

	strncat((char*)buffer,reg.pathName,MAX_FILE_NAME_SIZE);
	append((char*)buffer,'$');

	//FileType

	sprintf ((char*)aux, "%i",reg.fileType);
	strncat((char*)buffer,(char*)aux,sizeof(DWORD));
	limpa_buffer(aux);

	append((char*)buffer,'$');

	//FileSize

	sprintf ((char *)aux, "%i",reg.fileSize);
	strncat((char*)buffer,(char*)aux,sizeof(DWORD));
	limpa_buffer(aux);

	append((char*)buffer,'$');

	//Bloco Inicial

	sprintf ((char *)aux, "%i",reg.blocoInicial);
	strncat((char*)buffer,(char*)aux,sizeof(DWORD));
	limpa_buffer(aux);

	append((char*)buffer,'$');

	//Numero de Blocos

	sprintf ((char *)aux, "%i",reg.numeroDeBlocos);
	strncat((char*)buffer,(char*)aux,sizeof(DWORD));
	limpa_buffer(aux);

	append((char*)buffer,'$');

	//Salvar Filhos
	int i;
	for(i = 0; i < MAX_FILHOS;i++)
		{
		  append((char*)aux,(char)lista_registros[indice].filhos[i] + '0');
		  
		}

	strncat((char*)buffer,(char*)aux,MAX_FILHOS);
	append((char*)buffer,'$');

	if (write_sector(setor,buffer) != 0) return -14;


	return 0;
}

int readListaRegistrosNoDisco(){
	unsigned char buffer[SECTOR_SIZE];
	limpa_buffer(buffer);

	int setorInicial = SETOR_INICIO_LISTA_REGISTROS;

	if (read_sector(3,buffer) != 0) return -16; // Le no Setor 3 o numero de Setores da Lista de registros;
	int num_registros;

	sscanf((char *)buffer,"%i", &num_registros);

	//EsvaziaListaRegistros()

	int index = 0;
	for(index = 0; index < num_registros ; index++)
	{	
		limpa_buffer(buffer);
		if (read_sector(setorInicial + index,buffer) != 0) return -17;
		readRegistroNoSetor(index,buffer);
		index_registros++;
	}

	return 0;
}

int readRegistroNoSetor(int indice, unsigned char* buffer)
{
	//printf("\nesse é o buffer :%s",buffer);
	unsigned char aux[SECTOR_SIZE];
	limpa_buffer(aux);

	int indice_separador = 0;
	int max_separadores = 7;
	int separadores[7] = {0};	
	int i;
	for (i = 0; i < TAMANHO_SETOR || indice_separador == max_separadores; i++)
	    {
		if (buffer[i] == '$')
			{
			   separadores[indice_separador] = i;
			   indice_separador++;
			}
	    }
	
	//Escreve Name
	limpa_buffer((unsigned char *)lista_registros[indice].name);
	for (i=0;i < separadores[0];i++)
	    {
		append(lista_registros[indice].name,buffer[i]);
	    }

	//printf("\nNome: %s", lista_registros[indice].name);

	//Escreve Pathname
	limpa_buffer( (unsigned char *) lista_registros[indice].pathName);
	for (i=separadores[0] + 1;i < separadores[1];i++)
	    {
		append(lista_registros[indice].pathName,buffer[i]);
	    }
	
        
	//printf("\nPathName: %s", lista_registros[indice].pathName);

	//Escreve fileTypr
	char strFileType[5];
	for (i=separadores[1] + 1;i < separadores[2];i++)
	    {
		append(strFileType,buffer[i]);
	    }
	
	sscanf((char *)strFileType, "%i", &lista_registros[indice].fileType);	

	//printf("\nTipo Arquivo: %d", lista_registros[indice].fileType);


	//Escreve fileSize
	char strFileSize[5];
	for (i=separadores[2] + 1;i < separadores[3];i++)
	    {
		append(strFileSize,buffer[i]);
	    }
	
	sscanf((char *)strFileSize, "%i", &lista_registros[indice].fileSize);	

	//printf("\nTam Arquivo: %d", lista_registros[indice].fileSize);

	//Escreve blocoInicial
	char strBlocoInicial[5];
	for (i=separadores[4] + 1;i < separadores[5];i++)
	    {
		append(strBlocoInicial,buffer[i]);
	    }
	
	sscanf((char *)strBlocoInicial, "%i", &lista_registros[indice].blocoInicial);	

	//printf("\nBloco Inicial: %d", lista_registros[indice].blocoInicial);

	//Escreve numeroDeBlocos
	char strNumeroDeBlocos[5];
	for (i=separadores[4] + 1;i < separadores[5];i++)
	    {
		append(strNumeroDeBlocos,buffer[i]);
	    }
	
	sscanf((char *)strNumeroDeBlocos, "%i", &lista_registros[indice].numeroDeBlocos);

	//printf("\nNumero de Blocos: %d", lista_registros[indice].numeroDeBlocos);

	//Escreve Filhos
	int contFilho = 0;
	//printf(" separa %d",separadores[5]);
	//	printf("\nFilhos");

	for (i=separadores[5] + 1;i < separadores[6];i++)
	    {	
		lista_registros[indice].filhos[contFilho] = buffer[i] - '0';
			
		contFilho++;
	    }
		
	return 0;
}


void writeDwordOnBuffer(unsigned char *buffer, int start, DWORD dword){
	unsigned char *aux;
	aux = (unsigned char*)&dword;
	int i;
	for(i = 0; i < 4; i++)
		buffer[start + i] = aux[i];
}


void append(char* s, char c)
{
        int len = strlen(s);
        s[len] = c;
        s[len+1] = '\0';
}

void LimpaListaRegistros()
{
  int i;
  for(i = 0; i < index_registros; i++)
	{
		strncpy(lista_registros[index_registros].name,"\0",1);
		strncpy(lista_registros[index_registros].pathName,"\0",1);
		lista_registros[index_registros].fileType = INVALID_PTR;
		lista_registros[index_registros].blocoInicial = 0;
		lista_registros[index_registros].numeroDeBlocos = 0;
		
		int j = 0;
		for(j = 0; j < MAX_FILHOS; j++)
			{
				lista_registros[index_registros].filhos[j] = 0;
			}

	}
	index_registros = 0;
}

void LimpaOpenFiles()
{
  int i;
  for(i = 0; i < MAX_OPEN_FILES; i++)
	{

		strncpy(arquivos_abertos[i].registro.name,"\0",1);
		strncpy(arquivos_abertos[i].registro.pathName,"\0",1);
		arquivos_abertos[i].registro.fileType = INVALID_PTR;
		arquivos_abertos[i].registro.blocoInicial = 0;
		arquivos_abertos[i].registro.numeroDeBlocos = 0;
		arquivos_abertos[i].currentPointer = 0;
		int j = 0;
		for(j = 0; j < MAX_FILHOS; j++)
			{
				arquivos_abertos[i].registro.filhos[j] = 0;
			}

	}

}

