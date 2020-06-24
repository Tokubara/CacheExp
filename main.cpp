#include <iostream>
#include <cstring>
#include <stdlib.h>
#include<iostream>
#include <vector>
#include <assert.h>
#include <list>
#include <unordered_map>
#include <cstdio>
#include <unordered_map>
#include <assert.h>
#include <list>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <time.h>
#include <climits>
#include "map"
#include <fstream>
#include <vector>
#include <algorithm>
#include <string>
using namespace std;
//#include "CacheSim.h"
//#include "argparse.hpp"
#include "LIRS.cpp"

using namespace std;

typedef unsigned short _u16;
typedef unsigned char _u8;

//TODO 这里不知道有没有问题先注释掉，因为LIRS.cpp也定义了下面这些
typedef unsigned char _u8;
typedef unsigned int _u32;
typedef unsigned long long _u64;

const unsigned char CACHE_FLAG_VALID = 0x01;
const unsigned char CACHE_FLAG_DIRTY = 0x02;
const unsigned char CACHE_FLAG_MASK = 0xff;

const char OPERATION_READ = 'l';
const char OPERATION_WRITE = 's';

/**替换算法*/
enum cache_swap_style {
    CACHE_SWAP_LRU,
    CACHE_SWAP_FRU,
    CACHE_SWAP_RAND,
    CACHE_SWAP_SRRIP,
    CACHE_SWAP_SRRIP_FP,
    CACHE_SWAP_BRRIP,
    CACHE_SWAP_DRRIP,
    CACHE_SWAP_MINE
};


class Cache_Line {
public:
    _u64 tag;
    _u8 flag;
    _u32 LRU;
    _u32 RRPV;
    _u32 FRU;
};

_u64 tick_count;
class CacheSim {
public:
    /**真正的cache地址列。指针数组*/
    Cache_Line *caches;
    /**cache的总大小，单位byte*/

    _u64 cache_size;
    /**cache line(Cache block)cache块的大小*/
    _u64 cache_line_size;
    /**总的行数*/
    _u64 cache_line_num;
    /**每个set有多少way*/
    _u64 cache_mapping_ways;
    /**整个cache有多少组*/
    _u64 cache_set_size;
    /**2的多少次方是set的数量，用于匹配地址时，进行位移比较*/
    _u64 cache_set_shifts;
    /**2的多少次方是line的长度，用于匹配地址*/
    _u64 cache_line_shifts;
    /**缓存替换算法*/
    int swap_style;

    /**指令计数器*/


    /**读写内存的指令计数*/
    _u64 cache_r_count, cache_w_count;
    /**实际写内存的计数，cache --> memory */
    _u64 cache_w_memory_count;
    /**实际读内存的计数，memory --> cache */
    _u64 cache_r_memory_count;
    /**cache hit和miss的计数*/
    _u64 cache_hit_count, cache_miss_count;
    /**分别统计读写的命中次数*/
    _u64 cache_r_hit_count, cache_w_hit_count,cache_w_miss_count, cache_r_miss_count;

    /** SRRIP算法中的2^M-1 */
    _u32 SRRIP_2_M_1;
    /** SRRIP算法中的2^M-2 */
    _u32 SRRIP_2_M_2;
    /** SRRIP 替换算法的配置，M值确定了每个block替换权重RRPV的上限*/
    int SRRIP_M;
    /** BRRIP的概率设置，论文里设置是1/32*/
    double EPSILON = 1.0 / 32;
    /** DRRIP算法中的single policy selection (PSEL) counter*/
    long long PSEL;
    int cur_win_replace_policy;

    /** 写miss时，将数据读入cache */
    int write_allocation;
    /**向cache写数据的时候，向memory也写一份。=0为write back*/
    int write_through;

    CacheSim();

    ~CacheSim();

    void init(_u64 a_cache_size, _u64 a_cache_line_size, _u64 a_mapping_ways, cache_swap_style a_swap_style);
    void set_M(int m);

