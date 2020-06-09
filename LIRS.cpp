#include <cstdio>
#include <unordered_map>
#include <assert.h>
#include <list>
#include <iostream>
using namespace std;

typedef unsigned short _u16;
typedef unsigned char _u8;
//链表节点
struct Node {
    bool l; //是否是l状态，有可能是h
    _u8 index;
    _u16 tag;
};

/**
 * 为什么有cur_S_size？因为在L中的可能本来就未被分配，真实的S_size是应该分配的
 */

class LIRS {
public:
    /**
     * 这里暂时把size写死了，没办法，32路组相联，这多小，如果真按推荐的0.01的比例，那F根本就是0
     */
    LIRS() {
      //因为是32路组相连，和为32
      S_size_limit = 30;
      Q_size_limit = 2;
      cur_S_size = 0;
    }
    unordered_map<_u16, list<Node>::iterator> ms;
    unordered_map<_u16, list<Node>::iterator> mq;
    _u8 S_size_limit;
    _u8 Q_size_limit;
    _u8 cur_S_size;
    list<Node> S;
    list<Node> Q;
    /**
     * 栈剪枝，在最后一个元素是L的情况下不会出错
     */
    void prune_S() {
      assert(cur_S_size<=S_size_limit);
      assert(Q.size()<=Q_size_limit);
      //cur_S_size不变
      while(!S.empty() && !S.back().l) {
        ms.erase(S.back().tag);
        S.pop_back();
      }
    }
    /**
     * 把S的最后一个挪到Q的end
     */
    void drop_S_end() {
      assert(cur_S_size<=S_size_limit);
      assert(Q.size()<=Q_size_limit);
      if(S.empty()) return;  //TODO 有这种可能么？我感觉发生这种情况就挺奇怪的
      auto &end = S.back();
      end.l = false;
      ms.erase(end.tag);
//      cur_S_size--;  //?但是S的大小不是不变么？ 本来是会--，但是调用这个的时候，不会有增减
      Q.splice(Q.end(),S,--S.end());
      mq[end.tag]=--Q.end();
      prune_S();
    }
    void hit(_u16 tag) {
      assert(cur_S_size<=S_size_limit);
      assert(Q.size()<=Q_size_limit);
      //tag表示set_base+tag，也就是在cache数组中的下标
      auto it = ms.find(tag);
      list<Node>::iterator l_it;
      if(it!=ms.end()) {
        //在S中
        l_it = it->second;
        if(l_it->l) {
          //此操作不改变S的size
          S.splice(S.begin(),S,l_it);
          ms[tag]=S.begin();
          prune_S();  //这个可能在栈底，就可能发生栈剪枝，如果不在栈底，也不会发生任何事
        } else {
          //是HIR但是在S中
          l_it->l = true;
          S.splice(S.begin(),S,l_it);
          Q.erase(mq[tag]);  //也因此腾了一个空间
          //TODO 感觉if的else情况与在Q中不会同时发生，因此else也许是多余的
//          if(cur_S_size>=S_size_limit) {
            drop_S_end(); //这样cur_S_size不变
//          } else {
//            cur_S_size++;
//          }

          ms[tag]=S.begin();

          mq.erase(tag);
        }
      } else {
        //不在S中，必在H中
        //不涉及size的改变
        l_it = mq.find(tag)->second;
        S.push_front(*l_it);  //已经放在S栈顶上了
        ms[tag]=S.begin();
        Q.splice(Q.end(),Q,l_it);
        mq[tag]=(--Q.end());
      }
    }
    /**
     * 在需要更新指标的时候调用
     * @param tag
     */
    void insert(_u16 tag, _u8 index) {
      printf("cur_S_size=%d\n",cur_S_size);
      assert(cur_S_size<=S_size_limit); //调用这个函数的时候，一定victim已经被调用了，因为victim负责清除
      assert(Q.size()<Q_size_limit);
      auto it = ms.find(tag);
      list<Node>::iterator l_it;
//      if(cur_S_size<S_size_limit) {
//        node.l=true;
//        node.tag = tag;
//        S.push_front(node);
//        cur_S_size++;
//        return;
//      }
/*      if(Q.size()==Q_size_limit) {
        Q.
      }*/
//一定不在Q中，而且Q一定会走一个(除非是刚开始或者Q还没有满的情况)
      if(it!=ms.end()) {
        //如果在S中，复制hit中的if中的else，去掉与Q相关的
        l_it = it->second;
        l_it->l = true;
        S.splice(S.begin(),S,l_it);
//        if(cur_S_size>=S_size_limit) {
        //凡是在S中却不是l的，必然S已满，所以不需要判断
          drop_S_end();
//        } else {
//          cur_S_size++;
//        }
        ms[tag]=S.begin();
          //TODO 对Q做处理，目前是希望放在一起，这样Q没满的时候统一判断
      } else {
        Node node;
        node.tag = tag;
        node.index = index;
        //没对l处理是因为不确定S是否已满
        if(cur_S_size==S_size_limit) {
          node.l = false;
          Q.push_back(node);
          mq[tag] = (--Q.end());
//          S.push_front(node);
//          ms[tag] = S.begin();
        } else {
          node.l = true;
          S.push_front(node);
          cur_S_size++;
        }
        ms[tag] = S.begin();
      }
    }
    /**
     * 返回victim的，也更改victim
     * @return front的index，并不是-set_base的，这里当然没有减去set_base
     */
    _u8 victim() {
      //无论如何，都得撵走队首，不过，它已经在cache中走了，但这里还得收拾，但是只有在size超了的时候才需要撵走
//      printf("Qsize:%lu\n",Q.size());
//      assert(Q.size() >= Q_size_limit);
      auto tmp = Q.front();
      Q.pop_front();
      mq.erase(tmp.tag);
      return tmp.index;  //不知道为啥这里之前写的index
    }
//    bool exist(_u16 index) {
//      return (mq.find(index)!=mq.end()||(ms.find(index)!=ms.end() && ms.find(index)->second->l));
//    }
};