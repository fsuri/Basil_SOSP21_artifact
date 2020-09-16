#include <iostream>
#include <sys/time.h>
#include <unistd.h>
#include <chrono>
#include <functional>

#include <sched.h>

#include "lib/latency.h"
#include "lib/crypto.h"
#include "lib/batched_sigs.h"
#include "lib/blake3.h"
#include "lib/message.h"
#include "lib/threadpool.cc"  //why can I not include the .h?

#include <gflags/gflags.h>

#include <random>

#include <event2/event.h>

void* printTest(std::string &msg){
	std::cout << msg << " running on cpu " << sched_getcpu() << std::endl;
	//std::string s("success");
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	return static_cast<void*>(&msg);

}

void confirmTest(void* data){
	std::string &result = *(static_cast<std::string*>(data));
	std::cout << "CALLBACK FUNCTION:(" << sched_getcpu() << ") " << result << std::endl;


}

void EventCallback(evutil_socket_t fd, short what, void *arg) {
	std::cout << "working" << std::endl;

}

template<typename T>  void* Tester(T f){ //static?
		T* t = (T*) malloc(sizeof(T));
		*t = f;
    return (void*) t;
}

template<typename T>  void* pointerWrapper(std::function<T()> func){ //static?
    T* t = new T;//(T*) malloc(sizeof(T));
    *t = func();
    return (void*) t;
}

template<typename T>  void* pointerWrapper2(T func){ //static?
    T* t = new T;//(T*) malloc(sizeof(T));
    *t = func();
    return (void*) t;
}

void* stringWrapper(std::function<std::string()> func){
	std::string* t = new std::string;// (std::string*) malloc(sizeof(std::string));
	*t = func();
	return (void*) t;
}

bool testBool(){
		return true;
}

std::string printHello(){
	std::string g("HELLO TEMPLATE WORKS");
	return g;
}

//typedef std::function<std::string()> inputString;

int main(int argc, char *argv[]){

	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	int num_cpus = std::thread::hardware_concurrency();
	CPU_SET(num_cpus-1, &cpuset); //last core is for main
	int rc = pthread_setaffinity_np(pthread_self(),	sizeof(cpu_set_t), &cpuset);

	// std::string q("HELLO TEMPLATE WORKS");
	// auto y = std::bind(Tester<std::string>, q);
	// void* beta = y(); //Tester(q);
	// std::string* ceta = (std::string*) beta;
	//
	// std::cout << *ceta << std::endl;
	// 	free(beta);

	//std::function<std::string()> q(printHello);
	//auto w = std::bind(pointerWrapper<std::string>, printHello);
	auto w = std::bind(pointerWrapper<bool>, testBool);
	//std::cout << q.target_type().name() << std::endl;

	void* beta = w(); //pointerWrapper(q);
	bool* ceta = (bool*) beta;
	//std::string* ceta = (std::string*) beta;
	std::cout << *ceta << std::endl;

  // std::function<bool()> b = testBool;
	// void* beta = pointerWrapper(b);
	// bool* ceta = (bool*) beta;
	// std::cout << *ceta << std::endl;
	//free(ceta);

	// auto z = std::bind(stringWrapper, printHello); //why does &stringWrapper make no difference
	// void* beta = z(); //stringWrapper(printHello);
	// std::string* ceta = (std::string*) beta;
	// std::cout << *ceta << std::endl;
	// free(beta);



	/////////////

		bool* bool1 = (bool*) malloc(sizeof(bool));
		*bool1 = true;
		void* test2 = (void*) bool1;
		if( ! (* (bool*) test2)){
			std::cout << "void pointer failed" << std::endl;
		}
		else{
			std::cout << "void pointer success" << std::endl;
		}

		ThreadPool *tp = new ThreadPool();
		//ThreadPool tp = ThreadPool();
		std::string s("hello");

		printTest(s);
		event_base* libeventBase =  event_base_new();

	  void* test;
	  event* ev = event_new(libeventBase, -1, 0, EventCallback, test);
		struct timeval five_seconds = {5,0};
	  //int a = event_add(ev, &five_seconds);
		 int a = event_add(ev, NULL);
			std::cout << a << std::endl;
			



		std::function<void*()> f = std::bind(printTest, s);
		//void* data = f();
		//confirmTest(data);
		tp->dispatch(f, confirmTest, libeventBase );
		tp->dispatch(f, confirmTest, libeventBase );
		tp->dispatch(f, confirmTest, libeventBase );
		tp->dispatch(f, confirmTest, libeventBase );
		tp->dispatch(f, confirmTest, libeventBase );
		tp->dispatch(f, confirmTest, libeventBase );

			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		int ret = event_base_dispatch(libeventBase);
		std::cout << ret << std::endl;

		exit(0);





}
