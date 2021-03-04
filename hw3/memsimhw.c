//
// Virual Memory Simulator Homework
// One-level page table system with FIFO and LRU
// Two-level page table system with LRU
// Inverted page table with a hashing system
// OS Homework
// Submission Year : 2020
// Student Name : Seo jin hyuk
// Student Number : B811088
//
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define PAGESIZEBITS 12
#define VIRTUALADDRBITS 32

int simType, firstLevelBits, PhysicalMemorySizeBits, numProcess,  numFrame, numPageTable, numSecondPageTable, framePtr, full = 0;

struct pageTableEntry {
	int valid;
	int frameNumber;
	struct pageTableEntry *twoLevelPageTable;
} *pageTable;

struct frameEntry {
	int pid;
	int birthTime;
	int recentHitTime;
	int pageTableNumber;
	int secondPageTableNumber;
} *frame;

struct hashTableEntry {
	int pid;
	int size;
	int frame;
	int pageTableNumber;
	struct hashTableEntry *next;
} *hashTable, *hashNode;

struct procEntry {
	char *traceName;			// the memory trace name
	int pid;					// process (trace) id
	int ntraces;				// the number of memory traces
	int num2ndLevelPageTable;	// The 2nd level page created(allocated);
	int numIHTConflictAccess; 	// The number of Inverted Hash Table Conflict Accesses
	int numIHTNULLAccess;		// The number of Empty Inverted Hash Table Accesses
	int numIHTNonNULLAcess;		// The number of Non Empty Inverted Hash Table Accesses
	int numPageFault;			// The number of page faults
	int numPageHit;				// The number of page hits
	FILE *tracefp;
} *procTable;

void initProcTable() {
	int i;
	for(i = 0; i < numProcess; i++) {
		procTable[i].num2ndLevelPageTable = 0;
		procTable[i].numIHTConflictAccess = 0;
		procTable[i].numIHTNULLAccess = 0;
		procTable[i].numIHTNonNULLAcess = 0;
		procTable[i].numPageFault = 0;
		procTable[i].numPageHit = 0;
	}
}

void initHashTable() {
	int i;
	for(i = 0; i < numFrame; i++) {
		hashTable[i].size = 0;
		hashTable[i].next = NULL;
	}
}

int isHit(int pid, int pageNumber) {
	int h = Hash(pid, pageNumber);
	if(hashTable[h].size == 0) {procTable[pid].numIHTNULLAccess++;  return -1;}
	if(hashTable[h].size > 0) {procTable[pid].numIHTNonNULLAcess++;}
	struct hashTableEntry *ptr = hashTable[h].next;
	while(ptr != NULL) {
		procTable[pid].numIHTConflictAccess++;
		if(ptr->pid == pid && ptr->pageTableNumber == pageNumber) {return ptr->frame;}
		ptr = ptr->next;
	}
	return -1;
}

struct hashTableEntry * removeNode(int pid, int pageNumber) {
	int h = Hash(pid, pageNumber);
	hashTable[h].size--;
	struct hashTableEntry *pptr = &hashTable[h];
	struct hashTableEntry *ptr = hashTable[h].next;
	while(1) {
		if(ptr->pid == pid && ptr->pageTableNumber == pageNumber) {
			pptr->next = ptr->next;
			return ptr;
		}
		pptr = ptr;
		ptr = ptr->next;
	}
}

void insertNode(int pid, int pageNumber, int frame, struct hashTableEntry *ptr) {
	int h = Hash(pid, pageNumber);
	hashTable[h].size++;
	ptr->pid = pid;
	ptr->pageTableNumber = pageNumber;
	ptr->frame = frame;
	ptr->next = hashTable[h].next;
	hashTable[h].next = ptr;
}

int Hash(int pid, int pageNumber) {
	return (pid + pageNumber) % numFrame;
}

int Fifo() {
	if(full == 0) {return framePtr++;}
	int i, target = 0;
	for(i = 0; i < numFrame; i++) {
		if(frame[target].birthTime > frame[i].birthTime) {target = i;}
	}
	return target;
}

int Lru() {
	if(full == 0) {return framePtr++;}
	int i, target = 0;
	for(i = 0; i < numFrame; i++) {
		if(frame[target].recentHitTime > frame[i].recentHitTime) {target = i;}
	}
	return target;
}

