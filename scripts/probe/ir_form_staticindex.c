/* 形态族：静态数组基下标读（`ga[i]`，元素宽 4）—— 态 12。
   与态 1/2 的差别：基址是**静态数组的首地址**（`lea` 绝对位移），不是帧址也不是槽里的指针。 */
static int sarr[4] = { 10, 20, 30, 40 };
static unsigned int uarr[4] = { 1u, 2u, 3u, 4u };

int main(void) {
    return sarr[1] + sarr[2] + (int)uarr[3];
}