     // 检查是否命中
    int cache_check_hit(_u64 set_base, _u64 addr);
    /**获取cache当前set中空余的line*/
    int cache_get_free_line(_u64 set_base);
    /**使用指定的替换策略获取cache当前set中一个被替换的line*/
    int cache_find_victim(_u64 set_base, int a_swap_style, int hit_index);
    /**命中，更新Cache状态*/
    void cache_hit(_u64 set_base, _u64 index, int a_swap_style);
    /**缺失，插入新行后更新Cache状态*/
    void cache_insert(_u64 set_base, _u64 index, int a_swap_style);
    void dump_cache_set_info(_u64 set_base);

    /**对一个指令进行分析*/
    void do_cache_op(_u64 addr, char oper_style);

    /**读入trace文件*/
    void load_trace(const char *filename);

    /**@return 返回miss率*/
    double get_miss_rate();
    double get_hit_rate();

    /** 计算int的次幂*/
    _u32 pow_int(int base, int expontent);
    _u64 pow_64(_u64 base, _u64 expontent);

    /**判断当前set是不是选中作为sample的set，并返回给当前set设置的policy*/
    int get_set_flag(_u64 set_base);
    void re_init();
    LIRS lirs[2050];
};





//
// Created by find on 16-7-19.
// Cache architect
// memory address  format:
// |tag|组号 log2(组数)|组内块号log2(mapping_ways)|块内地址 log2(cache line)|
//
//#include "CacheSim.h"

static inline _u64 get_ulonglong_max(void) {
#ifdef __MACH__
  return ULONG_MAX;
#else
  return ULONG_LONG_MAX;
#endif
}

CacheSim::CacheSim() {}

/**@arg a_cache_size[] 多级cache的大小设置
 * @arg a_cache_line_size[] 多级cache的line size（block size）大小
 * @arg a_mapping_ways[] 组相连的链接方式*/
_u32 CacheSim::pow_int(int base, int expontent) {
  _u32 sum = 1;
  for (int i = 0; i < expontent; i++) {
    sum *= base;
  }
  return sum;
}

_u64 CacheSim::pow_64(_u64 base, _u64 expontent) {
  _u64 sum = 1;
  for (int i = 0; i < expontent; i++) {
    sum *= base;
  }
  return sum;
}

void CacheSim::set_M(int m) {
  SRRIP_M = m;
  SRRIP_2_M_1 = pow_int(2, SRRIP_M) - 1;
  SRRIP_2_M_2 = pow_int(2, SRRIP_M) - 2;
}

void CacheSim::init(_u64 a_cache_size, _u64 a_cache_line_size, _u64 a_mapping_ways, cache_swap_style a_swap_style) {
  //如果输入配置不符合要求
  if (a_cache_line_size < 0 || a_mapping_ways < 1) {
    printf("cache line size or mapping ways are illegal\n");
    return;
  }

  //cache replacement policy
  swap_style = a_swap_style;

  // 几路组相联
  cache_mapping_ways = a_mapping_ways;

  //Cache Line大小
  cache_line_size = a_cache_line_size;

  //Cache大小
  cache_size = a_cache_size;

  // 总的line数 = cache总大小/ 每个line的大小（一般64byte，模拟的时候可配置）
  cache_line_num = (_u64) a_cache_size / a_cache_line_size;
  cache_line_shifts = (_u64) log2(a_cache_line_size);

  // 总共有多少set
  cache_set_size = cache_line_num / cache_mapping_ways;
  // 其二进制占用位数，同其他shifts
  cache_set_shifts = (_u64) log2(cache_set_size);


  cache_r_count = 0;
  cache_w_count = 0;
  cache_hit_count = 0;
  cache_miss_count = 0;
  cache_w_memory_count = 0;
  cache_r_memory_count = 0;
  cache_r_hit_count = 0;
  cache_w_hit_count = 0;
  cache_w_miss_count = 0;
  cache_r_miss_count = 0;

  // ticktock，主要用来在替换策略的时候提供比较的key，在命中或者miss的时候，相应line会更新自己的count为当时的tick_count;
  tick_count = 0;

  // 为每一行分配空间
  caches = (Cache_Line *) malloc(sizeof(Cache_Line) * cache_line_num);
  memset(caches, 0, sizeof(Cache_Line) * cache_line_num);

  for (int i = 0; i < cache_set_size; ++i)
    for (int j = 0; j < cache_mapping_ways; ++j)
      caches[i * cache_mapping_ways + j].LRU = j;

  //用于SRRIP算法
  PSEL = 0;
  cur_win_replace_policy = CACHE_SWAP_SRRIP;

  write_through = 0;
  write_allocation = 1;
}

