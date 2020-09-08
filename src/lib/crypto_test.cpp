#include "ed25519.h"
#include <iostream>
#include <sys/time.h>
#include <unistd.h>
#include <chrono>




int main(int argc, char *argv[]){
	ed25519_secret_key sk;
	ed25519_randombytes_unsafe(&sk, sizeof(ed25519_secret_key));
	//std::cout << sk << std::endl;

	ed25519_public_key pk;
	ed25519_publickey(sk, pk);

	const unsigned char * testMsg = (unsigned char*) "hello";
	
  std::cout << testMsg << std::endl;
	ed25519_signature sig;
  struct timeval tv;
  struct timeval tv2;
  gettimeofday(&tv, NULL);
	ed25519_sign(testMsg, sizeof(testMsg)-1, sk, pk, sig);
  gettimeofday(&tv2, NULL);
  uint64_t t1 = (tv.tv_sec*1000000+tv.tv_usec);
  uint64_t t2 = (tv2.tv_sec*1000000+tv2.tv_usec);
  uint64_t elapsed_time = t2-t1;  //in microseconds;  //in microseconds
  std::cout << elapsed_time << std::endl;
	//std::cout << sig << std::endl;

  //gettimeofday(&tv, NULL);
  std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
	int ret = ed25519_sign_open(testMsg, sizeof(testMsg)-1, pk, sig) == 0;
  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
//  gettimeofday(&tv2, NULL);
  //  t1 = (tv.tv_sec*1000000+tv.tv_usec);
  //  t2 = (tv2.tv_sec*1000000+tv2.tv_usec);
  // elapsed_time = t2-t1;  //in microseconds;  //in microseconds
  // std::cout << elapsed_time << std::endl;
  std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << "[µs]" << std::endl;

	std::cout << ret << std::endl;

  //usleep(10000000);
//   //gettimeofday(&tv2, NULL);
//   uint64_t t1 = (tv.tv_sec*1000000+tv.tv_usec);
//   std::cout << t1 <<std::endl;
//   uint64_t t2 = (tv2.tv_sec*1000000+tv2.tv_usec);
//   std::cout << t2 <<std::endl;
// //add latencies

//try this
// const unsigned char *mp[num] = {message1, message2..}
// size_t ml[num] = {message_len1, message_len2..}
// const unsigned char *pkp[num] = {pk1, pk2..}
// const unsigned char *sigp[num] = {signature1, signature2..}
// int valid[num]
//
// /* valid[i] will be set to 1 if the individual signature was valid, 0 otherwise */
// int all_valid = ed25519_sign_open_batch(mp, ml, pkp, sigp, num, valid) == 0;

}
