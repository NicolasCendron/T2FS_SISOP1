
/**
*/
#include "t2fs.h"

int tamanho_setor = 256;
int tamanho_bloco = 0;
unsigned char buffer_setor[tamanho_setor];

typedef struct DIRENT2 Registro


/*-----------------------------------------------------------------------------
Função:	Informa a identificação dos desenvolvedores do T2FS.
-----------------------------------------------------------------------------*/
int identify2 (char *name, int size) {
	inicializaT2FS();

	char *group = "Nomes e Numeros";
	if(size < strlen(group)){
		printf("faltou espaço\n");
		return -1;
	}
	strncpy(name, group, strlen(group)+1);

	return 0;
}

/*-----------------------------------------------------------------------------
Função:	Formata logicamente o disco virtual t2fs_disk.dat para o sistema de
		arquivos T2FS definido usando blocos de dados de tamanho 
		corresponde a um múltiplo de setores dados por sectors_per_block.
-----------------------------------------------------------------------------*/
int format2 (int sectors_per_block) {
	tamanho_bloco = tamanho_setor * sectors_per_block;
	
	if(read_sector(0, buffer_setor) != 0){
		printf("Erro: Falha ao ler setor 0!\n");
		return -1;
	}

	 int inicioParticao =  *( (WORD*)(buffer_setor + 40) )
	 int fimParticao = inicioParticao + 200 *tamanho_bloco;

	 *WORD end_partition = *( (WORD*)(buffer_setor + 44) )

	 *end_partition = (WORD*)fimParticao;

	 write_sector(0,buffer_setor);


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
    
    Registro registro;

    char nomeArquivo[MAX_FILE_NAME_SIZE + 1];

    getLastDir(); // TODO

    if (VerificaSeNomeJaExiste(filename) != 0)
    {
    	TrataNomesDuplicados(filename);
    	
    }
    strncpy(registro.name,nomeArquivo);
    registro.....


	return open2(filename);
}

/*-----------------------------------------------------------------------------
Função:	Função usada para remover (apagar) um arquivo do disco. 
-----------------------------------------------------------------------------*/
int delete2 (char *filename) {
	return -1;
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
	if(getRecordFromPath(filename,&registro) != 0)
	{
		return -1;
	}

	if(registro.fileType == ARQUIVO_REGULAR)
	{
		arquivos_abertos[freeHandle].registro = registro;
		arquivos_abertos[freeHandle].currentPointer = 0;
		return freeHandle;
	}

	return -1;

}

/*-----------------------------------------------------------------------------
Função:	Função usada para fechar um arquivo.
-----------------------------------------------------------------------------*/
int close2 (FILE2 handle) {
	inicializaT2FS();
	if(fileHandValido(handle))
	{
		arquivos_abertos[hahdle].registro.fileType = INVALID_PTR;
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

	OpenFile file;
	int numBytes;

	if(isFileHandleValid(handle))
	{
		file = arquivos_abertos[handle];

		if(writeBytesOnFile(file.currentPointer... ))
		{
			file.currentPointer += numBytes;
			arquivos_abertos[handle] = file;
			return numBytes;
		}
		return -1;
	}
	return -1;
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
			file.currentPointer = registro.fileSize;
		}
		arquivos_abertos[handle] = file;
		return 0;
	}

	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um novo diretório.
-----------------------------------------------------------------------------*/
int mkdir2 (char *pathname) {
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
	if(pegaRegistroPeloPath(pathname,&registro) != 0)
	{
		return -1; // Deu problema
	}

	diretorios_abertos[freeHandle].record = record;
	diretorios_abertos[freeHandle].currentPointer = 0;

	return freeHandle;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para ler as entradas de um diretório.
-----------------------------------------------------------------------------*/
int readdir2 (DIR2 handle, DIRENT2 *dentry) {
	Registro registro;

	if(!isDirHandleValid(handle))
	{
		return -1;
	}

	arquivos_abertos[handle].currentPointer++;

	if(registro.fileType == INVALID_PTR)
	{
		return readdir2(handle, dentry);
	}

// Conctinua

}

/*-----------------------------------------------------------------------------
Função:	Função usada para fechar um diretório.
-----------------------------------------------------------------------------*/
int closedir2 (DIR2 handle) {
	inicializaT2FS();

	if(!isDirHandleVakid(handle)){
		return -1;
	}

	diretorios_abertos[handle].record.fileType = INVALID_PTR;

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

BOOL inicializado = FALSE;
int bloco_atual = 0;
char currentPath[200];

#define MAX_OPEN_FILES = 200;
#define MAX_OPEN_DIRS = 200;
void inicializaT2FS()
{
	if(inicializado)
		return;

	inicializaArquivosAbertos();
	inicializaDiretoriosAbertos();

	bloco_atual = 0;
	strcpy(currentPath, "/\0");

	inicializado = TRUE;
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
		if(arquivos_abertos[freeHandle].registro.fileType == INVALID_PTR);
			return freeHandle;
	}
}

FILE2 getFreeDirHandle(){
	DIR2 freeHandle;
	for(freeHandle = 0; freeHandle < MAX_OPEN_DIRS; freeHandle++)
	{
		if(diretorios_abertos[freeHandle].registro.fileType == INVALID_PTR);
			return freeHandle;
	}
}

BOOL isFileHandleValid(FILE2 handle){
	if(handle < 0 || handle >= MAX_OPEN_FILES || arquivos_abertos[handle].record.fileType != ARQUIVO_REGULAR)
		return FALSE;
	else
		return TRUE;
}

BOOL isDirHandleValid(DIR2 handle){
	if(handle < 0 || handle >= MAX_OPEN_DIR || diretorios_abertos[handle].record.fileType != ARQUIVO_DIRETORIO)
		return FALSE;
	else
		return TRUE;
}