/**顶部的初始化放在最一开始，如果中途需要对tick_count进行清零和caches的清空，执行此。*/
void CacheSim::re_init() {
  tick_count = 0;
  cache_hit_count = 0;
  cache_miss_count = 0;
  memset(caches, 0, sizeof(Cache_Line) * cache_line_num);
//  lirs = new LIRS[cache_set_size];
  for(int i = 0; i < 2050; i++) {
    lirs[i] = LIRS();
  }
}

CacheSim::~CacheSim() {
  free(caches);
}

int CacheSim::cache_check_hit(_u64 set_base, _u64 addr) {
  /**循环查找当前set的所有way（line），通过tag匹配，查看当前地址是否在cache中*/
  _u64 i;
  for (i = 0; i < cache_mapping_ways; ++i) {
        if ((caches[set_base + i].flag & CACHE_FLAG_VALID) &&
            (caches[set_base + i].tag == ((addr >> (cache_set_shifts + cache_line_shifts))))) {
      return i; //返回line在set内的偏移地址
    }
  }
  return -1;
}

/**获取当前set中INVALID的line*/
int CacheSim::cache_get_free_line(_u64 set_base) {
  for (_u64 i = 0; i < cache_mapping_ways; ++i) {
    if (!(caches[set_base + i].flag & CACHE_FLAG_VALID)) {
      return i;
    }
  }
  return -1;
}

/**Normal Cache命中，更新Cache状态*/
void CacheSim::cache_hit(_u64 set_base, _u64 index, int a_swap_style) {
  switch (a_swap_style) {
    case CACHE_SWAP_LRU:
      for (_u64 j = 0; j < cache_mapping_ways; ++j) {
                        if ((caches[set_base + j].LRU < caches[set_base + index].LRU) && (caches[set_base + j].flag & CACHE_FLAG_VALID)) {
          caches[set_base + j].LRU++;
        }
      }
      caches[set_base + index].LRU = 0;
      break;
    case CACHE_SWAP_FRU:
      caches[set_base + index].FRU++;
      break;
    case CACHE_SWAP_BRRIP:
    case CACHE_SWAP_SRRIP:
      caches[set_base + index].RRPV = 0;
      break;
    case CACHE_SWAP_SRRIP_FP:
      if (caches[set_base + index].RRPV != 0) {
        caches[set_base + index].RRPV -= 1;
      }
      break;
    case CACHE_SWAP_MINE:
//      cout << (set_base>>5) << endl;
      lirs[(set_base >> 5)].hit(caches[set_base + index].tag);
      break;
  }
}

/**缺失，更新index指向的cache行状态*/
void CacheSim::cache_insert(_u64 set_base, _u64 index, int a_swap_style) {
  double rand_num;
  switch (a_swap_style) {
    case CACHE_SWAP_LRU:
      for (_u64 j = 0; j < cache_mapping_ways; ++j) {
                        if ((caches[set_base + j].LRU < caches[set_base + index].LRU) && (caches[set_base + j].flag & CACHE_FLAG_VALID)) {
          caches[set_base + j].LRU++;
        }
      }
      caches[set_base + index].LRU = 0;
      break;
    case CACHE_SWAP_FRU:
      caches[set_base + index].FRU = 1;
      break;
    case CACHE_SWAP_SRRIP_FP:
    case CACHE_SWAP_SRRIP:
      caches[set_base + index].RRPV = SRRIP_2_M_2;
      break;
    case CACHE_SWAP_BRRIP:
      rand_num = (double) rand() / RAND_MAX;
      caches[set_base + index].RRPV = (rand_num > EPSILON) ? SRRIP_2_M_1 : SRRIP_2_M_2;
      break;
    case CACHE_SWAP_MINE:
//      cout << (set_base>>5) << endl;
      lirs[set_base>>5].insert(caches[set_base + index].tag,index);
      break;
  }
}

