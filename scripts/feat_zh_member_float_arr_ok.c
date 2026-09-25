// @EXPECTED exit:0
// @EXPECTED out:MEMFARR PASS
/* R5 dao3 sentinel (2026-09-25 Qiyuan, station 363): member FLOAT array read used 8-byte movsd.
 * Before fix (old compiler BFA74E62): B gf.a[0]=0.00, D lf.a[0]=0.00, E sf.a[0]=0.00,
 *   G gfm.a[0]=8.00 (key fingerprint: 0x3FC00000 as 1.5f spliced with 0x40200000 as 2.5f
 *   gives 0x402000003FC00000 whose double exponent 0x402 is exactly 8.0), H pf->a[0]=0.00.
 *   All DOUBLE member arrays (A/C/F) were already correct.
 * Root cause: the unified struct-array-field entry only set 码生成内存浮, never 码生成内存单,
 *   so the consumer took its else branch and emitted movsd for 4-byte floats.
 * Fix (+1 line): 码生成内存单 = (码生成内存浮 && 构域元素(构序, 函) == 4) ? 1 : 0;
 * Three gates: gcc values / new compiler 09BC6031 PASS / old compiler BFA74E62 must be RED.
 */
#include <stdio.h>

struct D1 { double a[1]; };
struct F1 { float  a[1]; };
struct DM { double a[2]; int tag; };
struct FM { float  a[2]; int tag; };

static struct D1 gd;
static struct F1 gf;
static struct DM gdm;
static struct FM gfm;

int main(void) {
    int bad = 0;
    struct D1 ld;
    struct F1 lf;
    static struct F1 sf;
    struct F1 *pf;

    gd.a[0] = 1.5;
    if (gd.a[0] != 1.5) bad |= 1;

    gf.a[0] = 1.5f;
    if ((double)gf.a[0] != 1.5) bad |= 2;

    ld.a[0] = 2.5;
    if (ld.a[0] != 2.5) bad |= 4;

    lf.a[0] = 2.5f;
    if ((double)lf.a[0] != 2.5) bad |= 8;

    sf.a[0] = 3.5f;
    if ((double)sf.a[0] != 3.5) bad |= 16;

    gdm.a[0] = 1.5;
    gdm.a[1] = 2.5;
    gdm.tag = 7;
    if (gdm.a[0] != 1.5) bad |= 32;
    if (gdm.a[1] != 2.5) bad |= 64;
    if (gdm.tag != 7) bad |= 128;

    gfm.a[0] = 1.5f;
    gfm.a[1] = 2.5f;
    gfm.tag = 9;
    if ((double)gfm.a[0] != 1.5) bad |= 256;
    if ((double)gfm.a[1] != 2.5) bad |= 512;
    if (gfm.tag != 9) bad |= 1024;

    pf = &lf;
    pf->a[0] = 4.5f;
    if ((double)pf->a[0] != 4.5) bad |= 2048;

    printf("A=%.2f\n", gd.a[0]);
    printf("B=%.2f\n", (double)gf.a[0]);
    printf("C=%.2f\n", ld.a[0]);
    printf("D=%.2f\n", (double)lf.a[0]);
    printf("E=%.2f\n", (double)sf.a[0]);
    printf("F=%.2f\n", gdm.a[0]);
    printf("F2=%.2f T=%d\n", gdm.a[1], gdm.tag);
    printf("G=%.2f\n", (double)gfm.a[0]);
    printf("G2=%.2f T=%d\n", (double)gfm.a[1], gfm.tag);
    printf("H=%.2f\n", (double)pf->a[0]);

    if (bad) {
        printf("MEMFARR FAIL bad=%d\n", bad);
        return 1;
    }
    printf("MEMFARR PASS\n");
    return 0;
}
