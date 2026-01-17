/*
*  myfs.c - Implementacao do sistema de arquivos MyFS
*
*  Autores: Cauã Moreno, Estêvão Barbosa Fiorilo da Rocha, Rhara Ianna, Yan
*  Projeto: Trabalho Pratico II - Sistemas Operacionais
*  Organizacao: Universidade Federal de Juiz de Fora
*  Departamento: Dep. Ciencia da Computacao
*
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "myfs.h"
#include "disk.h"
#include "vfs.h"
#include "inode.h"
#include "util.h"

//Declaracoes globais
//...
//...
#define MYFS_ID 0x00
#define MYFS_NAME "TrabalhoSO"
#define MYFS_MAGIC_NUMBER "CRRY"
#define FILE_NAME_MAX_LENGTH 28


// Ponteiros iniciam como NULL, índices como -1, flags como 0
int fsMounted = 0;        // 0 = desmontado, 1 = montado
Disk *currentDisk = NULL;   // disco atualmente montado

unsigned int sectorsPerBlock = 0;
unsigned int numSectors = 0;
unsigned int numBlocks = 0;

unsigned int bitmapStart = -1;        // setor inicial do bitmap
unsigned int bitmapTableSectors = 0;      // quantos setores o bitmap ocupa
unsigned int bitmapSizeInBytes = 0; // tamanho do bitmap em bits

unsigned int inodeSize = 64; 
unsigned int numInodes = 0;          // numero total de inodes
unsigned int inodeStart = -1;         // setor inicial da tabela de inodes
unsigned int inodeTableSectors;  // tamanho da tabela de inodes

unsigned int dataStart = -1;          // setor inicial dos dados
unsigned char *bitmapCache = NULL;
unsigned char *rootInodeCache = NULL;
unsigned char *superblockCache = NULL;
int filesOpened = -1; // -1 indica sistema nao montado

unsigned int fsMetadataSizeInSectors = 0;

typedef struct {
	char name[FILE_NAME_MAX_LENGTH];
	unsigned int inode;
} RootEntry;

typedef struct { 
	Inode *inode;
	unsigned int posicaoCursor;
	unsigned int emUso;
} DescritorArquivo;

DescritorArquivo tabelaAbertos[MAX_FDS] = {0}; //guarda as informações do arquivo enquanto ele estiver aberto

//...

//Conversao de Sector para Block
unsigned int sectorToBlock(unsigned int sector) {
    return sector / sectorsPerBlock;
}

//Conversao de Block para Sector
unsigned int blockToSector(unsigned int block, unsigned int offset) {
    return block * sectorsPerBlock + offset;
}

//Funcao que recebe um setor como parametro e retorna se ele esta vazio
//Retorna 0 caso nao esteja vazio, positivo caso vazio
//POR ENQUANTO ESTOU VERIFICANDO SE E TUDO ZERO MAS TEM QUE CONFERIR
//POIS A FORMATACAO PODE CAUSAR COM QUE SEJA DIFERNETE E PRECISA
//SEGUIR A LOGICA CONSISTENTEMENTE
int isSectorEmpty (const char sector[DISK_SECTORDATASIZE]) {
	for (int i=0; i < DISK_SECTORDATASIZE; i++) {
		if (sector[i]!=0) {
			return 0;
		}
	}
	return 1;
}

//Funcao para verificar se um bloco esta livre no bitmap
//Retorna positivo se livre, 0 se ocupado
int isBlockFree(unsigned char *bitmap, int block)
{
	int byteIndex = block / 8;
	int bitIndex  = block % 8;
	return (bitmap[byteIndex] & (1 << (7 - bitIndex))) == 0;
}

//Funcao para alocar blocos no bitmap, se baseando num intervalo de dois blocos
//Ambos definidos por parametro. Retorna 0 se alocacao bem sucedida ou
int allocateBlocksInBitmap(unsigned char *bitmap, int startBlock,int endBlock)
{
	//Verificação
	for (int s = startBlock; s <= endBlock; s++) {
		if (!isBlockFree(bitmap, s)) {
			return -1; // setor ocupado
		}
	}

	//Marcação
	for (int s = startBlock; s <= endBlock; s++) {
		int byteIndex = s / 8;
		int bitIndex  = s % 8;
		bitmap[byteIndex] |= (1 << (7 - bitIndex));
	}

	return 0;
}

//Funcao para verificacao se o sistema de arquivos está ocioso, ou seja,
//se nao ha quisquer descritores de arquivos em uso atualmente. Retorna
//um positivo se ocioso ou, caso contrario, 0.
int myFSIsIdle (Disk *d) {
	if (filesOpened > 0)
		return 0;
	return 1;
}

//Funcao para formatacao de um disco com o novo sistema de arquivos
//com tamanho de blocos igual a blockSize. Retorna o numero total de
//blocos disponiveis no disco, se formatado com sucesso. Caso contrario,
//retorna -1.
int myFSFormat (Disk *d, unsigned int blockSize) {

	//Um monte de Dados
		numSectors = diskGetNumSectors(d);

		sectorsPerBlock = blockSize / DISK_SECTORDATASIZE;
		
		// Determinar quantos Inodes queremos (1 para cada 16KB de disco -> nn sei pq nn, alguem falou q era o certo)
		numInodes = (numSectors * DISK_SECTORDATASIZE) / (16 * 1024);
		inodeTableSectors = (numInodes * inodeSize + DISK_SECTORDATASIZE - 1) / DISK_SECTORDATASIZE;
		
		// Bitmap: 1 bit por bloco
		numBlocks = numSectors / sectorsPerBlock;
		unsigned int bitsPerSector = DISK_SECTORDATASIZE * 8;
		unsigned int totalBytesNeeded = (numBlocks + 7) / 8;
		bitmapTableSectors = (totalBytesNeeded + DISK_SECTORDATASIZE - 1)/ DISK_SECTORDATASIZE;
		bitmapSizeInBytes = bitmapTableSectors * DISK_SECTORDATASIZE;
		


		
		
		// Aqui começa dvdd
		unsigned char emptySectors[DISK_SECTORDATASIZE] = {0};
		diskWriteSector(d, 0, emptySectors); // zera o primeiro setor
		unsigned char superbloco[DISK_SECTORDATASIZE] = {0};
		memcpy(superbloco, MYFS_MAGIC_NUMBER, 4);
		
		//vetor temporário só p fzr os calculos para char
		unsigned char temp[4] = {0};
		
		ul2char(sectorsPerBlock,temp);
		for (int i=0; i<4; i++) {
			superbloco[4+i]=temp[i];
		}
		
		//Zera Temp
		memset(temp, 0, sizeof(temp));
		
		unsigned int diskSizeInSectors = diskGetNumSectors(d); //tam disco
		ul2char(diskSizeInSectors,temp);
		for (int i=0; i<4; i++) {
			superbloco[8+i]=temp[i];
		}
		diskWriteSector(d, 0, superbloco); //finaliza superbloco escrevendo
		
		
		
	// INICIALIZAÇÃO DO BITMAP

		bitmapStart = 1;
		inodeStart = bitmapStart + bitmapTableSectors;
		dataStart = inodeStart + inodeTableSectors;

		unsigned int totalBlocksToReserve = (dataStart + sectorsPerBlock - 1) / sectorsPerBlock;

		//O cache deve ser múltiplo de DISK_SECTORDATASIZE para o diskWriteSector não quebrar
		unsigned int bitmapCacheSize = bitmapTableSectors * DISK_SECTORDATASIZE;
		bitmapCache = (unsigned char *) calloc(1, bitmapCacheSize);

		if (bitmapCache == NULL) return -1;

		// Marcamos os blocos de metadados como ocupados
		allocateBlocksInBitmap(bitmapCache, 0, totalBlocksToReserve - 1);

		// Escrita do Bitmap no Disco
		for (unsigned int i = 0; i < bitmapTableSectors; i++) {
			diskWriteSector(d, bitmapStart + i, &bitmapCache[i * DISK_SECTORDATASIZE]);
		}
        
	// FIM DO BITMAP


    // INICIALIZAÇÃO DA TABELA DE INODES

		//tabela de inodes  = numero de inodes x tamanho de inode -> tem q ser multiplo de setor pra passar dps pro disco
		inodeTableSectors = (numInodes * inodeSize + DISK_SECTORDATASIZE - 1) / DISK_SECTORDATASIZE;

		for(unsigned int i=0; i < inodeTableSectors; i++)
			diskWriteSector(d, inodeStart + i, emptySectors);


	//inode

	Inode* root = inodeCreate(1,d);
	if (root) {
        inodeSave(root);
        free(root);
    }

	// Cria todos os outros Inodes (ID 2 até numInodes) como livres
    for (unsigned int i = 2; i <= numInodes; i++) {
        Inode* temp = inodeCreate(i, d);
        if (temp) {
            inodeSave(temp); // Grava o ID correto e a estrutura zerada no disco
            free(temp);// Se não der free, o malloc vai estourar a memória
        }
    }

	// FIM DA TABELA DE INODES
	
    

    return numBlocks;
}

//Funcao para montagem/desmontagem do sistema de arquivos, se possível.
//Na montagem (x=1) e' a chance de se fazer inicializacoes, como carregar
//o superbloco na memoria. Na desmontagem (x=0), quaisquer dados pendentes
//de gravacao devem ser persistidos no disco. Retorna um positivo se a
//montagem ou desmontagem foi bem sucedida ou, caso contrario, 0.
int myFSxMount (Disk *d, int x) {
    if(x==1){
        for(int i = 0; i < fsMetadataSizeInSectors; i++) {
            unsigned char information_sector[DISK_SECTORDATASIZE] = {0};
            diskReadSector(d, i, information_sector);
			switch (i) {
				case 0:
					//verificacao do superbloco
					if (i == 0) {
						for (int j=0; j<4; j++) {
							unsigned char numMagico[4] = MYFS_MAGIC_NUMBER;
							if (information_sector[j]!=numMagico[j]) {
								return 0;
							}
						}
					}
					//escreve na memoria caso seja valido
					memcpy(superblockCache, information_sector, DISK_SECTORDATASIZE);
					break;
				case 1:
					bitmapCache = (unsigned char *) malloc(bitmapSizeInBytes);
					memcpy(bitmapCache, information_sector, DISK_SECTORDATASIZE);
					break;
				case 2:
					//verificar se existe indoe raiz aqui
					rootInodeCache = (unsigned char *) malloc(16);
					memcpy(rootInodeCache, information_sector, 16);
					break;
				default:
					break;
			}
        }
    	filesOpened=0;
		fsMounted=1;
		currentDisk=d;
    	return 1;
    }
	if (x==0) {
		diskWriteSector(d, 0, superblockCache);
		free(superblockCache);
		for (int i=bitmapStart; i < (bitmapTableSectors); i++) {
			unsigned char setorBitmap[DISK_SECTORDATASIZE] = {0};
			memcpy(setorBitmap, &bitmapCache[(i - bitmapStart) * DISK_SECTORDATASIZE], DISK_SECTORDATASIZE);
			diskWriteSector(d, i, setorBitmap);
		}
		for (int i=inodeStart; i < (inodeTableSectors); i++) {
			unsigned char setorInode[DISK_SECTORDATASIZE] = {0};
			memcpy(setorInode, &rootInodeCache[(i - inodeStart) * DISK_SECTORDATASIZE], DISK_SECTORDATASIZE);
			diskWriteSector(d, i, setorInode);
		}
		filesOpened=-1;
		currentDisk=NULL;
		fsMounted=0;
		return 1;
	}
	return 0;
}

//Funcao para abertura de um arquivo, a partir do caminho especificado
//em path, no disco montado especificado em d, no modo Read/Write,
//criando o arquivo se nao existir. Retorna um descritor de arquivo,
//em caso de sucesso. Retorna -1, caso contrario.
int myFSOpen (Disk *d, const char *path) {
	// Verifica se o maximo de arquivos abertos foi atingido
	if (filesOpened >= MAX_FDS) return -1;

	// Carrega arquivos a partir do rootInode
	Inode* rootInode = inodeLoad(1, d);
	unsigned char cacheSetor[DISK_SECTORDATASIZE] = {0};

	unsigned int blockRoot = inodeGetBlockAddr(rootInode, 0);
	unsigned int sectorRoot = blockToSector(blockRoot, 0);

	int leitura = diskReadSector(d, sectorRoot, cacheSetor);
	if (leitura == -1) {
		free(rootInode);
		return -1;
	}

	RootEntry*entradaRaiz = (RootEntry*) cacheSetor;
	int qtdEntradas = DISK_SECTORDATASIZE / sizeof(RootEntry);

	// Procura o arquivo pelo nome
	for (int i=0; i < qtdEntradas; i++){
		char nomeTemp[FILE_NAME_MAX_LENGTH+1]; // FILE_NAME_MAX_LENGTH chars + '\0'
		strncpy(nomeTemp, entradaRaiz[i].name, FILE_NAME_MAX_LENGTH);
		nomeTemp[FILE_NAME_MAX_LENGTH] = '\0';

		if (strcmp(nomeTemp, path+1) == 0){ 
			int numInodeEncontrado;
			// Usando a estrutura para acessar o campo inode
			numInodeEncontrado = entradaRaiz[i].inode;

			// Carregando pra memoria
			Inode* arquivoInode = inodeLoad(numInodeEncontrado, d);
			
			// Registrando na tabela de de arquivos abertos
			for (int j=1; j <= MAX_FDS; j++){
				if (tabelaAbertos[j-1].emUso == 0){
					tabelaAbertos[j-1].inode = arquivoInode;
					tabelaAbertos[j-1].posicaoCursor = 0;
					tabelaAbertos[j-1].emUso = 1;

					filesOpened++;
					free(rootInode);
					return j;
				}
			}
		}
	}

	// Cria arquivo se não encontrar (Apenas Inode e Diretório, sem bloco de dados inicial)
	// Procura por uma entrada vazia no diretório
	int vagaNoDiretorio = -1;
	for (int j=0; j < qtdEntradas; j++){
		if (entradaRaiz[j].name[0] == '\0') {
			vagaNoDiretorio = j;
			break;
		}
	}

	if (vagaNoDiretorio == -1) {
		free(rootInode);
		return -1; // Diretório cheio
	}
	
	// Cria o Inode
	unsigned int inodeNumber = inodeFindFreeInode(inodeStart, d);

	Inode* novoInode = inodeCreate(inodeNumber, d);
	
	if (novoInode == NULL) {
		free(rootInode);
		return -1; // Erro na criação do Inode
	}
	inodeSave(novoInode);
	
	// Atualiza o Diretório Raiz
	entradaRaiz[vagaNoDiretorio].inode = inodeNumber;
	strncpy(entradaRaiz[vagaNoDiretorio].name, path+1, FILE_NAME_MAX_LENGTH);
	entradaRaiz[vagaNoDiretorio].name[FILE_NAME_MAX_LENGTH - 1] = '\0';
	
	// Salva o diretório no disco
	diskWriteSector(d, sectorRoot, cacheSetor);
	
	// Abre o arquivo (Coloca na tabela)
	for (int k=1; k <= MAX_FDS; k++){
		if (tabelaAbertos[k-1].emUso == 0){
			tabelaAbertos[k-1].inode = novoInode;
			tabelaAbertos[k-1].posicaoCursor = 0;
			tabelaAbertos[k-1].emUso = 1;
			
			filesOpened++;
			free(rootInode);

			for (int m = 0; m < MAX_FDS; m++) {
				printf("Entrada %d: Uso=%d, Inode=%p, Cursor=%d \n", m+1, tabelaAbertos[m].emUso, tabelaAbertos[m].inode, tabelaAbertos[m].posicaoCursor);
			}
			printf("filesOpened: %d", filesOpened);

			return k;
		}
	}
	
	free(rootInode);
	free(novoInode);
	return -1;
}
	
//Funcao para a leitura de um arquivo, a partir de um descritor de arquivo
//existente. Os dados devem ser lidos a partir da posicao atual do cursor
//e copiados para buf. Terao tamanho maximo de nbytes. Ao fim, o cursor
//deve ter posicao atualizada para que a proxima operacao ocorra a partir
//do próximo byte apos o ultimo lido. Retorna o numero de bytes
//efetivamente lidos em caso de sucesso ou -1, caso contrario.
int myFSRead (int fd, char *buf, unsigned int nbytes) {
	return -1;
}

char* intParaBinario(int n) {
	char *bin = malloc(9 * sizeof(char)); // 9 bits + '\0'
	if (bin == NULL) return NULL;

	int i = 0;

	if (n == 0) {
		bin[i++] = '0';
	} else {
		while (n > 0) {
			bin[i++] = (n % 2) + '0';
			n /= 2;
		}
	}

	bin[i] = '\0';

	// inverte
	for (int j = 0; j < i / 2; j++) {
		char tmp = bin[j];
		bin[j] = bin[i - j - 1];
		bin[i - j - 1] = tmp;
	}

	return bin;
}

//Funcao que acha um bloco livre, cria o bloco no inode e retorna o endereco desse bloco
//Retorna -1 se falso
int createInodeBlock(Inode* inode)
{
	for (int i = 1; i < bitmapSizeInBytes; i++)
	{
		for (int j = 0; j < 8; j++)
		{
			if ((bitmapCache[i] & (1 << j)) == 0)
			{

				int blockAddr = (i * 8) + j;
				if (blockAddr == 0){continue;}
				printf("%d",blockAddr);
				allocateBlocksInBitmap(bitmapCache, blockAddr, blockAddr);
				inodeAddBlock(inode, blockAddr);

				return blockAddr;
			}
		}
	}
	return -1;
}

//Funcao para a escrita de um arquivo, a partir de um descritor de arquivo
//existente. Os dados de buf sao copiados para o disco a partir da posição
//atual do cursor e terao tamanho maximo de nbytes. Ao fim, o cursor deve
//ter posicao atualizada para que a proxima operacao ocorra a partir do
//proximo byte apos o ultimo escrito. Retorna o numero de bytes
//efetivamente escritos em caso de sucesso ou -1, caso contrario
int myFSWrite (int fd, const char *buf, unsigned int nbytes) {
	if (fd < 0 || fd >= MAX_FDS || tabelaAbertos[fd].emUso == 0) return -1;

	DescritorArquivo* arquivo = &tabelaAbertos[fd];
	Inode* iNodeAtual = arquivo->inode;
	int bytesEscritos = 0;

	while (bytesEscritos < nbytes) {
		int logicalBlock = arquivo->posicaoCursor / (DISK_SECTORDATASIZE * sectorsPerBlock);
		int offsetNoBloco = arquivo->posicaoCursor % (DISK_SECTORDATASIZE * sectorsPerBlock);
		int sectorNoBloco = offsetNoBloco / DISK_SECTORDATASIZE;
		int offsetNoSetor = offsetNoBloco % DISK_SECTORDATASIZE;

		int blockAddr = inodeGetBlockAddr(iNodeAtual, logicalBlock);

		int sectorToWrite;
		if (blockAddr == 0) {
			blockAddr = createInodeBlock(iNodeAtual);
			if (blockAddr == -1) return -1;
			sectorToWrite = blockToSector(blockAddr, sectorNoBloco);
		}
		else
		{
			sectorToWrite = blockToSector(blockAddr, sectorNoBloco);
		}

		unsigned char infoToWrite[DISK_SECTORDATASIZE];
		diskReadSector(currentDisk, sectorToWrite, infoToWrite);

		while (offsetNoSetor < DISK_SECTORDATASIZE && bytesEscritos < nbytes) {
			infoToWrite[offsetNoSetor] = buf[bytesEscritos];
			offsetNoSetor++;
			bytesEscritos++;
			arquivo->posicaoCursor++;;
		}

		diskWriteSector(currentDisk, sectorToWrite, infoToWrite);
		allocateBlocksInBitmap(bitmapCache, blockAddr, blockAddr);
	}

	// Salva o Inode atualizado no disco (metadados como tamanho e blocos novos)
	inodeSave(iNodeAtual);
	return bytesEscritos;
}

//Funcao para fechar um arquivo, a partir de um descritor de arquivo
//existente. Retorna 0 caso bem sucedido, ou -1 caso contrario
int myFSClose (int fd) {

	if(filesOpened <= 0 || fd < 1 || fd > MAX_FDS){
		return -1;
	}
	
	printf("filesOpened: %d", filesOpened);
	if (tabelaAbertos[fd-1].emUso == 1){
		tabelaAbertos[fd-1].inode = NULL;
		tabelaAbertos[fd-1].posicaoCursor = 0;
		tabelaAbertos[fd-1].emUso = 0;
		
		filesOpened--;
		for (int m = 0; m < MAX_FDS; m++) {
				printf("Entrada %d: Uso=%d, Inode=%p, Cursor=%d \n", m+1, tabelaAbertos[m].emUso, tabelaAbertos[m].inode, tabelaAbertos[m].posicaoCursor);
		}
		printf("filesOpened: %d", filesOpened);
		return 0;
	}

	
	return -1;
}

//Funcao para instalar seu sistema de arquivos no S.O., registrando-o junto
//ao virtual FS (vfs). Retorna um identificador unico (slot), caso
//o sistema de arquivos tenha sido registrado com sucesso.
//Caso contrario, retorna -1

int installMyFS (void) {
	if (superblockCache == NULL) {
		superblockCache = (unsigned char *) malloc(DISK_SECTORDATASIZE);
	}

	// Reservamos um espacinho na memória para FSInfo
	FSInfo *info = (FSInfo *) malloc(sizeof(FSInfo));
    
    // Se não conseguirmos memória, deu erro
    if (info == NULL) return -1;

    // Dados básicos sistema
    info->fsid = 0x00; 
    info->fsname = "TrabalhoSO";

    // "Apontamos" para as funções 
    // SO aprende quais funções chamar quando precisar
    info->isidleFn  = myFSIsIdle;
    info->formatFn  = myFSFormat;
    info->xMountFn   = myFSxMount;
    info->openFn    = myFSOpen;
    info->readFn    = myFSRead;
    info->writeFn   = myFSWrite;
    info->closeFn   = myFSClose;

    // Entregamos o formulário para o VFS 
    // Ele vai retornar o número da "vaga" q ele deu
    return vfsRegisterFS(info);
}