/**获取当前set中可用的line，如果没有，就找到要被替换的块*/
int CacheSim::cache_find_victim(_u64 set_base, int a_swap_style, int hit_index) {
  //我得提醒，hit_index在这里的实现都还没出现过呢
  int free_index;//要替换的cache line
  _u64 min_FRU;
  _u64 max_LRU;

  /**从当前cache set里找可用的空闲line */
  free_index = cache_get_free_line(set_base);
  if (free_index >= 0) {
    return free_index;
  }

  /**没有可用line，则执行替换算法*/
    free_index = -1;
  switch (a_swap_style) {
    case CACHE_SWAP_RAND:
      free_index = rand() % cache_mapping_ways;
      break;
    case CACHE_SWAP_LRU:
      max_LRU = 0;
      for (_u64 j = 0; j < cache_mapping_ways; ++j) {
        if (caches[set_base + j].LRU > max_LRU) {
          max_LRU = caches[set_base + j].LRU;
          free_index = j;
        }
      }
      break;
    case CACHE_SWAP_FRU:
      min_FRU = get_ulonglong_max();
      for (_u64 j = 0; j < cache_mapping_ways; ++j) {
        if (caches[set_base + j].FRU < min_FRU) {
          min_FRU = caches[set_base + j].FRU;
          free_index = j;
        }
      }
      break;
    case CACHE_SWAP_SRRIP:
    case CACHE_SWAP_SRRIP_FP:
    case CACHE_SWAP_BRRIP:
      while (free_index < 0) {
        for (_u64 j = 0; j < cache_mapping_ways; ++j) {
          if (caches[set_base + j].RRPV == SRRIP_2_M_1) {
            free_index = j;
            break;
          }
        }
        // increment all RRPVs
        if (free_index < 0) {
          // increment all RRPVs
          for (_u64 j = 0; j < cache_mapping_ways; ++j) {
            caches[set_base + j].RRPV++;
          }
        } else {
          break;
        }
      }
      break;
    case CACHE_SWAP_MINE:
//        cout << (set_base>>5) << endl;
      free_index = lirs[set_base>>5].victim();
      break;
  }

  if (free_index >= 0) {
    //如果原有的cache line是脏数据，标记脏位
    if (caches[set_base + free_index].flag & CACHE_FLAG_DIRTY) {
      // TODO: 写回到L2 cache中。
      caches[set_base + free_index].flag &= ~CACHE_FLAG_DIRTY;
      cache_w_memory_count++;
    }
  } else {
    printf("I should not show\n");
  }
  return free_index;
}

/**
 * 返回使用哪一种RRIP，但和原算法还是不太一样，似乎没有complement？
 * @param set_base 这里变量名起错了，应该是set
 * @return 0的话，1的话，其它
 */
int CacheSim::get_set_flag(_u64 set_base) {
  // size >> 10 << 5 = size * 32 / 1024 ，参照论文中的sample比例
  int K = cache_set_size >> 5;  //也就是选1/32为dedicate
  int log2K = (int) log2(K);  //log2(N/K)是区号
  int log2N = (int) log2(cache_set_size);  //总的
  // 使用高位的几位，作为筛选.比如需要32 = 2^5个，则用最高的5位作为mask
  _u64 mask = pow_64(2, (_u64) (log2N - log2K)) - 1;  //可这不是最高位啊
  _u64 residual = set_base & mask;  //set_base怎么这么奇怪呢？它不是总的？
  return residual;  //5
}

double CacheSim::get_miss_rate() {
  return 100.0 * cache_miss_count / (cache_miss_count + cache_hit_count);
}

double CacheSim::get_hit_rate() {
  return 100.0 * cache_hit_count / (cache_miss_count + cache_hit_count);
}

void CacheSim::dump_cache_set_info(_u64 set_base) {
  int i;
  printf("Ways: ");
  for (i = 0; i < cache_mapping_ways; ++i) {
    printf("%6d", i);
  }
  printf("\nTags: ");
  for (i = 0; i < cache_mapping_ways; ++i) {
    printf("%6llx", caches[set_base + i].tag);
  }
  printf("\nValid:");
  for (i = 0; i < cache_mapping_ways; ++i) {
    if (caches[set_base + i].flag & CACHE_FLAG_VALID)
      printf("%6d", 1);
    else
      printf("%6d", 0);
  }
  printf("\nLRU:  ");
  for (i = 0; i < cache_mapping_ways; ++i) {
    printf("%6u", caches[set_base + i].LRU);
  }
  printf("\nFRU:  ");
  for (i = 0; i < cache_mapping_ways; ++i) {
    printf("%6u", caches[set_base + i].FRU);
  }
  printf("\nRRPV: ");
  for (i = 0; i < cache_mapping_ways; ++i) {
    printf("%6u", caches[set_base + i].RRPV);
  }
  printf("\n");
}

