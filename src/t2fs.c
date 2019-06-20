
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
FILE2 getFreeFileHandle();
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
/* Interface Com o Disco */
void writeBlock(int, char*); //Não esta sendo usada
int PegaInformacoesDoDisco();
int writeListaRegistrosNoDisco();
void writeDwordOnBuffer(unsigned char *, int, DWORD);
int writeRegistroNoSetor(int indice, int setor);
/* Regras de Negocio */
int VerificaSeJaTemIrmaoComMesmoNome(int,int);

/* Utilitarios */
void limpa_buffer( unsigned char buffer[]);
void append(char*, char);
int contaQuantosFilhos(int indice);
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
	//Colocar nesse buffer o Registro do ROOT

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

	// Falta ainda criar a lista de Registros;

	// Esvazia o resto do disco; Se adicionar mais coisas atualize o inicio do I
	limpa_buffer(buffer);
	for (i = 4; i < 4050; i++)
	{

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

    //PathName precisa ser carregado do pai; Tem que ver as barrinhas

    strncpy(lista_registros[index_registros].name,filename,MAX_FILE_NAME_SIZE - 1);

    strncpy(lista_registros[index_registros].pathName,currentPath,MAX_FILE_NAME_SIZE - 1);
     if(strcmp(currentPath,"/") != 0)
	{
	  strcat(lista_registros[index_registros].pathName,"/");
	}

    strcat(lista_registros[index_registros].pathName,filename);

    printf("\n nome: %s",lista_registros[index_registros].name);
    printf("\n pathName: %s\n",lista_registros[index_registros].pathName);


    lista_registros[index_registros].fileType = ARQUIVO_REGULAR;
    lista_registros[index_registros].blocoInicial = index_registros;
    lista_registros[index_registros].numeroDeBlocos = 1;

    //Tem que adicionar o arquivo na lista de filhos do pai (pai é o currentPath)

   if( ColocaComoFilhoDoPai(index_registros) < 0) {
	lista_registros[index_registros].fileType = INVALID_PTR;
        printf("\nNão foi possivel colocar como filho\n");
        return -1;}

    index_registros++;

    //Tem que colocar o arquivo num bloco de disco
    //Tem que Atualizar o Vetor de Bits
	int i;
	for(i = 0; i < MAX_BLOCOS; i++){
		if(bitMap.bits[i] == 1) { //Bloco está livre
			bitMap.bits[i] = 0; //Está ocupado.
			bitMap.lastWrittenIndex = i; //Salva ultimo index usado.
			break; //Sai do loop
		}
	}

    //writeBlock()

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
	FILE2 freeHandle = getFreeFileHandle();
	if(freeHandle == -1)
		return -1;


	int index_arquivo = EncontraRegistroFilhoPeloNome(filename,ARQUIVO_REGULAR);


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
	OpenFile file;
	
	if(isFileHandleValid(handle))
	{
		file = arquivos_abertos[handle].registro;
		
		if(file.registro.fileType == ARQUIVO_REGULAR)
		{
			write_sector(file.currentPointer,&buffer); //Escreve na pos o que esta no buffer
			
			file.currentPointer += size + 1; // Atualiza o contador de posição
			
			arquivos_abertos[handle] = file;
			
			return size; // Retorna numero de bytes
		
		}
		return -2;
	}
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

    printf("%s",pathname);

    strncpy(lista_registros[index_registros].name,pathname,MAX_FILE_NAME_SIZE - 1);

    strncpy(lista_registros[index_registros].pathName,currentPath,MAX_FILE_NAME_SIZE - 1);
    if(strcmp(currentPath,"/") != 0)
	{
	  strcat(lista_registros[index_registros].pathName,"/");
	}

    strcat(lista_registros[index_registros].pathName,pathname);

    lista_registros[index_registros].fileType = ARQUIVO_DIRETORIO;
    lista_registros[index_registros].blocoInicial = index_registros;
    lista_registros[index_registros].numeroDeBlocos = 1;

    printf("\n nome: %s",lista_registros[index_registros].name);
    printf("\n pathName: %s\n",lista_registros[index_registros].pathName);

    if( ColocaComoFilhoDoPai(index_registros) < 0) {
	lista_registros[index_registros].fileType = INVALID_PTR;
        printf("\nNão foi possivel colocar como filho\n");
        return -1;}

    index_registros++;

    //Tem que colocar o diretorio num bloco de disco
    //Tem que atualizar a lista de registros no disco.
    //Tem que Atualizar o Vetor de Bits
	int i;
	for (i = 0; i<MAX_BLOCOS;i++){
		if(bitMap.bits[i] == 1){
			bitMap.bits[i] = 0;
			bitMap.lastWrittenIndex = i;
			break;
		}
	}

    //writeBlock()

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
            if(lista_registros[indice_filho].fileType != INVALID_PTR)
		{
	         strncpy(dentry->name,lista_registros[indice_filho].name,MAX_FILE_NAME_SIZE);
                 dentry->fileType =  (BYTE)lista_registros[indice_filho].fileType;
                 dentry->fileSize =  (DWORD)2; //Só depois
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

	PegaInformacoesDoDisco();

	int i; // DEPOIS VAI VIR TUDO DO DISCO
	for (i = 0; i < MAX_BLOCOS; i++){
		//lista_registros[i].name = malloc(sizeof(char) * MAX_FILE_NAME_SIZE);
		//lista_registros[i].pathName = malloc(sizeof(char) * MAX_FILE_NAME_SIZE);
		lista_registros[i].fileType = INVALID_PTR;

	}

	CriaRegistroDiretorioRoot(); // Depois quando passar pro disco, Vai para o Format


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
    lista_registros[index_registros].blocoInicial = index_registros;
    lista_registros[index_registros].numeroDeBlocos = 1;
    strcpy(currentPath, lista_registros[index_registros].pathName);
    index_registros++;
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

FILE2 getFreeFileHandle(){

	FILE2 freeHandle;
	for(freeHandle = 0; freeHandle < MAX_OPEN_FILES; freeHandle++)
	{
		if(arquivos_abertos[freeHandle].registro.fileType == INVALID_PTR){

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
	        //printf("\nAtual :%s Procurado: %s",lista_registros[cont].pathName,pathName);
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
	for(index = 0; index < MAX_BLOCOS ; index++)
	{
	   if(lista_registros[index].fileType != INVALID_PTR) //Aqui Ja Remove aqueles registros que foram apagados.
		{
		   writeRegistroNoSetor(index, setorInicial + contadorEscritos);
		   contadorEscritos++;
		}

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

	strncat((char*)buffer,reg.name,MAX_FILE_NAME_SIZE);
	append((char*)buffer,'$');
	strncat((char*)buffer,reg.pathName,MAX_FILE_NAME_SIZE);
	append((char*)buffer,'$');

	sprintf ((char*)aux, "%i",reg.fileType);
	strncat((char*)buffer,(char*)aux,sizeof(DWORD));
	limpa_buffer(aux);

	append((char*)buffer,'$');

	sprintf ((char *)aux, "%i",reg.fileSize);
	strncat((char*)buffer,(char*)aux,sizeof(DWORD));
	limpa_buffer(aux);

	append((char*)buffer,'$');

	sprintf ((char *)aux, "%i",reg.blocoInicial);
	strncat((char*)buffer,(char*)aux,sizeof(DWORD));
	limpa_buffer(aux);

	append((char*)buffer,'$');

	sprintf ((char *)aux, "%i",reg.numeroDeBlocos);
	strncat((char*)buffer,(char*)aux,sizeof(DWORD));
	limpa_buffer(aux);

	append((char*)buffer,'$');


	if (write_sector(setor,buffer) != 0) return -14;


	printf("esse é o buffer %s\n",buffer);

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

