/*
*  myfs.c - Implementacao do sistema de arquivos MyFS
*
*  Autores: SUPER_PROGRAMADORES_C
*  Projeto: Trabalho Pratico II - Sistemas Operacionais
*  Organizacao: Universidade Federal de Juiz de Fora
*  Departamento: Dep. Ciencia da Computacao
*
*/

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
#define MAX_INODES 256

// 1 + num_sec_bitmap + ceil(MAX_INODES/8)
#define NUM_SECTORS_HEADER 3

char cache[NUM_SECTORS_HEADER*DISK_SECTORDATASIZE];
int filesOpened = -1; // -1 indica sistema nao montado

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
		unsigned int numSectors = diskGetNumSectors(d);
		unsigned int sectorsPerBlock = blockSize / DISK_SECTORDATASIZE;
		
		// Determinar quantos Inodes queremos (1 para cada 16KB de disco -> nn sei pq nn, alguem falou q era o certo)
		// nn ta considerando os setores de conf ainda
		unsigned int numInodes = (numSectors * DISK_SECTORDATASIZE) / (16 * 1024);
		
		// Bitmap: 1 bit por bloco
		unsigned int numBlocks = numSectors / sectorsPerBlock;
		unsigned int bitmapSectors = (numBlocks / (DISK_SECTORDATASIZE * 8)) + 1;
		
		// Tabela de Inodes: (numInodes * tamanho inode) / tamanho_do_setor
		unsigned int inodeTableSectors = (numInodes * 128 / DISK_SECTORDATASIZE) + 1;

		unsigned int bitmapStart = 1;
		unsigned int inodeStart = bitmapStart + bitmapSectors;
		unsigned int dataStart = inodeStart + inodeTableSectors;
	

	// Aqui começa dvdd
	unsigned char emptySectors[DISK_SECTORDATASIZE] = {0};
	diskWriteSector(d, 0, emptySectors); // zera o primeiro setor
	unsigned char superbloco[DISK_SECTORDATASIZE] = {0};
	superbloco[0]='9';		//numero magico
	superbloco[1]='8';
	superbloco[2]='2';
	superbloco[3]='9';

    //vetor temporário só o fzr os calculos para char
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



    // Zerar Bitmap
    for(unsigned int i=0; i < bitmapSectors; i++) 
        diskWriteSector(d, bitmapStart + i, emptySectors);

    // Zerar Tabela de Inodes
    for(unsigned int i=0; i < inodeTableSectors; i++)
    diskWriteSector(d, inodeStart + i, emptySectors);

    //Falta a logica de como vai ver qq ta ocupado ou nn e colocar no bitmap

    //Fim Dessa lógica


	//fim do bitmap

	//inode

	diskWriteSector(d,2,emptySectors);
	Inode* i = inodeCreate(1,d);
	inodeSave(i);
	//fim inode
	
    

    return numBlocks;
}

//Funcao para montagem/desmontagem do sistema de arquivos, se possível.
//Na montagem (x=1) e' a chance de se fazer inicializacoes, como carregar
//o superbloco na memoria. Na desmontagem (x=0), quaisquer dados pendentes
//de gravacao devem ser persistidos no disco. Retorna um positivo se a
//montagem ou desmontagem foi bem sucedida ou, caso contrario, 0.
int myFSxMount (Disk *d, int x) {
    if(x==1){
        for(int i = 0; i < NUM_SECTORS_HEADER; i++) {
            unsigned char information_sector[DISK_SECTORDATASIZE] = {0};
            diskReadSector(d, i, information_sector);
			//verificacao do superbloco
        	if (i == 0) {
		        for (int j=0; j<4; j++) {
			        unsigned char numMagico[4] = {'9','8','2','9'};
			        if (information_sector[j]!=numMagico[j]) {
						return 0;
					}
        		}
        	}
        	//escreve na memoria caso seja valido
            unsigned int start_sector = i*DISK_SECTORDATASIZE;
            for(int j = 0; j < DISK_SECTORDATASIZE; j++) {
                cache[j + start_sector] = information_sector[j];
            }
        }
    	filesOpened=0;
    	return 1;
    }
	if (x==0) {
		for (int i=0; i < (NUM_SECTORS_HEADER*DISK_SECTORDATASIZE); i++) {
			cache[i] = 0x0;
		}
		filesOpened=-1;
		return 1;
	}
	return 0;
}

//Funcao para abertura de um arquivo, a partir do caminho especificado
//em path, no disco montado especificado em d, no modo Read/Write,
//criando o arquivo se nao existir. Retorna um descritor de arquivo,
//em caso de sucesso. Retorna -1, caso contrario.
int myFSOpen (Disk *d, const char *path) {
	//filesOpened ++, nao podeultrapassar MAX_FDS
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

//Funcao para a escrita de um arquivo, a partir de um descritor de arquivo
//existente. Os dados de buf sao copiados para o disco a partir da posição
//atual do cursor e terao tamanho maximo de nbytes. Ao fim, o cursor deve
//ter posicao atualizada para que a proxima operacao ocorra a partir do
//proximo byte apos o ultimo escrito. Retorna o numero de bytes
//efetivamente escritos em caso de sucesso ou -1, caso contrario
int myFSWrite (int fd, const char *buf, unsigned int nbytes) {
	return -1;
}

//Funcao para fechar um arquivo, a partir de um descritor de arquivo
//existente. Retorna 0 caso bem sucedido, ou -1 caso contrario
int myFSClose (int fd) {
	//filesOpened -- (nao fazer menos que zero)
	return -1;
}

//Funcao para instalar seu sistema de arquivos no S.O., registrando-o junto
//ao virtual FS (vfs). Retorna um identificador unico (slot), caso
//o sistema de arquivos tenha sido registrado com sucesso.
//Caso contrario, retorna -1

int installMyFS (void) {
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