//static int op_times;
/**不需要分level*/
void CacheSim::do_cache_op(_u64 addr, char oper_style) { //这也就是trace的前2个参数
//  if(op_times<50000)cout << ++op_times << ' ' << << endl;
//  ,hit_rate:%f=100.0*cache_hit_count/(cache_hit_count + cache_miss_count)
  //仅仅是根据指令是读还是写
  _u64 set, set_base;
  int hit_index, free_index;
  tick_count++;
  if (oper_style == OPERATION_READ) cache_r_count++;
  if (oper_style == OPERATION_WRITE) cache_w_count++;

  //normal cache and sample cache has the same set number
  set = (addr >> cache_line_shifts) % cache_set_size;  //得到了组号
  set_base = set * cache_mapping_ways; //cache内set的偏移地址 0 8 16 ...
  hit_index = cache_check_hit(set_base, addr);//set内line的偏移地址 0-7

  int temp_swap_style = swap_style;
//  printf("line=%d,addr=%.8llx,set=%.8llx\n",tick_count,addr,set);
  int set_flag = get_set_flag(set); //返回当前set是否为sample set
  if (swap_style == CACHE_SWAP_DRRIP) {
    /**是否是sample set*/
    switch (set_flag) {
      case 0:
        temp_swap_style = CACHE_SWAP_BRRIP;
        break;
      case 1:
        temp_swap_style = CACHE_SWAP_SRRIP;
        break;
      default:
        if (PSEL > 0) {
          cur_win_replace_policy = CACHE_SWAP_BRRIP;
        } else {
          cur_win_replace_policy = CACHE_SWAP_SRRIP;
        }
        temp_swap_style = cur_win_replace_policy;
    }
  }

  /*
  if (set==0)
      dump_cache_set_info(set_base);
  */

  //是否写操作
  if (oper_style == OPERATION_WRITE) {
    if (hit_index >= 0) {
      //命中Cache
      cache_w_hit_count++;
      cache_hit_count++;
      if (write_through) {
        cache_w_memory_count++;
      } else {
        caches[set_base + hit_index].flag |= CACHE_FLAG_DIRTY;
      }
      //更新替换相关状态位
      cache_hit(set_base, hit_index, temp_swap_style);
    } else {
      cache_w_miss_count++;
      cache_miss_count++;

      if (write_allocation) {
        //写操作需要先从memory读到cache，然后再写
        cache_r_memory_count++;

        //找一个victim，从Cache中替换出
        free_index = cache_find_victim(set_base, temp_swap_style, hit_index);
        //写入新的cache line
        caches[set_base + free_index].tag = addr >> (cache_set_shifts + cache_line_shifts);
        caches[set_base + free_index].flag = (_u8) ~CACHE_FLAG_MASK;
        caches[set_base + free_index].flag |= CACHE_FLAG_VALID;
        if (write_through) {
          cache_w_memory_count++;
        } else {
          // 只是置脏位，不用立刻写回去，这样如果下次是write hit，就可以减少一次memory写操作
          caches[set_base + free_index].flag |= CACHE_FLAG_DIRTY;
        }

        //更新替换相关的状态位
        cache_insert(set_base, free_index, temp_swap_style);

        // 如果是动态策略，则还需要更新psel(psel>0, select BRRIP; PSEL<=0, select SRRIP)
        int set_flag = get_set_flag(set);
        if (swap_style == CACHE_SWAP_DRRIP) {
          if (set_flag == 1) { //if it is SRRIP now, psel++, tend to select BRRIP
            PSEL++;
          } else if (set_flag == 0) { //if it is BRRIP now, psel++, tend to select SRRIP
            PSEL--;
          }
        }
      } else {
        cache_w_memory_count++;  //与cache操作无关
      }
    }
  } else {
    if (hit_index >= 0) {
      //命中实际Cache
      cache_r_hit_count++;
      cache_hit_count++;

      //更新替换相关状态位
      cache_hit(set_base, hit_index, temp_swap_style);

    } else {
      cache_r_miss_count++;
      cache_r_memory_count++;
      cache_miss_count++;

      //找一个victim，从Cache中替换出
      free_index = cache_find_victim(set_base, temp_swap_style, hit_index);
      //写cache line
      caches[set_base + free_index].tag = addr >> (cache_set_shifts + cache_line_shifts);
      caches[set_base + free_index].flag = (_u8) ~CACHE_FLAG_MASK;
      caches[set_base + free_index].flag |= CACHE_FLAG_VALID;

      //更新替换相关的状态位
      cache_insert(set_base, free_index, temp_swap_style);

      // 如果是动态策略，则还需要更新psel(psel>0, select BRRIP; PSEL<=0, select SRRIP)
      int set_flag = get_set_flag(set);
      if (swap_style == CACHE_SWAP_DRRIP) {
        if (set_flag == 1) { //if it is SRRIP now, psel++, tend to select BRRIP
          PSEL++;
        } else if (set_flag == 0) { //if it is BRRIP now, psel++, tend to select SRRIP
          PSEL--;
        }
      }
    }
  }
}

