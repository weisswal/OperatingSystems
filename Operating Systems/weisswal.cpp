#ifndef __PROGTEST__
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <climits>
#include <cfloat>
#include <cassert>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <string>
#include <vector>
#include <array>
#include <iterator>
#include <set>
#include <list>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <stack>
#include <deque>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <condition_variable>
#include <pthread.h>
#include <semaphore.h>
#include "progtest_solver.h"
#include "sample_tester.h"
using namespace std;
#endif /* __PROGTEST__ */

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

struct SheetStruct{
  // SheetStruct(){
  //   shared_ptr<ASheet> sheetPointer = make_shared <ASheet> () ;
  //   bool done = false;
  // }
  bool done = false;
  //shared_ptr<ASheet> sheetPointer;
  ASheet sheetPointer;
};

class CQualityControl
{
  public:
    // takes sheet , runs algorithims, updates sheet
    static void checkAlgorithm ( ASheet sheet ); //~done~..maybe

    void addLine ( AProductionLine line ); //done

    void start( int workThreads ); //done
    void stop ( void );
    void serving_workers ( int tid );
    void submitting_work (int tid);
    void doing_work (int tid);

    void printPending();

  private:

    //mutex mtx;
    mutex mtx2;
    condition_variable wait_for_communication;
    condition_variable wait_for_communication2;
    vector<thread> worker_threads;
    vector< thread > comm_threads_serving;
    vector< thread > comm_threads_submitting;
    deque < shared_ptr<SheetStruct> > assemblyLine;
    vector<  pair < AProductionLine , deque<shared_ptr<SheetStruct>> > >  pendingSheets;
    atomic <bool> work_being_done = true;
    atomic <int> serving_ = 0;
};

void CQualityControl::printPending()
{
  int counter = 0;
  for(auto & i : pendingSheets)
  {
    //cout << "testtesttest" << endl;
    for(auto & j : i.second)
    {
      cout <<"RollingMill:"<< counter <<  " sheet bool is..."  << j->done << endl;

      //printf( " sheet struct boolean is %d \n" , j->done);
    }
    counter ++;
    cout << "========================================" << endl;
  }
}

void CQualityControl::checkAlgorithm( ASheet sheet )
{
  int ** thicc = new int *[sheet->m_Length];

  for (int i = 0; i < sheet->m_Length; ++i) {

    thicc[i] = new int [sheet->m_Width];
  }
  for (int i = 0; i < sheet->m_Length; ++i) {

    for (int j = 0; j < sheet->m_Width; ++j) {

      thicc[i][j] = sheet->m_Thickness[i*sheet->m_Width + j];
    }
  }

  for( auto & i : sheet ->m_RelDev )
  {
    double rdm = i.first;
    int area = maxRectByRelDev( thicc, sheet ->m_Width, sheet ->m_Length, rdm );
    sheet -> updateRelDev( rdm , area );
  }
  for( auto & i : sheet ->m_Volume )
  {
    int64_t vm = i.first;
    int area = maxRectByVolume( thicc, sheet ->m_Width, sheet ->m_Length, vm );
    sheet -> updateVolume( vm , area );
  }
  for( auto & i : sheet ->m_MinMax )
  {
    int area = maxRectByMinMax( thicc, sheet ->m_Width, sheet ->m_Length, i.first.m_Lo , i.first.m_Hi );
    sheet -> updateMinMax( i.first , area );
  }
}

void CQualityControl::addLine ( AProductionLine line )
{
  deque < shared_ptr<SheetStruct> > x;
  pendingSheets.push_back(make_pair(line , x));
  serving_++;
}

void CQualityControl::start( int workThreads )
{
  for(int i = 0; i < workThreads ; i ++ )
  {
    worker_threads.emplace_back(thread(&CQualityControl::doing_work, this, i));
  }
  for(unsigned int i = 0; i < pendingSheets.size() ; i ++ )
  {
    comm_threads_serving.emplace_back(thread(&CQualityControl::serving_workers, this, i));
  }
  for(unsigned int i = 0; i < pendingSheets.size() ; i ++ )
  {
    comm_threads_submitting.emplace_back(thread(&CQualityControl::submitting_work, this, i));
  }
}

