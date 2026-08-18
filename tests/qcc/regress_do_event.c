#include <stdio.h>

typedef int (*event_fn_t)(int type, size_t begin, size_t end,
			  void *cs, void *data);

struct opts_t {
	void *event_fn;
	void *event_fn_data;
};

struct edata_t {
	int previous_type;
	size_t previous_offset;
	struct opts_t *opts;
};

struct cs_t {
	int dummy;
	long (*do_ftell)(struct cs_t *c);
};

static int do_event(struct cs_t *cs, int type, struct edata_t *data)
{
	size_t offset;

	if (!data->opts || !data->opts->event_fn)
		return 0;

	if (type == 1 && data->previous_type == type)
		return 0;

	offset = cs->do_ftell(cs);
	if (type != 0)
		offset--;

	if (data->previous_type != 0 &&
	    ((event_fn_t)data->opts->event_fn)(data->previous_type,
					       data->previous_offset, offset,
					       cs, data->opts->event_fn_data) < 0)
		return -1;

	data->previous_type = type;
	data->previous_offset = offset;

	return 0;
}

static long my_ftell(struct cs_t *c) { (void)c; return 8; }

static int my_event(int type, size_t begin, size_t end, void *cs, void *data)
{
	(void)type; (void)begin; (void)end; (void)cs; (void)data;
	return 0;
}

int main(void)
{
	struct cs_t cs;
	struct opts_t opts;
	struct edata_t data;
	cs.do_ftell = my_ftell;
	opts.event_fn = (void*)my_event;
	opts.event_fn_data = 0;
	data.previous_type = 0;
	data.previous_offset = 0;
	data.opts = &opts;
	int r = do_event(&cs, 2, &data);
	printf("r=%d prev_type=%d prev_off=%zu\n", r, data.previous_type,
	       data.previous_offset);
	if (r != 0 || data.previous_type != 2 || data.previous_offset != 7) {
		printf("FAIL\n");
		return 1;
	}
	printf("OK\n");
	return 0;
}
