#include <cmath>
#include <ctime>
#include <cstdlib>
#include <cstdio>

#include "BinarySearchTree.h"
using CSC1310::BinarySearchTree;
#include "BinaryTreeIterator.h"
using CSC1310::BinaryTreeIterator;
#include "ListArray.h"
using CSC1310::ListArray;
#include "ListArrayIterator.h"
using CSC1310::ListArrayIterator;

#include "Permutation.h"
using CSC1310::Permutation;
#include "Random.h"
using CSC1310::Random;

#include <pthread.h>
#include "CD.h"

#include <iostream>
using namespace std;

int BUFFER_SIZE = 1;
int NUM_PRODUCERS = 1;
int NUM_CONSUMERS = 1;
int NUM_TREES = 10000;
int ERROR_RATE = 10;


CD*** buffer;
int buffer_count = 0;
int producer_index = 0;
int consumer_index = 0;

int num_trees_p = 0;
int num_trees_c = 0;

pthread_cond_t empty, full = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

CD** producer_seq(ListArray<CD>* cds, Random* rand);
void consumer_seq(CD** cds_array, int num_items, int expected_height);
void* producer_par(void *arg);
void* consumer_par(void *arg);
void parallel();
void test(void* arg);
void deleteCDs(ListArray<CD>* list);
void put(CD** cds_array);
CD** get();
void thr_join();

int main()
{
	buffer = new CD**[BUFFER_SIZE];
	
	time_t start, end;
	Random* rand = Random::getRandom();
	   
   //the unsorted ListArray of cds
   ListArray<CD>* cds = CD::readCDs("cds.txt");
   int num_items = cds->size();
   cout << num_items << endl;
   cout << "based on the number of items, the min height should be: " << endl;
   int expected_height = ceil(log2(num_items + 1));
   cout << expected_height << endl;
   cout << endl;
   
   long producer_args[2];
   producer_args[0] = (long) cds;
   producer_args[1] = (long) rand;
   
   int consumer_args[2];
   consumer_args[0] = num_items;
   consumer_args[1] = expected_height;
  
   start = time(NULL);

//sequential solution: process one tree at a time
	for (int i = 1; i <= NUM_TREES; i++)
	{
	    CD** cd_array = producer_seq(cds, rand);
		consumer_seq(cd_array, num_items, expected_height);
	}
   
    end = time(NULL);

	printf("sequential: %ds\n", (int)(end - start)); 

//parallel solution: process trees in parallel
	start = time(NULL);

		printf("creating producer\n");
		pthread_t p, c;
		pthread_create(&p, NULL, producer_par, producer_args);
		printf("creating consumer\n");
		pthread_create(&c, NULL, consumer_par, consumer_args);
		thr_join();

	end = time(NULL);
  	
	printf("sequential: %ds\n", (int)(end - start)); 
	
   deleteCDs(cds);
   delete cds;
   
   delete[] buffer;

   return 0;
}
//Puts CD** in the buffer, modified to increment num_trees_p
void put(CD** cds_array)
{
	buffer[producer_index] = cds_array;
	producer_index = (producer_index + 1) % BUFFER_SIZE;
	buffer_count++;  //buffer fills up
	num_trees_p; //to keep track of how far along we are
}

//Returns a CD double pointer and decrements as needed,  modified to increment num_trees_c
CD** get()
{
	CD** cds_array = buffer[consumer_index];
	consumer_index = (consumer_index + 1) % BUFFER_SIZE;
	buffer_count--;  //buffer empties out
	num_trees_c++;//to keep track of how far along we are
	return cds_array;
}

void deleteCDs(ListArray<CD>* list)
{
   ListArrayIterator<CD>* iter = list->iterator();

   while(iter->hasNext())
   {
      CD* cd = iter->next();
      delete cd;
   }

   delete iter;
}

