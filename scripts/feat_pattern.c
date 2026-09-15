// @EXPECTED out:PASS
#include <stdio.h>
#include <string.h>

/* C 模式匹配: switch + enum 判别 (tagged union 的判别字段) */
typedef enum { SHAPE_CIRCLE, SHAPE_RECT, SHAPE_TRI } ShapeKind;

typedef struct {
    ShapeKind kind;
    union {
        struct { double r; } circle;      /* 圆: 半径 */
        struct { double w, h; } rect;     /* 矩形: 宽高 */
        struct { double b, h; } tri;      /* 三角: 底高 */
    } u;
} Shape;

static const char *shape_name(ShapeKind k) {
    switch (k) {
    case SHAPE_CIRCLE: return "circle";
    case SHAPE_RECT: return "rect";
    case SHAPE_TRI: return "tri";
    default: return "?";
    }
}

static double shape_area(const Shape *s) {
    switch (s->kind) {
    case SHAPE_CIRCLE: return 3.14159 * s->u.circle.r * s->u.circle.r;
    case SHAPE_RECT: return s->u.rect.w * s->u.rect.h;
    case SHAPE_TRI: return 0.5 * s->u.tri.b * s->u.tri.h;
    }
    return 0;
}

int main(void) {
    Shape circle = { SHAPE_CIRCLE, { .circle = { 2.0 } } };
    Shape rect   = { SHAPE_RECT,   { .rect = { 3.0, 4.0 } } };
    Shape tri    = { SHAPE_TRI,    { .tri = { 4.0, 5.0 } } };

    printf("%s %.4f\n", shape_name(circle.kind), shape_area(&circle));
    printf("%s %.4f\n", shape_name(rect.kind), shape_area(&rect));
    printf("%s %.4f\n", shape_name(tri.kind), shape_area(&tri));

    if (shape_area(&circle) < 12.56 || shape_area(&circle) > 12.57) return 1;
    if (shape_area(&rect) != 12.0) return 2;
    if (shape_area(&tri) != 10.0) return 3;
    if (strcmp(shape_name(SHAPE_CIRCLE), "circle")) return 4;
    printf("PASS\n");
    return 0;
}