/**从文件读取trace，在我最后的修改目标里，为了适配项目，这里需要改掉*/
void CacheSim::load_trace(const char *filename) {
  char buf[128];
  // 添加自己的input路径
  FILE *fin;
  // 记录的是trace中指令的读写，由于cache机制，和真正的读写次数可能不一样。主要是如果设置的写回法，则被写的脏cache line会等在cache中，直到被替换。
  _u64 rcount = 0, wcount = 0;

  /**读取cache line，做cache操作*/
  fin = fopen(filename, "r");
  if (!fin) {
    printf("load_trace %s failed\n", filename);
    return;
  }
  _u64 i = 0;

  while (fgets(buf, sizeof(buf), fin)) {
    char tmp_style[5];
    char style;

    _u64 addr = 0;
    int datalen = 0;
    int burst = 0;
    int mid = 0;
    float delay = 0;
    float ATIME = 0;
    int ch = 0;
    int qos = 0;

    sscanf(buf, "%s %llx %d %d %x %f %f %d %d", tmp_style, &addr, &datalen, &burst, &mid, &delay, &ATIME, &ch, &qos);
    if (strcmp(tmp_style, "nr") == 0 || strcmp(tmp_style, "wr") == 0) {
      style = 'l';
    } else if (strcmp(tmp_style, "nw") == 0 || strcmp(tmp_style, "naw") == 0) {
      style = 's';
    } else {
      printf("%s", tmp_style);
      return;
    }

    //访问cache
    do_cache_op(addr, style);

    switch (style) {
      case 'l' :
        rcount++;
        break;
      case 's' :
        wcount++;
        break;
      case 'k' :
        break;
      case 'u' :
        break;
    }
    i++;
  }

  printf("\n========================================================\n");
    printf("cache_size: %lld, cache_line_size:%lld, cache_set_size:%lld, mapping_ways:%lld, ", cache_size, cache_line_size, cache_set_size, cache_mapping_ways);
  char a_swap_style[99];
  switch (swap_style) {
    case CACHE_SWAP_RAND:
      strcpy(a_swap_style, "RAND");
      break;
    case CACHE_SWAP_LRU:
      strcpy(a_swap_style, "LRU");
      break;
    case CACHE_SWAP_FRU:
      strcpy(a_swap_style, "FRU");
      break;
    case CACHE_SWAP_SRRIP:
      strcpy(a_swap_style, "SRRIP");
      break;
    case CACHE_SWAP_SRRIP_FP:
      strcpy(a_swap_style, "SRRIP_FP");
      break;
    case CACHE_SWAP_BRRIP:
      strcpy(a_swap_style, "BRRIP");
      break;
    case CACHE_SWAP_DRRIP:
      strcpy(a_swap_style, "DRRIP");
      break;
    case CACHE_SWAP_MINE:
      strcpy(a_swap_style, "LIRS");
      break;
  }
  printf("cache_repl_policy:%s", a_swap_style);
  if (swap_style == CACHE_SWAP_SRRIP) {
    printf("\t%d", SRRIP_M);
  }
  printf("\n");

  // 文件中的指令统计
  printf("all r/w/sum: %lld %lld %lld \nall in cache r/w/sum: %lld %lld %lld \nread rate: %f%%\twrite rate: %f%%\nread rate in cache: %f%%\twrite rate in cache: %f%%\n",
         rcount, wcount, rcount + wcount,
         cache_r_count, cache_w_count, tick_count,
         100.0 * rcount / (rcount + wcount),
         100.0 * wcount / (rcount + wcount),
         100.0 * cache_r_count / tick_count,
         100.0 * cache_w_count / tick_count
  );
  printf("Cache hit/miss: %lld/%lld\t hit/miss rate: %f%%/%f%%\n",
         cache_hit_count, cache_miss_count,
         100.0 * cache_hit_count / (cache_hit_count + cache_miss_count),
         100.0 * cache_miss_count / (cache_hit_count + cache_miss_count));
    printf("read hit count in cache %lld\tmiss count in cache %lld\nwrite hit count in cache %lld\tmiss count in cache %lld\n", cache_r_hit_count, cache_r_miss_count, cache_w_hit_count, cache_w_miss_count);
  printf("Write through:\t%d\twrite allocation:\t%d\n", write_through, write_allocation);
  printf("Memory --> Cache:\t%.4fGB\nCache --> Memory:\t%.4fMB\ncache sum:\t%.4fGB\n",
         cache_r_memory_count * cache_line_size * 1.0 / 1024 / 1024 / 1024,
         cache_w_memory_count * cache_line_size * 1.0 / 1024 / 1024,
           (cache_r_memory_count * cache_line_size * 1.0 / 1024 / 1024 / 1024)+cache_w_memory_count * cache_line_size * 1.0 / 1024 / 1024/ 1024); //cache sum：经过cache操作的流量总和

  fclose(fin);
}