void execute_OneLevel() {
	int i, j, pageNumber, currentTime = 0;
	unsigned addr;
	char rw;

	for(i = 0; i < 1000000; i++) {
		for(j = 0; j < numProcess; j++) {
			if(fscanf(procTable[j].tracefp, "%x %c", &addr, &rw) != EOF) {
				currentTime++;
				pageNumber = addr>>12;

				//hit
				if(pageTable[j * numPageTable + pageNumber].valid == 1) {
					frame[pageTable[j * numPageTable + pageNumber].frameNumber].recentHitTime = currentTime;
					procTable[j].numPageHit++;
				}

				//fault
				else {
					int nFrame;
					if(simType == 0) {nFrame = Fifo();}
					if(simType == 1) {nFrame = Lru();}
					if(full == 1) {pageTable[frame[nFrame].pid * numPageTable + frame[nFrame].pageTableNumber].valid = 0;}
					if(framePtr == numFrame) {full = 1;}

					pageTable[j * numPageTable + pageNumber].valid = 1;
					pageTable[j * numPageTable + pageNumber].frameNumber = nFrame;
					frame[nFrame].pid = j;
					frame[nFrame].birthTime = currentTime;
					frame[nFrame].recentHitTime = currentTime;
					frame[nFrame].pageTableNumber = pageNumber;
					procTable[j].numPageFault++;
				}
			}
			else {goto EXIT;}
		}
	}

EXIT:
	for(i = 0; i < numProcess; i++) {procTable[i].ntraces = procTable[i].numPageHit + procTable[i].numPageFault;}
}

void execute_TwoLevel() {
	int i, j, pageNumber, secondPageNumber, currentTime = 0;
	unsigned addr;
	char rw;

	for(i = 0; i < 1000000; i++) {
		for(j = 0; j < numProcess; j++) {
			if(fscanf(procTable[j].tracefp, "%x %c", &addr, &rw) != EOF) {
				currentTime++;
				pageNumber = (addr>>12) / numSecondPageTable;
				secondPageNumber = (addr>>12) % numSecondPageTable;

				//hit
				if(pageTable[j * numPageTable + pageNumber].valid == 1 && pageTable[j * numPageTable + pageNumber].twoLevelPageTable[secondPageNumber].valid == 1) {
					frame[pageTable[j * numPageTable + pageNumber].twoLevelPageTable[secondPageNumber].frameNumber].recentHitTime = currentTime;
					procTable[j].numPageHit++;
				}

				//fault
				else {
					int nFrame = Lru();
					if(full == 1) {pageTable[frame[nFrame].pid * numPageTable + frame[nFrame].pageTableNumber].twoLevelPageTable[frame[nFrame].secondPageTableNumber].valid = 0;}
					if(framePtr == numFrame) {full = 1;}
					if(pageTable[j * numPageTable + pageNumber].valid == 0) {
						pageTable[j * numPageTable + pageNumber].twoLevelPageTable = (struct pageTableEntry *) malloc(sizeof(struct pageTableEntry) * numSecondPageTable);
						pageTable[j * numPageTable + pageNumber].valid = 1;
						procTable[j].num2ndLevelPageTable++;
					}

					pageTable[j * numPageTable + pageNumber].twoLevelPageTable[secondPageNumber].valid = 1;
					pageTable[j * numPageTable + pageNumber].twoLevelPageTable[secondPageNumber].frameNumber = nFrame;
					frame[nFrame].pid = j;
					frame[nFrame].birthTime = currentTime;
					frame[nFrame].recentHitTime = currentTime;
					frame[nFrame].pageTableNumber = pageNumber;
					frame[nFrame].secondPageTableNumber = secondPageNumber;
					procTable[j].numPageFault++;
				}
			}
			else {goto EXIT;}
		}
	}

EXIT:
	for(i = 0; i < numProcess; i++) {procTable[i].ntraces = procTable[i].numPageHit + procTable[i].numPageFault;}
}

void execute_Inverterd() {
	initHashTable();
	int i, j, pageNumber, fNumber, currentTime = 0;
	unsigned addr;
	char rw;

	for(i = 0; i < 1000000; i++) {
		for(j = 0; j < numProcess; j++) {
			if(fscanf(procTable[j].tracefp, "%x %c", &addr, &rw) != EOF) {
				currentTime++;
				pageNumber = addr>>12;

				//hit
				if((fNumber = isHit(j, pageNumber)) >= 0) {
					frame[fNumber].recentHitTime = currentTime;
					procTable[j].numPageHit++;
				}

				//fault
				else {
					struct hashTableEntry *ptr;
					if(full == 0) {ptr = &hashNode[framePtr];}
					int nFrame = Lru();
					if(full == 1) {
						ptr = removeNode(frame[nFrame].pid, frame[nFrame].pageTableNumber);
					}
					if(framePtr == numFrame) {full = 1;}

					insertNode(j, pageNumber, nFrame, ptr);
					frame[nFrame].pid = j;
					frame[nFrame].birthTime = currentTime;
					frame[nFrame].recentHitTime = currentTime;
					frame[nFrame].pageTableNumber = pageNumber;
					procTable[j].numPageFault++;
				}
			}
			else {goto EXIT;}
		}
	}

EXIT:
	for(i = 0; i < numProcess; i++) {procTable[i].ntraces = procTable[i].numPageHit + procTable[i].numPageFault;}
}