CD** producer_seq(ListArray<CD>* cds, Random* rand)
{
//1.  get an array with the cds in a random order
	    int num_items = cds->size();
		Permutation* p = new Permutation(num_items, num_items, rand);
		p->basicInit();
		
		CD** permute_cds = new CD*[num_items];
		int count = 0;
		while(p->hasNext())
		{
			int i = p->next();
			permute_cds[count] = cds->get(i);
			count++;
		}
		delete p;

//2.  insert the cds in array order (different each time) into BST
		BinarySearchTree<CD>* bst = new BinarySearchTree<CD>(&CD::compare_items, &CD::compare_keys);
		for (int i = 0; i < num_items; i++)
		{
			CD* cd = permute_cds[i];
			bst->insert(cd);
		}
		delete[] permute_cds;
  
//3.  create a complete binary search tree
		BinarySearchTree<CD>* complete_bst = bst->minimizeComplete();
		CD** cds_array = new CD*[num_items];
		BinaryTreeIterator<CD>* complete_iter = complete_bst->iterator();

//4.  place the cds into an array using a level order traversal
		//intentionally inserting an error in some cases using a post order traversal
		int traversal_error = rand->getRandomInt(1, NUM_TREES);
		if (traversal_error >= ERROR_RATE)
		{
			complete_iter->setLevelorder();
		}
		else
		{
			//wrong traversal, consumer should detect a height error
			//still will be sorted, however
			complete_iter->setPostorder();
		}
		count = 0;
		while(complete_iter->hasNext())
		{
			cds_array[count] = complete_iter->next();
			count++;
		}
		delete bst;
		delete complete_iter;
		delete complete_bst;
		
//5.  return the array that is supposed to represent a complete binary tree
		return cds_array;
}

void consumer_seq(CD** cds_array, int num_items, int expected_height)
{
//1.  put the items in the provided array into a BST
//note that if the array represents a complete binary tree,
//this process will create a complete binary tree that is also a BST
		BinarySearchTree<CD>* bst = new BinarySearchTree<CD>(&CD::compare_items, &CD::compare_keys);
		for (int i = 0; i < num_items; i++)
		{
			CD* cd = cds_array[i];
			bst->insert(cd);
		}

//2.  verify that the items are in sorted order using an inorder traversal
		BinaryTreeIterator<CD>* bst_iter = bst->iterator();
		bst_iter->setInorder();  //this was omitted
		CD* first = bst_iter->next();
		bool sorted = true;
		while(bst_iter->hasNext())
		{
			CD* second = bst_iter->next();
			if (CD::compare_items(first, second) >= 0)
			{
				sorted = false;
				break;
			}
			first = second;
		}
		delete bst_iter;

//3.  check that the BST is minimum height and balanced
//the randomly determined errors will fail the height test
		int h = bst->getHeight();
		bool bal = bst->isBalanced();
   
		if (!sorted || h != expected_height || !bal)
		{
			cout << "invalid complete binary tree" << endl;
		}
		else
		{
			//cout << "valid complete binary tree" << endl;
		}

		delete bst;
		delete[] cds_array;
}

void* producer_par(void *arg){
	int i;

	//argparsing
	long args[2];
	args[0] = *(long*) arg;
	args[1] = *(long*) (arg + sizeof(long)); 

	//main loop
	for (int i = 1; i <= NUM_TREES; i++)
	{
		pthread_mutex_lock(&mutex);
		printf("Producer has the lock!\n");
		while(buffer_count == BUFFER_SIZE){
			printf("Producer has gone to sleep...\n");
			pthread_cond_wait(&empty, &mutex);
			
		}
		printf("Attempting to put...\n");
		//Error here, tries to access invalid memory...
	    put(producer_seq( (ListArray<CD>*) args[0], (Random*) args[1]));
		pthread_cond_signal(&full);
		printf("Producer no longer has the lock!\n");
		pthread_mutex_unlock(&mutex);
	}
	return NULL;
}

void* consumer_par(void *arg){
	int i;

	//argparsing here
	int args[2];
	args[0] = *(int*) arg;
	args[1] = *(int*) arg + sizeof(int); 

	//main loop
	for (int i = 1; i <= NUM_TREES; i++)
	{
		pthread_mutex_lock(&mutex);
		printf("Consumer has the lock!\n");
		while(buffer_count == 0){
			printf("Consumer has gone to sleep...\n");
			pthread_cond_wait(&full, &mutex);
		}
		CD** tmp = get();
		pthread_cond_signal(&empty);
		printf("Consumer no longer has the lock!\n");
		pthread_mutex_unlock(&mutex);
		printf("Attempting to consume...\n");
		consumer_seq(tmp, args[0], args[1]);
	}
	return NULL;
}

void thr_join(){
	pthread_mutex_lock(&mutex);
	//Should spin and wait until both consumers and producers have gone through 
	//a total of NUM_TREES trees 
	while (num_trees_c != NUM_TREES && num_trees_p != NUM_TREES){
		pthread_cond_wait(&empty, &mutex);
	}
	pthread_mutex_unlock(&mutex);
}