void CQualityControl::stop ( void )
{
  //this->_stop = true;
  for (auto &x: comm_threads_serving){
     x.join();
   }
  for (auto &x: worker_threads){
    x.join();
  }
  for (auto &x: comm_threads_submitting){
      x.join();
   }
}

void CQualityControl::serving_workers ( int tid )
{
  while(true)
  {
    ASheet p = pendingSheets[tid].first -> getSheet();
    //this_thread::sleep_for(chrono::milliseconds(100));
    shared_ptr <SheetStruct> data = make_shared<SheetStruct>();
    data->sheetPointer = p;
    data->done = false;

    unique_lock<mutex> work_lock(mtx2);
    if(!p){
      printf("C1: %d Serving DONE\n" , tid);
      serving_--;
      work_lock.unlock();
      wait_for_communication.notify_all();
      break;
    }else{
     printf("C1: %d  Serving Sheets \n", tid);
     pendingSheets[tid].second.push_back(data);
     assemblyLine.push_back(data);
     work_lock.unlock();
     wait_for_communication.notify_all();
  }
 }
}

void CQualityControl::doing_work (int tid)
{
  while(true)
  {
    unique_lock<mutex> work_lock (mtx2);
    wait_for_communication.wait(work_lock, [ & ] () { return  (!assemblyLine.empty() || serving_ == 0); });

    if(serving_ == 0 && assemblyLine.empty() ){
      printf("T: %d  NO WORK LEFT \n", tid);
      work_being_done = false;
      wait_for_communication2.notify_all();
      break;
    }else{
      printf("T: %d  WORKER WORKING \n", tid);

      auto tmp = assemblyLine.front();
      assemblyLine.pop_front();
      work_lock.unlock();

      checkAlgorithm(tmp->sheetPointer);
      tmp->done = true;

      printPending();
      printf("T: %d  WORKER DONE \n", tid);
      wait_for_communication2.notify_all();
    }
  }
}

void CQualityControl::submitting_work (int tid)
{
  while(true)
  {
    unique_lock<mutex>sheet_lock(mtx2);
    if(!pendingSheets[tid].second.empty())
    {
      wait_for_communication2.wait(sheet_lock, [ & ] () { return pendingSheets[tid].second.front()->done; } );
      if(pendingSheets[tid].second.front()->done)
      {
        printf("C2: %d WORK READY TO BE SUBMITTED \n", tid);
        auto data = pendingSheets[tid].second.front();
        pendingSheets[tid].second.pop_front();
        sheet_lock.unlock();
        //this_thread::sleep_for(chrono::milliseconds(100));
        pendingSheets[tid].first -> doneSheet(data->sheetPointer);
        printf("C2: %d WORK SUBMITTED \n", tid);
      }
    }
    else
    {
      if( pendingSheets[tid].second.empty() && serving_ == 0 )
      {
          printf("SUBMITTING THREAD %d YA DUN KNO\n", tid);
          break;
      }
    }
  }
}


//-------------------------------------------------------------------------------------------------------------------------------------------------------------
#ifndef __PROGTEST__
int                main                                    ( void )
{
  CQualityControl control;
  AProductionLineTest line = std::make_shared<CProductionLineTest> ();
  AProductionLineTest line1 = std::make_shared<CProductionLineTest> ();
  AProductionLineTest line2 = std::make_shared<CProductionLineTest> ();

  control . addLine ( line );
  control . addLine ( line1 );
  control . addLine ( line2 );

  control . start ( 7 );
  control . stop  ();
  // if ( ! line -> allProcessed () )
  //   throw std::logic_error ( "(some) sheets were not correctly processsed" );
  return 0;
}
#endif /* __PROGTEST__ */
