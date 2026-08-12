/* 甲言内核 — tensor 模块 (0.2 模块化)
 * Tensor 池: 索引@RAM + 数据块@NVMe 磁盘 (版本链, 追加写).
 * bin 模式零全局 → 池基址/暂存/NVMe ctx 显式传递.
 */
#ifndef KERNEL_TENSOR_H
#define KERNEL_TENSOR_H

#include "srclib/kernel/nvme.h"

#define TENTRIES 64
#define TPOOL_BASE 0x2000000
#define DISK_BASE_LBA 0x200

/* 哈希 djb2 */
整 tp_hash(字 *key){
    整 h=5381;
    循环(*key){h=((h<<5)+h)+*key;key=key+1;}
    返 h&63;
}
/* 找 key 的最大/指定版本条目, 返索引 (-1 无) */
整 tp_find(整 tbl, 字 *key, 整 wantmax, 整 wantv){
    整 best=-1; 整 bestv=-1;
    整 i=0;
    循环(i<TENTRIES){
        整 fl=*(整*)(tbl+i*52+48);
        若(fl==1){
            字 *nm=(字*)(tbl+i*52);
            整 match=1; 整 j=0;
            循环(key[j]&&j<31){若(nm[j]!=key[j]){match=0;break;}j=j+1;}
            若(match&&nm[j]==0){
                整 v=*(整*)(tbl+i*52+40);
                若(wantmax){若(v>bestv){bestv=v;best=i;}}
                否则{若(v==wantv){返 i;}}
            }
        }
        i=i+1;
    }
    返 wantmax?best:-1;
}
/* 存: 新 key → v1, 已存在 → 追加版本. 返暂存RAM地址 (0=表满) */
整 tp_put(整 tbl, 字 *key, 整 size){
    整 idx=tp_hash(key);
    整 probe=0;
    循环(probe<TENTRIES){
        整 i=(idx+probe)&63;
        整 fl=*(整*)(tbl+i*52+48);
        若(fl==0){
            整 j=0; 循环(key[j]&&j<31){*(字节*)(tbl+i*52+j)=key[j];j=j+1;}
            *(字节*)(tbl+i*52+j)=0;
            整 v=1; 整 prev=-1;
            整 old=tp_find(tbl,key,1,0);
            若(old>=0){v=*(整*)(tbl+old*52+40)+1;prev=old;}
            *(整*)(tbl+i*52+32)=DISK_BASE_LBA+i*8;
            *(整*)(tbl+i*52+36)=size;
            *(整*)(tbl+i*52+40)=v;
            *(整*)(tbl+i*52+44)=prev;
            *(整*)(tbl+i*52+48)=1;
            返 TPOOL_BASE+TENTRIES*52+i*4096;
        }
        probe=probe+1;
    }
    返 0;
}
/* 提交: 暂存块 → 磁盘 */
整 tp_commit(NvmeCtx *nv, 整 tbl, 字 *key){
    整 i=tp_find(tbl,key,1,0);
    若(i<0)返 -1;
    整 lba=*(整*)(tbl+i*52+32);
    整 staging=TPOOL_BASE+TENTRIES*52+i*4096;
    返 nvme_iorw(nv,0x01,0x50+i,staging,lba,1);
}
/* 取最新版本: 磁盘 → out */
整 tp_get(NvmeCtx *nv, 整 tbl, 字 *key, 整 out){
    整 i=tp_find(tbl,key,1,0);
    若(i<0)返 0;
    整 lba=*(整*)(tbl+i*52+32);
    整 st=nvme_iorw(nv,0x02,0x60+i,out,lba,1);
    返 st==-1?0:out;
}
整 tp_version(整 tbl, 字 *key){
    整 i=tp_find(tbl,key,1,0);
    若(i>=0)返 *(整*)(tbl+i*52+40);
    返 0;
}
整 tp_del(整 tbl, 字 *key){
    整 i=0; 整 found=0;
    循环(i<TENTRIES){
        整 fl=*(整*)(tbl+i*52+48);
        若(fl==1){
            字 *nm=(字*)(tbl+i*52);
            整 match=1; 整 j=0;
            循环(key[j]&&j<31){若(nm[j]!=key[j]){match=0;break;}j=j+1;}
            若(match&&nm[j]==0){*(整*)(tbl+i*52+48)=0;found=1;}
        }
        i=i+1;
    }
    返 found?0:-1;
}
/* GC (2026-08-12 Gate5 收尾): 回收版本链超限的旧版本 — 每 key 保留最新 KEEPV 版.
   tp_del 已隐式回收 (fl=0 槽被 tp_put 重用). 两遍: 先统计每条目同 key 版本数,
   再回收 (单遍会因已回收条目使后续 cnt 变小 → 少回收, fix 2026-08-12). 返回回收数. */
整 tp_gc(整 tbl){
    整 KEEPV=4; 整 freed=0; 整 i=0;
    整 cnts[TENTRIES];
    循环(i<TENTRIES){cnts[i]=0;i=i+1;}
    /* 第一遍: 每条目统计同 key 版本数 */
    i=0;
    循环(i<TENTRIES){
        若(*(整*)(tbl+i*52+48)==1){
            字 *nm=(字*)(tbl+i*52);
            整 j=0;
            循环(j<TENTRIES){
                若(*(整*)(tbl+j*52+48)==1){
                    字 *nj=(字*)(tbl+j*52);
                    整 match=1; 整 k2=0;
                    循环(nm[k2]&&k2<31){若(nj[k2]!=nm[k2]){match=0;break;}k2=k2+1;}
                    若(match&&nj[k2]==0){cnts[i]=cnts[i]+1;}
                }
                j=j+1;
            }
        }
        i=i+1;
    }
    /* 第二遍: 回收超限旧版本 */
    i=0;
    循环(i<TENTRIES){
        若(*(整*)(tbl+i*52+48)==1){
            整 v=*(整*)(tbl+i*52+40);
            若(cnts[i]>KEEPV && v<(cnts[i]-KEEPV+1)){*(整*)(tbl+i*52+48)=0;freed=freed+1;}
        }
        i=i+1;
    }
    返 freed;
}

#endif /* KERNEL_TENSOR_H */
