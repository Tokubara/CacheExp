//
// Created by find on 16-7-19.
// cache line = cache block = 原代码里的cache item ~= cache way
//
#ifndef CACHE_SIM
#define CACHE_SIM
typedef unsigned char _u8;
typedef unsigned int _u32;
typedef unsigned long long _u64;

const unsigned char CACHE_FLAG_VALID = 0x01;
const unsigned char CACHE_FLAG_DIRTY = 0x02;
const unsigned char CACHE_FLAG_LOCK = 0x04;
const unsigned char CACHE_FLAG_MASK = 0xff;
/**最多多少层cache*/
const int MAXLEVEL = 3;
const char OPERATION_READ = 'l';
const char OPERATION_WRITE = 's';
const char OPERATION_LOCK = 'k';
const char OPERATION_UNLOCK = 'u';


#include "map"

/** 替换算法*/
enum cache_swap_style {
    CACHE_SWAP_FIFO,
    CACHE_SWAP_LRU,
    CACHE_SWAP_RAND,
    CACHE_SWAP_MAX,
    CACHE_SWAP_SRRIP,
    CACHE_SWAP_SRRIP_FP,
    CACHE_SWAP_BRRIP,
    CACHE_SWAP_DRRIP
};


//写内存方法就默认写回吧。
class Cache_Line {
public:
    _u64 tag;
    /**计数，FIFO里记录最一开始的访问时间，LRU里记录上一次访问的时间*/
    _u64 count;
    _u8 flag;
    _u8 *buf;
    /** 相当于当前block是否要被evict的权重，使用于RRIP替换策略中 ，有可能-u8过小*/
    _u32 RRPV;
};

class ShareMemoryPage {
public:
    _u64 tag;
    union {
        _u64 count;
        _u64 lru_count;
    };

    ShareMemoryPage() {
        this->free = true;
        this->tag = 0;
        this->flag = 0;
        this->count = 0;
        this->lru_count = 0;

    }

    bool free;
    _u8 flag;
    _u8 *buf;

    bool isFree();

};

class Page {
public:
    _u64 tag;
    _u64 count;
    _u64 lru_count;

    Page() {
        this->tag = 0;
        this->count = 0;
        this->lru_count = 0;
    }
};


class CacheSim {
    // 隐患
public:
    std::map<int, std::map<int, _u64>> block_freq_each_time;
    _u64 SM_in;
    _u64 target_out;
    _u64 target_in;
    _u64 target;
    int page_size;
    std::map<_u64, int> miss_freequency;
    /**cache的总大小，单位byte*/
    ShareMemoryPage *ShareMemory;
    //sharememory


    _u64 cache_size[MAXLEVEL];
    /**cache line(Cache block)cache块的大小*/
    _u64 cache_line_size[MAXLEVEL];
    /**总的行数*/
    _u64 cache_line_num[MAXLEVEL];
    /**每个set有多少way*/
    _u64 cache_mapping_ways[MAXLEVEL];
    /**整个cache有多少组*/
    _u64 cache_set_size[MAXLEVEL];
    /**2的多少次方是set的数量，用于匹配地址时，进行位移比较*/
    _u64 cache_set_shifts[MAXLEVEL];
    /**2的多少次方是line的长度，用于匹配地址*/
    _u64 cache_line_shifts[MAXLEVEL];
    /**真正的cache地址列。指针数组*/
    Cache_Line *caches[MAXLEVEL];

    _u64 *lock_table;

    /**指令计数器*/
    _u64 tick_count;
    /**cache缓冲区,由于并没有数据*/
//    _u8 *cache_buf[MAXLEVEL];
    /**缓存替换算法*/
    int swap_style[MAXLEVEL];
    /**读写内存的指令计数*/
    _u64 cache_r_count, cache_w_count;
    /**实际写内存的计数，cache --> memory */
    _u64 cache_w_memory_count;
    /**cache hit和miss的计数*/
    _u64 cache_hit_count[MAXLEVEL], cache_miss_count[MAXLEVEL];
    _u64 SM_hit_count;
    /**空闲cache line的index记录，在寻找时，返回空闲line的index*/
    _u64 cache_free_num[MAXLEVEL];
    int num_of_share_memory_page;
    int free_SM_page;

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
    int cur_win_repalce_policy;
    CacheSim();

    ~CacheSim();

    void init(_u64 a_cache_size[], _u64 a_cache_line_size[], _u64 a_mapping_ways[]);

    /**原代码中addr的处理有些问题，导致我没有成功运行他的代码。
     * 检查是否命中
     * @args:
     * cache: 模拟的cache
     * set_base: 当前地址属于哪一个set，其基址是什么。
     * addr: 要判断的内存地址
     * @return:
     * 由于cache的地址肯定不会超过int（因为cache大小决定的）
     * TODO: check the addr */
    int check_cache_hit(_u64 set_base, _u64 addr, int level);

    /**获取cache当前set中空余的line*/
    int get_cache_free_line(_u64 set_base, int level);
    /**使用指定的swap策略获取cache当前set中空余的line*/
    int get_cache_free_line_specific(_u64 set_base, int level, int a_swap_style);
    /**找到合适的line之后，将数据写入cache line中*/
    void set_cache_line(_u64 index, _u64 addr, int level);

    /**对一个指令进行分析*/
    void do_cache_op(_u64 addr, char oper_style);

    /**读入trace文件*/
    void load_trace(const char *filename);

    /**lock a cache line*/
    int lock_cache_line(_u64 addr, int level);

    /**unlock a cache line*/
    int unlock_cache_line(_u64 addr, int level);

    /**@return 返回miss率*/
    double get_miss_rate(int level) {
        return 100.0 * cache_miss_count[level] / (cache_miss_count[level] + cache_hit_count[level]);
    }

    /** 计算int的次幂*/
    _u32 pow_int(int base, int expontent);
    _u64 pow_64(_u64 base, _u64 expontent);

    /**判断当前set是不是选中作为sample的set，并返回给当前set设置的policy*/
    int get_set_flag(_u64 set_base);
    int check_sm_hit(_u64 addr, int level);

    int get_free_sm_page();

    void set_sm_page(_u64 index, _u64 addr, int level);

    void swap_pages(Page *in_pages);

    void re_init();
};

#endif