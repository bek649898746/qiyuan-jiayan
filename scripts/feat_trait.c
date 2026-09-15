// @EXPECTED out:PASS
#include <stdio.h>

/* trait 静态分发: C 的函数指针表 (接口模拟, 无 RTTI 静态分发) */
typedef struct {
    double (*area)(const void *self);
    const char *(*name)(void);
} ShapeOps;

typedef struct { double r; } Circle;
typedef struct { double w, h; } Rect;

static double circle_area(const void *self) {
    const Circle *c = (const Circle *)self;
    return 3.14159 * c->r * c->r;
}
static const char *circle_name(void) { return "circle"; }

static double rect_area(const void *self) {
    const Rect *r = (const Rect *)self;
    return r->w * r->h;
}
static const char *rect_name(void) { return "rect"; }

static const ShapeOps circle_ops = { circle_area, circle_name };
static const ShapeOps rect_ops = { rect_area, rect_name };

static void describe(const ShapeOps *ops, const void *shape) {
    printf("%s area=%.4f\n", ops->name(), ops->area(shape));
}

int main(void) {
    Circle c = { 2.0 };
    Rect r = { 3.0, 4.0 };
    describe(&circle_ops, &c);
    describe(&rect_ops, &r);

    if (circle_ops.area(&c) < 12.56 || circle_ops.area(&c) > 12.57) return 1;
    if (rect_ops.area(&r) != 12.0) return 2;
    if (circle_ops.name() == rect_ops.name()) return 3;
    printf("PASS\n");
    return 0;
}