void oneLevelVMSim() {
	int i;
	for(i=0; i < numProcess; i++) {
		printf("**** %s *****\n",procTable[i].traceName);
		printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
		printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
	}
}

void twoLevelVMSim() {
	int i;
	for(i=0; i < numProcess; i++) {
		printf("**** %s *****\n",procTable[i].traceName);
		printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
		printf("Proc %d Num of second level page tables allocated %d\n",i,procTable[i].num2ndLevelPageTable);
		printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
	}
}

void invertedPageVMSim() {
	int i;
	for(i=0; i < numProcess; i++) {
		printf("**** %s *****\n",procTable[i].traceName);
		printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
		printf("Proc %d Num of Inverted Hash Table Access Conflicts %d\n",i,procTable[i].numIHTConflictAccess);
		printf("Proc %d Num of Empty Inverted Hash Table Access %d\n",i,procTable[i].numIHTNULLAccess);
		printf("Proc %d Num of Non-Empty Inverted Hash Table Access %d\n",i,procTable[i].numIHTNonNULLAcess);
		printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
		assert(procTable[i].numIHTNULLAccess + procTable[i].numIHTNonNULLAcess == procTable[i].ntraces);
	}
}

int main(int argc, char *argv[]) {
	int i, j;

	simType = atoi(argv[1]);
	firstLevelBits = atoi(argv[2]);
	PhysicalMemorySizeBits = atoi(argv[3]);

	if(PhysicalMemorySizeBits < 12) {printf("PhysicalMemorySizeBits %d should be larger than PageSizeBits 12\n", PhysicalMemorySizeBits); return(0);}
	if(firstLevelBits >= 20) {printf("firstLevelBits %d is too Big for the 2nd level page system\n", firstLevelBits); return(0);}

	framePtr = 0;
	numProcess = argc - 4;
	numFrame = 1L<<(PhysicalMemorySizeBits-12);
	if(simType == 0 || simType == 1) {numPageTable = 1L<<20;}
	if(simType == 2) {numPageTable = 1L<<firstLevelBits; numSecondPageTable = 1L<<(20-firstLevelBits);}

	procTable = (struct procEntry *) malloc(sizeof(struct procEntry) * numProcess);
	frame = (struct frameEntry *) malloc(sizeof(struct frameEntry) * numFrame);
	if(simType == 0 || simType == 1 || simType == 2) {pageTable = (struct pageTableEntry *) malloc(sizeof(struct pageTableEntry) * numProcess * numPageTable);}
	if(simType == 3) {
		hashTable = (struct hashTableEntry *) malloc(sizeof(struct hashTableEntry) * numFrame);
		hashNode = (struct hashTableEntry *) malloc(sizeof(struct hashTableEntry) * numFrame);
	}

	for(i = 0; i < numProcess; i++) {
		printf("process %d opening %s\n",i,argv[i+4]);
		procTable[i].tracefp = fopen(argv[i+4],"r");
		if (procTable[i].tracefp == NULL) {
			printf("ERROR: can't open %s file; exiting...",argv[i+4]);
			exit(1);
		}
		else {
			procTable[i].pid = i;
			procTable[i].traceName = malloc(strlen(argv[i+4]));
			for(j = 0; j < strlen(argv[i+4]); j++) {procTable[i].traceName[j] = argv[i+4][j];}
		}
	}

	initProcTable();

	if(simType == 0) {execute_OneLevel();}

	if(simType == 1) {execute_OneLevel();}

	if(simType == 2) {execute_TwoLevel();}

	if(simType == 3) {execute_Inverterd();}

	printf("Num of Frames %d Physical Memory Size %ld bytes\n",numFrame, (1L<<PhysicalMemorySizeBits));

	if (simType == 0) {
		printf("=============================================================\n");
		printf("The One-Level Page Table with FIFO Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		oneLevelVMSim();
	}

	if (simType == 1) {
		printf("=============================================================\n");
		printf("The One-Level Page Table with LRU Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		oneLevelVMSim();
	}

	if (simType == 2) {
		printf("=============================================================\n");
		printf("The Two-Level Page Table Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		twoLevelVMSim();
	}

	if (simType == 3) {
		printf("=============================================================\n");
		printf("The Inverted Page Table Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		invertedPageVMSim();
	}

	return(0);
}