int main(const int argc, const char *argv[]) {

    //以下为固定的测试
//    ArgumentParser parser;
//
//    parser.addArgument("-i", "--input", 1, false);//trace的地址
//    parser.addArgument("--l1", 1, true);
//    parser.addArgument("--l2", 1, true);
//    parser.addArgument("--line_size", 1, true);
//    parser.addArgument("--ways", 1, true);
//    parser.parse(argc, argv);

    CacheSim cache;
    /**cache有关的参数*/
    _u64 line_size[] = {64}; //cache line 大小：64B
    //_u64 ways[] = {16,32}; //组相联的路数
    _u64 ways[] = {32}; //组相联的路数
    //_u64 cache_size[] = {0x400000,0x800000};//cache大小:4M, 8M
    _u64 cache_size[] = {0x400000};//cache大小:4M
    //cache_swap_style swap_style[] = {CACHE_SWAP_LRU};
    //cache_swap_style swap_style[] = {CACHE_SWAP_RAND, CACHE_SWAP_LRU, CACHE_SWAP_FRU, CACHE_SWAP_SRRIP, CACHE_SWAP_SRRIP_FP, CACHE_SWAP_BRRIP, CACHE_SWAP_DRRIP};
  cache_swap_style swap_style[] = {CACHE_SWAP_MINE};
  int ms[] = {3};
    int i, j, m, k, n;
    for (m = 0; m < sizeof(cache_size) / sizeof(_u64); m++) {
        for (i = 0; i < sizeof(line_size) / sizeof(_u64); i++) {
            for (j = 0; j < sizeof(ways) / sizeof(_u64); j++) {
                for (k = 0; k < sizeof(ms) / sizeof(int); k++) {
                   for (n = 0; n < sizeof(swap_style) / sizeof(cache_swap_style); n++) {
//                        printf("begin\n");
                        _u64 temp_cache_size, temp_line_size, temp_ways;
                        cache_swap_style temp_swap_style;

                        temp_cache_size = cache_size[m];
                        temp_line_size = line_size[i];
                        temp_ways = ways[j];
                        temp_swap_style = swap_style[n];
                        cache.init(temp_cache_size, temp_line_size, temp_ways, temp_swap_style);
                        cache.set_M(ms[k]);
                        cache.load_trace("/Users/quebec/Documents/Materials/大三下/系统/trace2/wangzherongyao60fps_b620_2.txt");
//                        printf("end\n");
                        cache.re_init();
//                        printf("end\n");
                    }
                }
            }
        }
    }
    return 0;
}
